#include "lua/plus.hpp"
#include "GameResource/AsyncResourceLoader.hpp"
#include "LuaBinding/Resource.hpp"
#include "LuaBinding/modern/Texture2D.hpp"
#include "AppFrame.h"

namespace luastg::binding
{
    // 现代 API 的异步纹理加载任务包装
    struct AsyncTexture2DTask
    {
        static constexpr std::string_view const ClassID{ "LuaSTG.Sub.AsyncTexture2DTask" };
        
        std::shared_ptr<ResourceLoadingTask> task;
        bool textures_cached = false;  // 标记是否已缓存 Lua 对象
        int textures_ref = LUA_NOREF;  // 缓存的纹理数组引用
        
        static int api_getProgress(lua_State* L)
        {
            lua::stack_t S(L);
            auto* self = cast(L, 1);
            if (!self->task)
            {
                return luaL_error(L, "Invalid async task");
            }
            
            S.push_value(static_cast<lua_Integer>(self->task->GetCompletedCount()));
            S.push_value(static_cast<lua_Integer>(self->task->GetTotalCount()));
            return 2;
        }
        
        static int api_isCompleted(lua_State* L)
        {
            lua::stack_t S(L);
            auto* self = cast(L, 1);
            if (!self->task)
            {
                return luaL_error(L, "Invalid async task");
            }
            
            S.push_value(self->task->IsCompleted());
            return 1;
        }
        
        static int api_wait(lua_State* L)
        {
            auto* self = cast(L, 1);
            if (!self->task)
            {
                return luaL_error(L, "Invalid async task");
            }
            
            while (!self->task->IsCompleted() && !self->task->IsCancelled())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            return 0;
        }
        
        static int api_cancel(lua_State* L)
        {
            auto* self = cast(L, 1);
            if (!self->task)
            {
                return luaL_error(L, "Invalid async task");
            }
            
            self->task->Cancel();
            return 0;
        }
        
        static int api_getTextures(lua_State* L)
        {
            lua::stack_t S(L);
            auto* self = cast(L, 1);
            if (!self->task)
            {
                return luaL_error(L, "Invalid async task");
            }
            
            // 如果已经缓存了 Lua 对象，直接返回
            if (self->textures_cached && self->textures_ref != LUA_NOREF)
            {
                lua_rawgeti(L, LUA_REGISTRYINDEX, self->textures_ref);
                return 1;
            }
            
            auto results = self->task->GetResults();
            
            // 创建纹理对象数组
            lua_createtable(L, static_cast<int>(results.size()), 0);
            
            for (size_t i = 0; i < results.size(); ++i)
            {
                const auto& result = results[i];
                
                if (result.success && result.texture)
                {
                    auto* tex_obj = Texture2D::create(L);
                    tex_obj->data = result.texture.get();
                    if (tex_obj->data)
                    {
                        tex_obj->data->retain();
                    }
                }
                else
                {
                    lua_pushnil(L);
                }
                
                lua_rawseti(L, -2, static_cast<int>(i + 1));
            }
            
            // 缓存这个数组到 registry，避免重复创建
            lua_pushvalue(L, -1);  // 复制数组到栈顶
            self->textures_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            self->textures_cached = true;
            
            return 1;
        }
        
        static int api___gc(lua_State* L)
        {
            auto* self = cast(L, 1);
            
            // 释放缓存的 Lua 对象引用
            if (self->textures_ref != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, self->textures_ref);
                self->textures_ref = LUA_NOREF;
            }
            
            self->task.reset();
            return 0;
        }
        
        static int api___tostring(lua_State* L)
        {
            lua::stack_t S(L);
            auto* self = cast(L, 1);
            if (self->task)
            {
                char buffer[128];
                snprintf(buffer, sizeof(buffer), "AsyncTexture2DTask(%zu/%zu)", 
                         self->task->GetCompletedCount(),
                         self->task->GetTotalCount());
                S.push_value(buffer);
            }
            else
            {
                S.push_value<std::string_view>(ClassID);
            }
            return 1;
        }
        
        static AsyncTexture2DTask* create(lua_State* L, std::shared_ptr<ResourceLoadingTask> task)
        {
            lua::stack_t S(L);
            
            auto* self = S.create_userdata<AsyncTexture2DTask>();
            auto const self_index = S.index_of_top();
            S.set_metatable(self_index, ClassID);
            
            self->task = std::move(task);
            return self;
        }
        
        static AsyncTexture2DTask* cast(lua_State* L, int idx)
        {
            return static_cast<AsyncTexture2DTask*>(luaL_checkudata(L, idx, ClassID.data()));
        }
        
        static void registerClass(lua_State* L)
        {
            [[maybe_unused]] lua::stack_balancer_t SB(L);
            lua::stack_t S(L);
            
            // method
            auto const method_table = S.create_map();
            S.set_map_value(method_table, "getProgress", &api_getProgress);
            S.set_map_value(method_table, "isCompleted", &api_isCompleted);
            S.set_map_value(method_table, "wait", &api_wait);
            S.set_map_value(method_table, "cancel", &api_cancel);
            S.set_map_value(method_table, "getTextures", &api_getTextures);
            
            // metatable
            auto const metatable = S.create_metatable(ClassID);
            S.set_map_value(metatable, "__gc", &api___gc);
            S.set_map_value(metatable, "__tostring", &api___tostring);
            S.set_map_value(metatable, "__index", method_table);
        }
    };
    
    // 现代 API 的异步纹理加载
    struct AsyncTexture2D
    {
        // 异步加载多个纹理
        static int loadAsync(lua_State* L)
        {
            lua::stack_t S(L);
            
            if (!lua_istable(L, 1))
            {
                return luaL_error(L, "Expected table of file paths");
            }
            
            std::vector<ResourceLoadRequest> requests;
            size_t count = lua_objlen(L, 1);
            requests.reserve(count);
            
            bool mipmaps = true;
            if (lua_gettop(L) >= 2 && lua_isboolean(L, 2))
            {
                mipmaps = lua_toboolean(L, 2) != 0;
            }
            
            for (size_t i = 1; i <= count; ++i)
            {
                lua_rawgeti(L, 1, static_cast<int>(i));
                
                if (lua_isstring(L, -1))
                {
                    ResourceLoadRequest request;
                    request.type = ResourceType::Texture;
                    request.name.clear(); // 现代 API 不使用名称
                    
                    TextureLoadParams params;
                    params.path = lua_tostring(L, -1);
                    params.mipmaps = mipmaps;
                    request.params = params;
                    
                    requests.push_back(std::move(request));
                }
                
                lua_pop(L, 1);
            }
            
            if (requests.empty())
            {
                return luaL_error(L, "No valid file paths provided");
            }
            
            // 提交任务（不使用资源池）
            auto* loader = LAPP.GetAsyncResourceLoader();
            if (!loader)
            {
                return luaL_error(L, "AsyncResourceLoader not initialized");
            }
            
            auto task = loader->SubmitTask(std::move(requests), false); // false = 不使用资源池
            if (!task)
            {
                return luaL_error(L, "Failed to submit loading task");
            }
            
            AsyncTexture2DTask::create(L, task);
            return 1;
        }
        
        static void registerFunctions(lua_State* L)
        {
            [[maybe_unused]] lua::stack_balancer_t SB(L);
            lua::stack_t S(L);
            
            // 注册 AsyncTexture2DTask 类
            AsyncTexture2DTask::registerClass(L);
            
            // 获取或创建 lstg.Texture2D 表
            lua_getglobal(L, "lstg");
            if (!lua_istable(L, -1))
            {
                lua_pop(L, 1);
                lua_newtable(L);
                lua_setglobal(L, "lstg");
                lua_getglobal(L, "lstg");
            }
            
            lua_getfield(L, -1, "Texture2D");
            if (!lua_istable(L, -1))
            {
                lua_pop(L, 1);
                lua_newtable(L);
                lua_setfield(L, -2, "Texture2D");
                lua_getfield(L, -1, "Texture2D");
            }
            
            // 添加 loadAsync 函数
            lua_pushcfunction(L, &loadAsync);
            lua_setfield(L, -2, "loadAsync");
            
            lua_pop(L, 2); // pop Texture2D and lstg
        }
    };
}

namespace luastg::binding
{
    void RegisterAsyncTexture2D(lua_State* L)
    {
        AsyncTexture2D::registerFunctions(L);
    }
}