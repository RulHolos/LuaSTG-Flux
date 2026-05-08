#include "lua/plus.hpp"
#include "GameResource/AsyncResourceLoader.hpp"
#include "LuaBinding/Resource.hpp"
#include "LuaBinding/modern/Sprite.hpp"
#include "LuaBinding/modern/Texture2D.hpp"
#include "AppFrame.h"

namespace luastg::binding
{
    // 现代 API 的异步精灵加载任务包装
    struct AsyncSpriteTask
    {
        static constexpr std::string_view const ClassID{ "LuaSTG.Flux.AsyncSpriteTask" };
        
        std::shared_ptr<ResourceLoadingTask> task;
        bool sprites_cached = false;  // 标记是否已缓存 Lua 对象
        int sprites_ref = LUA_NOREF;  // 缓存的精灵数组引用
        
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
        
        static int api_getSprites(lua_State* L)
        {
            lua::stack_t S(L);
            auto* self = cast(L, 1);
            if (!self->task)
            {
                return luaL_error(L, "Invalid async task");
            }
            
            // 如果已经缓存了 Lua 对象，直接返回
            if (self->sprites_cached && self->sprites_ref != LUA_NOREF)
            {
                lua_rawgeti(L, LUA_REGISTRYINDEX, self->sprites_ref);
                return 1;
            }
            
            auto results = self->task->GetResults();
            
            // 创建精灵对象数组
            lua_createtable(L, static_cast<int>(results.size()), 0);
            
            for (size_t i = 0; i < results.size(); ++i)
            {
                const auto& result = results[i];
                
                if (result.success && result.sprite)
                {
                    auto* sprite_obj = Sprite::create(L);
                    sprite_obj->data = result.sprite.get();
                    if (sprite_obj->data)
                    {
                        sprite_obj->data->retain();
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
            self->sprites_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            self->sprites_cached = true;
            
            return 1;
        }
        
        static int api___gc(lua_State* L)
        {
            auto* self = cast(L, 1);
            
            // 释放缓存的 Lua 对象引用
            if (self->sprites_ref != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, self->sprites_ref);
                self->sprites_ref = LUA_NOREF;
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
                snprintf(buffer, sizeof(buffer), "AsyncSpriteTask(%zu/%zu)", 
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
        
        static AsyncSpriteTask* create(lua_State* L, std::shared_ptr<ResourceLoadingTask> task)
        {
            lua::stack_t S(L);
            
            auto* self = S.create_userdata<AsyncSpriteTask>();
            auto const self_index = S.index_of_top();
            S.set_metatable(self_index, ClassID);
            
            self->task = std::move(task);
            return self;
        }
        
        static AsyncSpriteTask* cast(lua_State* L, int idx)
        {
            return static_cast<AsyncSpriteTask*>(luaL_checkudata(L, idx, ClassID.data()));
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
            S.set_map_value(method_table, "getSprites", &api_getSprites);
            
            // metatable
            auto const metatable = S.create_metatable(ClassID);
            S.set_map_value(metatable, "__gc", &api___gc);
            S.set_map_value(metatable, "__tostring", &api___tostring);
            S.set_map_value(metatable, "__index", method_table);
        }
    };
    
    // 现代 API 的异步精灵加载
    struct AsyncSprite
    {
        // 异步加载多个精灵
        // 参数：资源列表表，可选的默认参数表
        static int loadAsync(lua_State* L)
        {
            lua::stack_t S(L);
            
            if (!lua_istable(L, 1))
            {
                return luaL_error(L, "Expected table of sprite definitions");
            }
            
            std::vector<ResourceLoadRequest> requests;
            size_t count = lua_objlen(L, 1);
            requests.reserve(count);
            
            // 读取默认参数（如果有）
            SpriteLoadParams default_params;
            core::Graphics::ITexture2D* default_texture_obj = nullptr;
            
            if (lua_gettop(L) >= 2 && lua_istable(L, 2))
            {
                // texture (默认纹理，可以是字符串或 Texture2D 对象)
                lua_getfield(L, 2, "texture");
                if (lua_isstring(L, -1))
                {
                    default_params.texture_name = lua_tostring(L, -1);
                }
                else if (Texture2D::is(L, -1))
                {
                    auto* tex = Texture2D::as(L, -1);
                    default_texture_obj = tex->data;
                }
                lua_pop(L, 1);
                
                // 读取其他默认参数
                lua_getfield(L, 2, "rect");
                if (lua_isboolean(L, -1))
                {
                    default_params.is_rect = lua_toboolean(L, -1) != 0;
                }
                lua_pop(L, 1);
                
                lua_getfield(L, 2, "anchor_x");
                if (lua_isnumber(L, -1)) default_params.anchor_x = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, 2, "anchor_y");
                if (lua_isnumber(L, -1)) default_params.anchor_y = lua_tonumber(L, -1);
                lua_pop(L, 1);
            }
            
            for (size_t i = 1; i <= count; ++i)
            {
                lua_rawgeti(L, 1, static_cast<int>(i));
                
                if (lua_istable(L, -1))
                {
                    SpriteLoadParams params = default_params;
                    core::Graphics::ITexture2D* texture_obj = default_texture_obj;
                    
                    // texture (可选，如果有默认值)
                    lua_getfield(L, -1, "texture");
                    if (lua_isstring(L, -1))
                    {
                        params.texture_name = lua_tostring(L, -1);
                        texture_obj = nullptr;
                    }
                    else if (Texture2D::is(L, -1))
                    {
                        auto* tex = Texture2D::as(L, -1);
                        texture_obj = tex->data;
                        params.texture_name.clear();
                    }
                    else if (!lua_isnil(L, -1))
                    {
                        lua_pop(L, 2);
                        continue; // 无效的纹理类型
                    }
                    lua_pop(L, 1);
                    
                    // 如果既没有字符串也没有对象，检查是否有默认值
                    if (params.texture_name.empty() && !texture_obj)
                    {
                        lua_pop(L, 1);
                        continue; // 跳过没有纹理的项
                    }
                    
                    // x, y, w, h (必需)
                    lua_getfield(L, -1, "x");
                    if (lua_isnumber(L, -1)) params.x = lua_tonumber(L, -1);
                    lua_pop(L, 1);
                    
                    lua_getfield(L, -1, "y");
                    if (lua_isnumber(L, -1)) params.y = lua_tonumber(L, -1);
                    lua_pop(L, 1);
                    
                    lua_getfield(L, -1, "w");
                    if (lua_isnumber(L, -1)) params.w = lua_tonumber(L, -1);
                    lua_pop(L, 1);
                    
                    lua_getfield(L, -1, "h");
                    if (lua_isnumber(L, -1)) params.h = lua_tonumber(L, -1);
                    lua_pop(L, 1);
                    
                    // anchor_x, anchor_y (可选，覆盖默认值)
                    lua_getfield(L, -1, "anchor_x");
                    if (lua_isnumber(L, -1)) params.anchor_x = lua_tonumber(L, -1);
                    lua_pop(L, 1);
                    
                    lua_getfield(L, -1, "anchor_y");
                    if (lua_isnumber(L, -1)) params.anchor_y = lua_tonumber(L, -1);
                    lua_pop(L, 1);
                    
                    // rect (可选，覆盖默认值)
                    lua_getfield(L, -1, "rect");
                    if (lua_isboolean(L, -1))
                    {
                        params.is_rect = lua_toboolean(L, -1) != 0;
                    }
                    lua_pop(L, 1);
                    
                    // 如果使用 Texture2D 对象，存储指针供主线程使用
                    if (texture_obj)
                    {
                        params.texture_object = texture_obj;
                        params.texture_name.clear();
                    }
                    
                    ResourceLoadRequest request;
                    request.type = ResourceType::Sprite;
                    request.name.clear();
                    request.params = params;
                    
                    requests.push_back(std::move(request));
                }
                
                lua_pop(L, 1);
            }
            
            if (requests.empty())
            {
                return luaL_error(L, "No valid sprite definitions provided");
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
            
            AsyncSpriteTask::create(L, task);
            return 1;
        }
        
        static void registerFunctions(lua_State* L)
        {
            [[maybe_unused]] lua::stack_balancer_t SB(L);
            lua::stack_t S(L);
            
            // 注册 AsyncSpriteTask 类
            AsyncSpriteTask::registerClass(L);
            
            // 获取或创建 lstg.Sprite 表
            lua_getglobal(L, "lstg");
            if (!lua_istable(L, -1))
            {
                lua_pop(L, 1);
                lua_newtable(L);
                lua_setglobal(L, "lstg");
                lua_getglobal(L, "lstg");
            }
            
            lua_getfield(L, -1, "Sprite");
            if (!lua_istable(L, -1))
            {
                lua_pop(L, 1);
                lua_newtable(L);
                lua_setfield(L, -2, "Sprite");
                lua_getfield(L, -1, "Sprite");
            }
            
            // 添加 loadAsync 函数
            lua_pushcfunction(L, &loadAsync);
            lua_setfield(L, -2, "loadAsync");
            
            lua_pop(L, 2); // pop Sprite and lstg
        }
    };
}

namespace luastg::binding
{
    void RegisterAsyncSprite(lua_State* L)
    {
        AsyncSprite::registerFunctions(L);
    }
}