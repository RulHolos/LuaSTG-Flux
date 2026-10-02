#include "LuaBinding/modern/Ipc.hpp"
#include "Ipc/IpcManager.hpp"
#include "lua/plus.hpp"

namespace luastg::binding
{

    std::string_view const Ipc::class_name{"lstg.IPC"};

    namespace
    {
        constexpr char const *kFunctionsRegistryKey = "lstg.IPC.functions";

        constexpr char const *kErrorInvalidJson = "invalid_json";
        constexpr char const *kErrorInvalidRequest = "invalid_request";
        constexpr char const *kErrorMissingFunction = "missing_function";
        constexpr char const *kErrorFunctionNotFound = "function_not_found";
        constexpr char const *kErrorLuaError = "lua_error";
        constexpr char const *kErrorInternalEncode = "internal_encode_error";

        void pushFunctionsTable(lua_State *const L)
        {
            lua_getfield(L, LUA_REGISTRYINDEX, kFunctionsRegistryKey);
            if (lua_isnil(L, -1))
            {
                lua_pop(L, 1);
                lua_newtable(L);
                lua_pushvalue(L, -1);
                lua_setfield(L, LUA_REGISTRYINDEX, kFunctionsRegistryKey);
            }
        }

        void pushCjsonFunction(lua_State *const L, char const *const name)
        {
            lua_getglobal(L, "cjson");
            lua_getfield(L, -1, name);
            lua_remove(L, -2);
        }

        bool decodeMessage(lua_State *const L, std::string const &message)
        {
            pushCjsonFunction(L, "decode");
            lua_pushlstring(L, message.data(), message.size());
            return lua_pcall(L, 1, 1, 0) == 0;
        }

        std::string encodeMessage(lua_State *const L, int const table_index)
        {
            pushCjsonFunction(L, "encode");
            lua_pushvalue(L, table_index);
            if (lua_pcall(L, 1, 1, 0) != 0)
            {
                lua_pop(L, 1);
                return R"({"ok":false,"code":")" + std::string(kErrorInternalEncode) + R"(","error":"internal encode failure"})";
            }
            size_t len = 0;
            char const *const str = lua_tolstring(L, -1, &len);
            std::string result(str, len);
            lua_pop(L, 1);
            return result;
        }

        void respond(lua_State *const L, uint64_t const connection_id, int const response_table_index)
        {
            auto const response = encodeMessage(L, response_table_index);
            IpcManager::GetInstance().sendResponse(connection_id, response);
        }

        void respondError(lua_State *const L, uint64_t const connection_id, int const id_index, char const *const code, std::string_view const error)
        {
            lua_newtable(L);
            int const table_index = lua_gettop(L);
            if (id_index != 0)
            {
                lua_pushvalue(L, id_index);
                lua_setfield(L, table_index, "id");
            }
            lua_pushboolean(L, false);
            lua_setfield(L, table_index, "ok");
            lua_pushstring(L, code);
            lua_setfield(L, table_index, "code");
            lua_pushlstring(L, error.data(), error.size());
            lua_setfield(L, table_index, "error");
            respond(L, connection_id, table_index);
            lua_pop(L, 1);
        }

        void dispatchOne(lua_State *const L, IpcRequest const &request)
        {
            lua::stack_balancer_t const sb(L);

            if (!decodeMessage(L, request.message))
            {
                char const *const err = lua_tostring(L, -1);
                std::string const message = err != nullptr ? err : "invalid json";
                lua_pop(L, 1);
                respondError(L, request.connection_id, 0, kErrorInvalidJson, message);
                return;
            }
            int const decoded_index = lua_gettop(L);

            if (!lua_istable(L, decoded_index))
            {
                respondError(L, request.connection_id, 0, kErrorInvalidRequest, "request must be a json object");
                return;
            }

            lua_getfield(L, decoded_index, "id");
            int const id_index = lua_gettop(L);

            lua_getfield(L, decoded_index, "function");
            if (!lua_isstring(L, -1))
            {
                lua_pop(L, 1);
                respondError(L, request.connection_id, id_index, kErrorMissingFunction, "missing 'function' field");
                return;
            }
            std::string const function_name = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, decoded_index, "args");
            int const args_index = lua_gettop(L);
            bool const has_args = lua_istable(L, args_index);
            size_t const arg_count = has_args ? lua_objlen(L, args_index) : 0;

            pushFunctionsTable(L);
            lua_getfield(L, -1, function_name.c_str());
            lua_remove(L, -2);

            if (!lua_isfunction(L, -1))
            {
                lua_pop(L, 1);
                respondError(L, request.connection_id, id_index, kErrorFunctionNotFound, "function not registered: " + function_name);
                return;
            }

            for (size_t i = 1; i <= arg_count; ++i)
            {
                lua_rawgeti(L, args_index, static_cast<int>(i));
            }

            if (lua_pcall(L, static_cast<int>(arg_count), LUA_MULTRET, 0) != 0)
            {
                char const *const err = lua_tostring(L, -1);
                std::string const message = err != nullptr ? err : "unknown error";
                lua_pop(L, 1);
                respondError(L, request.connection_id, id_index, kErrorLuaError, message);
                return;
            }

            int const result_top = lua_gettop(L);
            int const return_count = result_top - args_index;
            lua_newtable(L);
            for (int i = 0; i < return_count; ++i)
            {
                lua_pushvalue(L, args_index + 1 + i);
                lua_rawseti(L, -2, i + 1);
            }
            int const results_index = lua_gettop(L);

            lua_newtable(L);
            int const response_index = lua_gettop(L);
            lua_pushvalue(L, id_index);
            lua_setfield(L, response_index, "id");
            lua_pushboolean(L, true);
            lua_setfield(L, response_index, "ok");
            lua_pushvalue(L, results_index);
            lua_setfield(L, response_index, "result");
            respond(L, request.connection_id, response_index);
        }
    }

    void Ipc::update(lua_State *const vm)
    {
        if (!IpcManager::GetInstance().isRunning())
        {
            return;
        }
        IpcRequest request;
        while (IpcManager::GetInstance().popRequest(&request))
        {
            dispatchOne(vm, request);
        }
    }

    struct IpcBinding : Ipc
    {
        static int start(lua_State *const L)
        {
            char const *const name = luaL_checkstring(L, 1);
            bool const ok = IpcManager::GetInstance().start(name);
            lua_pushboolean(L, ok);
            return 1;
        }
        static int stop(lua_State *const)
        {
            IpcManager::GetInstance().stop();
            return 0;
        }
        static int isRunning(lua_State *const L)
        {
            lua_pushboolean(L, IpcManager::GetInstance().isRunning());
            return 1;
        }
        static int registerFunction(lua_State *const L)
        {
            luaL_checkstring(L, 1);
            luaL_checktype(L, 2, LUA_TFUNCTION);
            pushFunctionsTable(L);
            lua_pushvalue(L, 1);
            lua_pushvalue(L, 2);
            lua_settable(L, -3);
            lua_pop(L, 1);
            return 0;
        }
        static int unregisterFunction(lua_State *const L)
        {
            char const *const name = luaL_checkstring(L, 1);
            pushFunctionsTable(L);
            lua_pushnil(L);
            lua_setfield(L, -2, name);
            lua_pop(L, 1);
            return 0;
        }
    };

    void Ipc::registerClass(lua_State *const vm)
    {
        [[maybe_unused]] lua::stack_balancer_t sb(vm);
        lua::stack_t ctx(vm);

        auto const method_table = ctx.create_module(class_name);
        ctx.set_map_value(method_table, "start", &IpcBinding::start);
        ctx.set_map_value(method_table, "stop", &IpcBinding::stop);
        ctx.set_map_value(method_table, "isRunning", &IpcBinding::isRunning);
        ctx.set_map_value(method_table, "register", &IpcBinding::registerFunction);
        ctx.set_map_value(method_table, "unregister", &IpcBinding::unregisterFunction);
    }
}
