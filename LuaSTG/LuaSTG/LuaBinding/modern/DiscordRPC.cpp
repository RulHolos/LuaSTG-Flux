#include "DiscordRPC.hpp"
#include "DiscordRPC/DiscordRPCManager.hpp"
#include "lua/plus.hpp"
#include "AppFrame.h"

namespace luastg::binding {

	std::string_view const DiscordRPC::class_name{ "lstg.DiscordRPC" };

	struct DiscordRPCBinding : DiscordRPC {
		static int Initialize(lua_State* L) {
            try {
                const char* app_id = luaL_checkstring(L, 1);
                bool ok = ::luastg::DiscordRPCManager::GetInstance().Initialize(app_id);
                lua_pushboolean(L, ok);
                return 1;
            } catch (const std::exception& e) {
                return luaL_error(L, "Discord.Initialize failed: %s", e.what());
            }
        }

        static int SetPresence(lua_State* L) {
            try {
                const char* state = luaL_checkstring(L, 1);
                const char* details = luaL_checkstring(L, 2);
                const char* large_image_key = luaL_optstring(L, 3, nullptr);
                const char* large_image_text = luaL_optstring(L, 4, nullptr);
                const char* small_image_key = luaL_optstring(L, 5, nullptr);
                const char* small_image_text = luaL_optstring(L, 6, nullptr);
                int64_t start_timestamp = static_cast<int64_t>(luaL_optnumber(L, 7, 0));

                bool ok = ::luastg::DiscordRPCManager::GetInstance().SetPresence(
                    state, details, large_image_key, large_image_text,
                    small_image_key, small_image_text, start_timestamp
                );
                lua_pushboolean(L, ok);
                return 1;
            } catch (const std::exception& e) {
                return luaL_error(L, "Discord.SetPresence failed: %s", e.what());
            }
        }

        static int ClearPresence(lua_State* L) {
            try {
                ::luastg::DiscordRPCManager::GetInstance().ClearPresence();
                return 0;
            } catch (const std::exception& e) {
                return luaL_error(L, "Discord.ClearPresence failed: %s", e.what());
            }
        }

        static int Shutdown(lua_State* L) {
            try {
                ::luastg::DiscordRPCManager::GetInstance().Shutdown();
                return 0;
            } catch (const std::exception& e) {
                return luaL_error(L, "Discord.Shutdown failed: %s", e.what());
            }
        }

        static int IsInitialized(lua_State* L) {
            try {
                bool ok = ::luastg::DiscordRPCManager::GetInstance().IsInitialized();
                lua_pushboolean(L, ok);
                return 1;
            } catch (const std::exception& e) {
                return luaL_error(L, "Discord.IsInitialized failed: %s", e.what());
            }
        }
	};

	void DiscordRPC::registerClass(lua_State* vm) {
		[[maybe_unused]] lua::stack_balancer_t sb(vm);
		lua::stack_t ctx(vm);

		// method

		auto const method_table = ctx.create_module(class_name);
		ctx.set_map_value(method_table, "Initialize", &DiscordRPCBinding::Initialize);
		ctx.set_map_value(method_table, "SetPresence", &DiscordRPCBinding::SetPresence);
		ctx.set_map_value(method_table, "ClearPresence", &DiscordRPCBinding::ClearPresence);
		ctx.set_map_value(method_table, "Shutdown", &DiscordRPCBinding::Shutdown);
		ctx.set_map_value(method_table, "IsInitialized", &DiscordRPCBinding::IsInitialized);
	}
}
