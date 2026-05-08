#include "LuaBinding/LuaWrapper.hpp"
#include "lua/plus.hpp"
#include "GameResource/AsyncResourceLoader.hpp"
#include "AppFrame.h"

namespace luastg::binding
{
    // LoadingTask Lua 包装
    struct LuaLoadingTask
    {
        static constexpr std::string_view const ClassID{ "LuaSTG.Sub.LoadingTask" };
        
        std::shared_ptr<ResourceLoadingTask> task;
        
        static int api_getId(lua_State* L)
        {
            lua::stack_t S(L);
            auto* self = cast(L, 1);
            if (!self->task)
            {
                return luaL_error(L, "Invalid loading task");
            }
            S.push_value(static_cast<lua_Integer>(self->task->GetId()));
            return 1;
        }
        
        static int api_getProgress(lua_State* L)
        {
            lua::stack_t S(L);
            auto* self = cast(L, 1);
            if (!self->task)
            {
                return luaL_error(L, "Invalid loading task");
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
                return luaL_error(L, "Invalid loading task");
            }
            
            S.push_value(self->task->IsCompleted());
            return 1;
        }
        
        static int api_isCancelled(lua_State* L)
        {
            lua::stack_t S(L);
            auto* self = cast(L, 1);
            if (!self->task)
            {
                return luaL_error(L, "Invalid loading task");
            }
            
            S.push_value(self->task->IsCancelled());
            return 1;
        }
        
        static int api_getStatus(lua_State* L)
        {
            lua::stack_t S(L);
            auto* self = cast(L, 1);
            if (!self->task)
            {
                return luaL_error(L, "Invalid loading task");
            }
            
            auto status = self->task->GetStatus();
            std::string_view status_str = "unknown";
            
            switch (status)
            {
            case LoadingTaskStatus::Pending:
                status_str = "pending";
                break;
            case LoadingTaskStatus::Loading:
                status_str = "loading";
                break;
            case LoadingTaskStatus::Completed:
                status_str = "completed";
                break;
            case LoadingTaskStatus::Failed:
                status_str = "failed";
                break;
            case LoadingTaskStatus::Cancelled:
                status_str = "cancelled";
                break;
            }
            
            S.push_value(status_str);
            return 1;
        }
        
        static int api_cancel(lua_State* L)
        {
            auto* self = cast(L, 1);
            if (!self->task)
            {
                return luaL_error(L, "Invalid loading task");
            }
            
            self->task->Cancel();
            return 0;
        }
        
        static int api_wait(lua_State* L)
        {
            auto* self = cast(L, 1);
            if (!self->task)
            {
                return luaL_error(L, "Invalid loading task");
            }
            
            // 简单的轮询等待
            while (!self->task->IsCompleted() && !self->task->IsCancelled())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            return 0;
        }
        
        static int api_getResults(lua_State* L)
        {
            lua::stack_t S(L);
            auto* self = cast(L, 1);
            if (!self->task)
            {
                return luaL_error(L, "Invalid loading task");
            }
            
            auto results = self->task->GetResults();
            
            // 创建结果表
            lua_createtable(L, static_cast<int>(results.size()), 0);
            
            for (size_t i = 0; i < results.size(); ++i)
            {
                const auto& result = results[i];
                
                // 创建单个结果表
                lua_createtable(L, 0, 4);
                
                // name
                lua_pushstring(L, "name");
                lua_pushstring(L, result.name.c_str());
                lua_settable(L, -3);
                
                // success
                lua_pushstring(L, "success");
                lua_pushboolean(L, result.success);
                lua_settable(L, -3);
                
                // error (如果有)
                if (!result.success && !result.error_message.empty())
                {
                    lua_pushstring(L, "error");
                    lua_pushstring(L, result.error_message.c_str());
                    lua_settable(L, -3);
                }
                
                // type
                lua_pushstring(L, "type");
                lua_pushinteger(L, static_cast<lua_Integer>(result.type));
                lua_settable(L, -3);
                
                // 设置到结果数组
                lua_rawseti(L, -2, static_cast<int>(i + 1));
            }
            
            return 1;
        }
        
        static int api___gc(lua_State* L)
        {
            auto* self = cast(L, 1);
            // 显式调用析构函数
            self->~LuaLoadingTask();
            return 0;
        }
        
        static int api___tostring(lua_State* L)
        {
            lua::stack_t S(L);
            auto* self = cast(L, 1);
            if (self->task)
            {
                char buffer[128];
                snprintf(buffer, sizeof(buffer), "LoadingTask(id=%llu, %zu/%zu)", 
                         static_cast<unsigned long long>(self->task->GetId()),
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
        
        static LuaLoadingTask* create(lua_State* L, const std::shared_ptr<ResourceLoadingTask>& task)
        {
            lua::stack_t S(L);
            
            auto* self = S.create_userdata<LuaLoadingTask>();
            
            new (self) LuaLoadingTask();
            
            auto const self_index = S.index_of_top();
            S.set_metatable(self_index, ClassID);
            
            self->task = task;
            return self;
        }
        
        static LuaLoadingTask* cast(lua_State* L, int idx)
        {
            return static_cast<LuaLoadingTask*>(luaL_checkudata(L, idx, ClassID.data()));
        }
        
        static void registerClass(lua_State* L)
        {
            [[maybe_unused]] lua::stack_balancer_t SB(L);
            lua::stack_t S(L);
            
            // method
            auto const method_table = S.create_map();
            S.set_map_value(method_table, "getId", &api_getId);
            S.set_map_value(method_table, "getProgress", &api_getProgress);
            S.set_map_value(method_table, "isCompleted", &api_isCompleted);
            S.set_map_value(method_table, "isCancelled", &api_isCancelled);
            S.set_map_value(method_table, "getStatus", &api_getStatus);
            S.set_map_value(method_table, "cancel", &api_cancel);
            S.set_map_value(method_table, "wait", &api_wait);
            S.set_map_value(method_table, "getResults", &api_getResults);
            
            // metatable
            auto const metatable = S.create_metatable(ClassID);
            S.set_map_value(metatable, "__gc", &api___gc);
            S.set_map_value(metatable, "__tostring", &api___tostring);
            S.set_map_value(metatable, "__index", method_table);
        }
    };
    
    // 异步资源管理器 Lua API
    struct AsyncResourceManager
    {
        // 从 Lua 表中读取参数到 request
        static void ReadRequestParams(lua_State* L, int table_index, ResourceLoadRequest& request)
        {
            lua_getfield(L, table_index, "name");
            if (lua_isstring(L, -1))
            {
                request.name = lua_tostring(L, -1);
            }
            lua_pop(L, 1);
            
            lua_getfield(L, table_index, "pool");
            if (lua_isstring(L, -1))
            {
                const char* pool_name = lua_tostring(L, -1);
                if (strcmp(pool_name, "global") == 0)
                {
                    request.target_pool = LRES.GetResourcePool(ResourcePoolType::Global);
                }
                else if (strcmp(pool_name, "stage") == 0)
                {
                    request.target_pool = LRES.GetResourcePool(ResourcePoolType::Stage);
                }
            }
            lua_pop(L, 1);
            
            switch (request.type)
            {
            case ResourceType::Texture:
            {
                TextureLoadParams params;
                if (std::holds_alternative<TextureLoadParams>(request.params))
                {
                    params = std::get<TextureLoadParams>(request.params);
                }
                
                lua_getfield(L, table_index, "path");
                if (lua_isstring(L, -1))
                {
                    params.path = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "mipmaps");
                if (lua_isboolean(L, -1))
                {
                    params.mipmaps = lua_toboolean(L, -1) != 0;
                }
                else if (lua_isnumber(L, -1))
                {
                    params.mipmaps = lua_tonumber(L, -1) != 0;
                }
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "width");
                if (lua_isnumber(L, -1))
                {
                    params.width = static_cast<int>(lua_tonumber(L, -1));
                }
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "height");
                if (lua_isnumber(L, -1))
                {
                    params.height = static_cast<int>(lua_tonumber(L, -1));
                }
                lua_pop(L, 1);
                
                request.params = params;
                break;
            }
            
            case ResourceType::Sprite:
            {
                SpriteLoadParams params;
                if (std::holds_alternative<SpriteLoadParams>(request.params))
                {
                    params = std::get<SpriteLoadParams>(request.params);
                }
                
                lua_getfield(L, table_index, "texture");
                if (lua_isstring(L, -1))
                {
                    params.texture_name = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "x");
                if (lua_isnumber(L, -1)) params.x = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "y");
                if (lua_isnumber(L, -1)) params.y = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "w");
                if (lua_isnumber(L, -1)) params.w = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "h");
                if (lua_isnumber(L, -1)) params.h = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "anchor_x");
                if (lua_isnumber(L, -1)) params.anchor_x = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "anchor_y");
                if (lua_isnumber(L, -1)) params.anchor_y = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "rect");
                if (lua_isboolean(L, -1))
                {
                    params.is_rect = lua_toboolean(L, -1) != 0;
                }
                lua_pop(L, 1);
                
                request.params = params;
                break;
            }
            
            case ResourceType::Animation:
            {
                AnimationLoadParams params;
                if (std::holds_alternative<AnimationLoadParams>(request.params))
                {
                    params = std::get<AnimationLoadParams>(request.params);
                }
                
                lua_getfield(L, table_index, "texture");
                if (lua_isstring(L, -1))
                {
                    params.texture_name = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "x");
                if (lua_isnumber(L, -1)) params.x = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "y");
                if (lua_isnumber(L, -1)) params.y = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "w");
                if (lua_isnumber(L, -1)) params.w = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "h");
                if (lua_isnumber(L, -1)) params.h = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "n");
                if (lua_isnumber(L, -1)) params.n = static_cast<int>(lua_tonumber(L, -1));
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "m");
                if (lua_isnumber(L, -1)) params.m = static_cast<int>(lua_tonumber(L, -1));
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "interval");
                if (lua_isnumber(L, -1)) params.interval = static_cast<int>(lua_tonumber(L, -1));
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "anchor_x");
                if (lua_isnumber(L, -1)) params.anchor_x = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "anchor_y");
                if (lua_isnumber(L, -1)) params.anchor_y = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "rect");
                if (lua_isboolean(L, -1))
                {
                    params.is_rect = lua_toboolean(L, -1) != 0;
                }
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "sprites");
                if (lua_istable(L, -1))
                {
                    size_t sprite_count = lua_objlen(L, -1);
                    for (size_t j = 1; j <= sprite_count; ++j)
                    {
                        lua_rawgeti(L, -1, static_cast<int>(j));
                        if (lua_isstring(L, -1))
                        {
                            params.sprite_names.push_back(lua_tostring(L, -1));
                        }
                        lua_pop(L, 1);
                    }
                }
                lua_pop(L, 1);
                
                request.params = params;
                break;
            }
            
            case ResourceType::Music:
            {
                MusicLoadParams params;
                if (std::holds_alternative<MusicLoadParams>(request.params))
                {
                    params = std::get<MusicLoadParams>(request.params);
                }
                
                lua_getfield(L, table_index, "path");
                if (lua_isstring(L, -1))
                {
                    params.path = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "loop_start");
                if (lua_isnumber(L, -1)) params.loop_start = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "loop_end");
                if (lua_isnumber(L, -1)) params.loop_end = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "once_decode");
                if (lua_isboolean(L, -1))
                {
                    params.once_decode = lua_toboolean(L, -1) != 0;
                }
                lua_pop(L, 1);
                
                request.params = params;
                break;
            }
            
            case ResourceType::SoundEffect:
            {
                SoundEffectLoadParams params;
                if (std::holds_alternative<SoundEffectLoadParams>(request.params))
                {
                    params = std::get<SoundEffectLoadParams>(request.params);
                }
                
                lua_getfield(L, table_index, "path");
                if (lua_isstring(L, -1))
                {
                    params.path = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                
                request.params = params;
                break;
            }
            
            case ResourceType::SpriteFont:
            {
                SpriteFontLoadParams params;
                if (std::holds_alternative<SpriteFontLoadParams>(request.params))
                {
                    params = std::get<SpriteFontLoadParams>(request.params);
                }
                
                lua_getfield(L, table_index, "path");
                if (lua_isstring(L, -1))
                {
                    params.path = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "tex_path");
                if (lua_isstring(L, -1))
                {
                    params.font_tex_path = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "mipmaps");
                if (lua_isboolean(L, -1))
                {
                    params.mipmaps = lua_toboolean(L, -1) != 0;
                }
                else if (lua_isnumber(L, -1))
                {
                    params.mipmaps = lua_tonumber(L, -1) != 0;
                }
                lua_pop(L, 1);
                
                request.params = params;
                break;
            }
            
            case ResourceType::TrueTypeFont:
            {
                TrueTypeFontLoadParams params;
                if (std::holds_alternative<TrueTypeFontLoadParams>(request.params))
                {
                    params = std::get<TrueTypeFontLoadParams>(request.params);
                }
                
                lua_getfield(L, table_index, "path");
                if (lua_isstring(L, -1))
                {
                    params.path = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "width");
                if (lua_isnumber(L, -1)) params.font_width = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "height");
                if (lua_isnumber(L, -1)) params.font_height = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);
                
                request.params = params;
                break;
            }
            
            case ResourceType::FX:
            {
                FXLoadParams params;
                if (std::holds_alternative<FXLoadParams>(request.params))
                {
                    params = std::get<FXLoadParams>(request.params);
                }
                
                lua_getfield(L, table_index, "path");
                if (lua_isstring(L, -1))
                {
                    params.path = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                
                request.params = params;
                break;
            }
            
            case ResourceType::Model:
            {
                ModelLoadParams params;
                if (std::holds_alternative<ModelLoadParams>(request.params))
                {
                    params = std::get<ModelLoadParams>(request.params);
                }
                
                lua_getfield(L, table_index, "path");
                if (lua_isstring(L, -1))
                {
                    params.path = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                
                request.params = params;
                break;
            }
            
            case ResourceType::Particle:
            {
                ParticleLoadParams params;
                if (std::holds_alternative<ParticleLoadParams>(request.params))
                {
                    params = std::get<ParticleLoadParams>(request.params);
                }
                
                lua_getfield(L, table_index, "path");
                if (lua_isstring(L, -1))
                {
                    params.path = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "img_name");
                if (lua_isstring(L, -1))
                {
                    params.particle_img_name = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "anchor_x");
                if (lua_isnumber(L, -1)) params.anchor_x = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "anchor_y");
                if (lua_isnumber(L, -1)) params.anchor_y = lua_tonumber(L, -1);
                lua_pop(L, 1);
                
                lua_getfield(L, table_index, "rect");
                if (lua_isboolean(L, -1))
                {
                    params.is_rect = lua_toboolean(L, -1) != 0;
                }
                lua_pop(L, 1);
                
                request.params = params;
                break;
            }
            
            case ResourceType::Video:
            {
                VideoLoadParams params;
                if (std::holds_alternative<VideoLoadParams>(request.params))
                {
                    params = std::get<VideoLoadParams>(request.params);
                }
                
                lua_getfield(L, table_index, "path");
                if (lua_isstring(L, -1))
                {
                    params.path = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
                
                request.params = params;
                break;
            }
            
            default:
                break;
            }
        }
        
        // 解析加载请求表，支持默认参数和覆盖
        // table_index: 资源列表表的索引
        // defaults_index: 默认参数表的索引（0 表示没有）
        // default_type: 默认资源类型
        static std::vector<ResourceLoadRequest> ParseLoadRequests(lua_State* L, int table_index, int defaults_index, ResourceType default_type)
        {
            std::vector<ResourceLoadRequest> requests;
            
            if (!lua_istable(L, table_index))
            {
                luaL_error(L, "Expected table of load requests");
                return requests;
            }
            
            ResourceLoadRequest default_request;
            default_request.type = default_type;
            
            if (defaults_index != 0 && lua_istable(L, defaults_index))
            {
                ReadRequestParams(L, defaults_index, default_request);
            }
            
            size_t count = lua_objlen(L, table_index);
            requests.reserve(count);
            
            for (size_t i = 1; i <= count; ++i)
            {
                lua_rawgeti(L, table_index, static_cast<int>(i));
                
                if (!lua_istable(L, -1))
                {
                    lua_pop(L, 1);
                    continue;
                }
                
                ResourceLoadRequest request = default_request;
                ReadRequestParams(L, -1, request);
                
                requests.push_back(std::move(request));
                lua_pop(L, 1);
            }
            
            return requests;
        }
        
        static ResourcePool* GetPoolFromLua(lua_State* L, int index)
        {
            if (lua_gettop(L) < index || lua_isnil(L, index))
            {
                return nullptr;
            }
            
            if (lua_isstring(L, index))
            {
                const char* pool_name = lua_tostring(L, index);
                if (strcmp(pool_name, "global") == 0)
                {
                    return LRES.GetResourcePool(ResourcePoolType::Global);
                }
                else if (strcmp(pool_name, "stage") == 0)
                {
                    return LRES.GetResourcePool(ResourcePoolType::Stage);
                }
            }
            
            return nullptr;
        }
        
        static int LoadTextureAsync(lua_State* L) noexcept
        {
            try
            {
                int defaults_index = (lua_gettop(L) >= 2 && lua_istable(L, 2)) ? 2 : 0;
                auto requests = ParseLoadRequests(L, 1, defaults_index, ResourceType::Texture);
                
                if (requests.empty())
                {
                    return luaL_error(L, "No valid load requests");
                }
                
                auto* loader = LAPP.GetAsyncResourceLoader();
                if (!loader)
                {
                    return luaL_error(L, "AsyncResourceLoader not initialized");
                }
                
                auto* target_pool = GetPoolFromLua(L, 3);
                auto task = loader->SubmitTask(std::move(requests), true, target_pool);
                if (!task)
                {
                    return luaL_error(L, "Failed to submit loading task");
                }
                
                LuaLoadingTask::create(L, task);
                return 1;
            }
            catch (const std::exception& e)
            {
                return luaL_error(L, "LoadTextureAsync failed: %s", e.what());
            }
        }
        
        static int LoadSpriteAsync(lua_State* L) noexcept
        {
            try
            {
                int defaults_index = (lua_gettop(L) >= 2 && lua_istable(L, 2)) ? 2 : 0;
                auto requests = ParseLoadRequests(L, 1, defaults_index, ResourceType::Sprite);
                
                if (requests.empty())
                {
                    return luaL_error(L, "No valid load requests");
                }
                
                auto* loader = LAPP.GetAsyncResourceLoader();
                if (!loader)
                {
                    return luaL_error(L, "AsyncResourceLoader not initialized");
                }
                
                auto* target_pool = GetPoolFromLua(L, 3);
                auto task = loader->SubmitTask(std::move(requests), true, target_pool);
                if (!task)
                {
                    return luaL_error(L, "Failed to submit loading task");
                }
                
                LuaLoadingTask::create(L, task);
                return 1;
            }
            catch (const std::exception& e)
            {
                return luaL_error(L, "LoadSpriteAsync failed: %s", e.what());
            }
        }
        
        static int LoadAnimationAsync(lua_State* L) noexcept
        {
            try
            {
                int defaults_index = (lua_gettop(L) >= 2 && lua_istable(L, 2)) ? 2 : 0;
                auto requests = ParseLoadRequests(L, 1, defaults_index, ResourceType::Animation);
                
                if (requests.empty())
                {
                    return luaL_error(L, "No valid load requests");
                }
                
                auto* loader = LAPP.GetAsyncResourceLoader();
                if (!loader)
                {
                    return luaL_error(L, "AsyncResourceLoader not initialized");
                }
                
                auto* target_pool = GetPoolFromLua(L, 3);
                auto task = loader->SubmitTask(std::move(requests), true, target_pool);
                if (!task)
                {
                    return luaL_error(L, "Failed to submit loading task");
                }
                
                LuaLoadingTask::create(L, task);
                return 1;
            }
            catch (const std::exception& e)
            {
                return luaL_error(L, "LoadAnimationAsync failed: %s", e.what());
            }
        }
        
        static int LoadMusicAsync(lua_State* L) noexcept
        {
            try
            {
                int defaults_index = (lua_gettop(L) >= 2 && lua_istable(L, 2)) ? 2 : 0;
                auto requests = ParseLoadRequests(L, 1, defaults_index, ResourceType::Music);
                
                if (requests.empty())
                {
                    return luaL_error(L, "No valid load requests");
                }
                
                auto* loader = LAPP.GetAsyncResourceLoader();
                if (!loader)
                {
                    return luaL_error(L, "AsyncResourceLoader not initialized");
                }
                
                auto* target_pool = GetPoolFromLua(L, 3);
                auto task = loader->SubmitTask(std::move(requests), true, target_pool);
                if (!task)
                {
                    return luaL_error(L, "Failed to submit loading task");
                }
                
                LuaLoadingTask::create(L, task);
                return 1;
            }
            catch (const std::exception& e)
            {
                return luaL_error(L, "LoadMusicAsync failed: %s", e.what());
            }
        }
        
        static int LoadSoundAsync(lua_State* L) noexcept
        {
            try
            {
                int defaults_index = (lua_gettop(L) >= 2 && lua_istable(L, 2)) ? 2 : 0;
                auto requests = ParseLoadRequests(L, 1, defaults_index, ResourceType::SoundEffect);
                
                if (requests.empty())
                {
                    return luaL_error(L, "No valid load requests");
                }
                
                auto* loader = LAPP.GetAsyncResourceLoader();
                if (!loader)
                {
                    return luaL_error(L, "AsyncResourceLoader not initialized");
                }
                
                auto* target_pool = GetPoolFromLua(L, 3);
                auto task = loader->SubmitTask(std::move(requests), true, target_pool);
                if (!task)
                {
                    return luaL_error(L, "Failed to submit loading task");
                }
                
                LuaLoadingTask::create(L, task);
                return 1;
            }
            catch (const std::exception& e)
            {
                return luaL_error(L, "LoadSoundAsync failed: %s", e.what());
            }
        }
        
        static int LoadFontAsync(lua_State* L) noexcept
        {
            try
            {
                int defaults_index = (lua_gettop(L) >= 2 && lua_istable(L, 2)) ? 2 : 0;
                auto requests = ParseLoadRequests(L, 1, defaults_index, ResourceType::TrueTypeFont);
                
                if (requests.empty())
                {
                    return luaL_error(L, "No valid load requests");
                }
                
                auto* loader = LAPP.GetAsyncResourceLoader();
                if (!loader)
                {
                    return luaL_error(L, "AsyncResourceLoader not initialized");
                }
                
                auto* target_pool = GetPoolFromLua(L, 3);
                auto task = loader->SubmitTask(std::move(requests), true, target_pool);
                if (!task)
                {
                    return luaL_error(L, "Failed to submit loading task");
                }
                
                LuaLoadingTask::create(L, task);
                return 1;
            }
            catch (const std::exception& e)
            {
                return luaL_error(L, "LoadFontAsync failed: %s", e.what());
            }
        }
        
        static int LoadSpriteFontAsync(lua_State* L) noexcept
        {
            try
            {
                int defaults_index = (lua_gettop(L) >= 2 && lua_istable(L, 2)) ? 2 : 0;
                auto requests = ParseLoadRequests(L, 1, defaults_index, ResourceType::SpriteFont);
                
                if (requests.empty())
                {
                    return luaL_error(L, "No valid load requests");
                }
                
                auto* loader = LAPP.GetAsyncResourceLoader();
                if (!loader)
                {
                    return luaL_error(L, "AsyncResourceLoader not initialized");
                }
                
                auto* target_pool = GetPoolFromLua(L, 3);
                auto task = loader->SubmitTask(std::move(requests), true, target_pool);
                if (!task)
                {
                    return luaL_error(L, "Failed to submit loading task");
                }
                
                LuaLoadingTask::create(L, task);
                return 1;
            }
            catch (const std::exception& e)
            {
                return luaL_error(L, "LoadSpriteFontAsync failed: %s", e.what());
            }
        }
        
        static int LoadFXAsync(lua_State* L) noexcept
        {
            try
            {
                int defaults_index = (lua_gettop(L) >= 2 && lua_istable(L, 2)) ? 2 : 0;
                auto requests = ParseLoadRequests(L, 1, defaults_index, ResourceType::FX);
                
                if (requests.empty())
                {
                    return luaL_error(L, "No valid load requests");
                }
                
                auto* loader = LAPP.GetAsyncResourceLoader();
                if (!loader)
                {
                    return luaL_error(L, "AsyncResourceLoader not initialized");
                }
                
                auto* target_pool = GetPoolFromLua(L, 3);
                auto task = loader->SubmitTask(std::move(requests), true, target_pool);
                if (!task)
                {
                    return luaL_error(L, "Failed to submit loading task");
                }
                
                LuaLoadingTask::create(L, task);
                return 1;
            }
            catch (const std::exception& e)
            {
                return luaL_error(L, "LoadFXAsync failed: %s", e.what());
            }
        }
        
        static int LoadModelAsync(lua_State* L) noexcept
        {
            try
            {
                int defaults_index = (lua_gettop(L) >= 2 && lua_istable(L, 2)) ? 2 : 0;
                auto requests = ParseLoadRequests(L, 1, defaults_index, ResourceType::Model);
                
                if (requests.empty())
                {
                    return luaL_error(L, "No valid load requests");
                }
                
                auto* loader = LAPP.GetAsyncResourceLoader();
                if (!loader)
                {
                    return luaL_error(L, "AsyncResourceLoader not initialized");
                }
                
                auto* target_pool = GetPoolFromLua(L, 3);
                auto task = loader->SubmitTask(std::move(requests), true, target_pool);
                if (!task)
                {
                    return luaL_error(L, "Failed to submit loading task");
                }
                
                LuaLoadingTask::create(L, task);
                return 1;
            }
            catch (const std::exception& e)
            {
                return luaL_error(L, "LoadModelAsync failed: %s", e.what());
            }
        }
        
        static int LoadParticleAsync(lua_State* L) noexcept
        {
            try
            {
                int defaults_index = (lua_gettop(L) >= 2 && lua_istable(L, 2)) ? 2 : 0;
                auto requests = ParseLoadRequests(L, 1, defaults_index, ResourceType::Particle);
                
                if (requests.empty())
                {
                    return luaL_error(L, "No valid load requests");
                }
                
                auto* loader = LAPP.GetAsyncResourceLoader();
                if (!loader)
                {
                    return luaL_error(L, "AsyncResourceLoader not initialized");
                }
                
                auto* target_pool = GetPoolFromLua(L, 3);
                auto task = loader->SubmitTask(std::move(requests), true, target_pool);
                if (!task)
                {
                    return luaL_error(L, "Failed to submit loading task");
                }
                
                LuaLoadingTask::create(L, task);
                return 1;
            }
            catch (const std::exception& e)
            {
                return luaL_error(L, "LoadParticleAsync failed: %s", e.what());
            }
        }
        
        static int LoadVideoAsync(lua_State* L) noexcept
        {
            try
            {
                int defaults_index = (lua_gettop(L) >= 2 && lua_istable(L, 2)) ? 2 : 0;
                auto requests = ParseLoadRequests(L, 1, defaults_index, ResourceType::Video);
                
                if (requests.empty())
                {
                    return luaL_error(L, "No valid load requests");
                }
                
                auto* loader = LAPP.GetAsyncResourceLoader();
                if (!loader)
                {
                    return luaL_error(L, "AsyncResourceLoader not initialized");
                }
                
                auto* target_pool = GetPoolFromLua(L, 3);
                auto task = loader->SubmitTask(std::move(requests), true, target_pool);
                if (!task)
                {
                    return luaL_error(L, "Failed to submit loading task");
                }
                
                LuaLoadingTask::create(L, task);
                return 1;
            }
            catch (const std::exception& e)
            {
                return luaL_error(L, "LoadVideoAsync failed: %s", e.what());
            }
        }
        
        static void Register(lua_State* L) noexcept
        {
            // 注册 LoadingTask 类
            LuaLoadingTask::registerClass(L);
            
            // 注册异步加载函数
            struct Wrapper
            {
                static int LoadTextureAsync(lua_State* L) noexcept
                {
                    return AsyncResourceManager::LoadTextureAsync(L);
                }
                static int LoadSpriteAsync(lua_State* L) noexcept
                {
                    return AsyncResourceManager::LoadSpriteAsync(L);
                }
                static int LoadAnimationAsync(lua_State* L) noexcept
                {
                    return AsyncResourceManager::LoadAnimationAsync(L);
                }
                static int LoadMusicAsync(lua_State* L) noexcept
                {
                    return AsyncResourceManager::LoadMusicAsync(L);
                }
                static int LoadSoundAsync(lua_State* L) noexcept
                {
                    return AsyncResourceManager::LoadSoundAsync(L);
                }
                static int LoadFontAsync(lua_State* L) noexcept
                {
                    return AsyncResourceManager::LoadFontAsync(L);
                }
                static int LoadSpriteFontAsync(lua_State* L) noexcept
                {
                    return AsyncResourceManager::LoadSpriteFontAsync(L);
                }
                static int LoadFXAsync(lua_State* L) noexcept
                {
                    return AsyncResourceManager::LoadFXAsync(L);
                }
                static int LoadModelAsync(lua_State* L) noexcept
                {
                    return AsyncResourceManager::LoadModelAsync(L);
                }
                static int LoadParticleAsync(lua_State* L) noexcept
                {
                    return AsyncResourceManager::LoadParticleAsync(L);
                }
                static int LoadVideoAsync(lua_State* L) noexcept
                {
                    return AsyncResourceManager::LoadVideoAsync(L);
                }
                static int GetAsyncLoaderThreadCount(lua_State* L) noexcept
                {
                    auto* loader = LAPP.GetAsyncResourceLoader();
                    if (!loader)
                    {
                        lua_pushinteger(L, 0);
                    }
                    else
                    {
                        lua_pushinteger(L, static_cast<lua_Integer>(loader->GetThreadCount()));
                    }
                    return 1;
                }
                static int SetAsyncLoaderMaxItemsPerFrame(lua_State* L) noexcept
                {
                    auto* loader = LAPP.GetAsyncResourceLoader();
                    if (!loader)
                    {
                        return luaL_error(L, "AsyncResourceLoader not initialized");
                    }
                    
                    lua_Integer count = luaL_checkinteger(L, 1);
                    if (count < 1)
                    {
                        return luaL_error(L, "MaxGPUItemsPerFrame must be at least 1");
                    }
                    
                    loader->SetMaxGPUItemsPerFrame(static_cast<size_t>(count));
                    return 0;
                }
                static int GetAsyncLoaderMaxItemsPerFrame(lua_State* L) noexcept
                {
                    auto* loader = LAPP.GetAsyncResourceLoader();
                    if (!loader)
                    {
                        lua_pushinteger(L, 0);
                    }
                    else
                    {
                        lua_pushinteger(L, static_cast<lua_Integer>(loader->GetMaxGPUItemsPerFrame()));
                    }
                    return 1;
                }
                static int ClearAsyncLoaderTasks(lua_State* L) noexcept
                {
                    auto* loader = LAPP.GetAsyncResourceLoader();
                    if (loader)
                    {
                        loader->ClearAllTasks();
                    }
                    return 0;
                }
            };
            
            luaL_Reg const lib[] = {
                { "LoadTextureAsync", &Wrapper::LoadTextureAsync },
                { "LoadSpriteAsync", &Wrapper::LoadSpriteAsync },
                { "LoadAnimationAsync", &Wrapper::LoadAnimationAsync },
                { "LoadMusicAsync", &Wrapper::LoadMusicAsync },
                { "LoadSoundAsync", &Wrapper::LoadSoundAsync },
                { "LoadFontAsync", &Wrapper::LoadFontAsync },
                { "LoadSpriteFontAsync", &Wrapper::LoadSpriteFontAsync },
                { "LoadFXAsync", &Wrapper::LoadFXAsync },
                { "LoadModelAsync", &Wrapper::LoadModelAsync },
                { "LoadParticleAsync", &Wrapper::LoadParticleAsync },
                { "LoadVideoAsync", &Wrapper::LoadVideoAsync },
                { "GetAsyncLoaderThreadCount", &Wrapper::GetAsyncLoaderThreadCount },
                { "SetAsyncLoaderMaxItemsPerFrame", &Wrapper::SetAsyncLoaderMaxItemsPerFrame },
                { "GetAsyncLoaderMaxItemsPerFrame", &Wrapper::GetAsyncLoaderMaxItemsPerFrame },
                { "ClearAsyncLoaderTasks", &Wrapper::ClearAsyncLoaderTasks },
                { nullptr, nullptr }
            };
            
            luaL_register(L, "lstg", lib);
            lua_pop(L, 1);
        }
    };
}

// 在 LuaWrapper.cpp 中调用此函数注册
namespace luastg::binding
{
    void RegisterAsyncResourceManager(lua_State* L)
    {
        AsyncResourceManager::Register(L);
    }
}
