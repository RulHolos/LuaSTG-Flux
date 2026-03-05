#pragma once

#include "LuaObjectDebugInfo.hpp"
#include <tracy/Tracy.hpp>
#include <lua.hpp>
#include <cstring>
#include <string>

namespace LuaDebugCache {

    static std::string get_func_location(lua_State* L, int idx)
    {
        lua_pushvalue(L, idx);

        lua_Debug ar{};
        if (lua_getinfo(L, ">Sn", &ar) == 0)
            return "?";
        
        std::string result = ar.short_src;

        if (result.size() > 2 && result[0] == '.' && result[1] == '/')
            result = result.substr(2);
        
        result += ':';
        result += std::to_string(ar.linedefined);
        return result;
    }

    static std::string resolve_class_name(lua_State* L, int class_idx)
    {
        // Tentative 1
        for (const char* key : {"class_name", "__name", "_name", "name"})
        {
            lua_getfield(L, class_idx, key);
            if (lua_isstring(L, -1))
            {
                std::string name = lua_tostring(L, -1);
                if (!name.empty())
                {
                    lua_pop(L, 1);
                    return name;
                }
            }
            lua_pop(L, 1);
        }

        // Tentative 2 (likely to fail)
        lua_getglobal(L, "_editor_class");
        if (lua_istable(L, -1))
        {
            std::string found_name;
            lua_pushnil(L);
            while (lua_next(L, -2) != 0)
            {
                if (lua_istable(L, -1))
                {
                    lua_pushvalue(L, class_idx);
                    if (lua_rawequal(L, -1, -2))
                    {
                        lua_pop(L, 1);
                        if (lua_isstring(L, -2))
                            found_name = lua_tostring(L, -2);
                        lua_pop(L, 2);
                        break;
                    } else {
                        lua_pop(L, 1);
                    }
                }
                lua_pop(L, 1);
            }
            if (!found_name.empty())
            {
                lua_pop(L, 1);
                return found_name;
            }
        }
        lua_pop(L, 1);
        
        // Tentative 3
        lua_getfield(L, class_idx, "render");
        if (lua_isfunction(L, -1))
        {
            lua_Debug ar{};
            lua_pushvalue(L, -1);
            if (lua_getinfo(L, ">S", &ar))
            {
                std::string src = ar.short_src;
                auto slash = src.rfind('/');
                if (slash == std::string::npos)
                    slash = src.rfind('\\');
                if (slash != std::string::npos)
                    src = src.substr(slash + 1);
                auto dot = src.rfind('.');
                if (dot != std::string::npos)
                    src = src.substr(0, dot);
                if (!src.empty())
                {
                    lua_pop(L, 1);
                    return src;
                }
            }
        }
        lua_pop(L, 1);

        return "UnknownClass";
    }

    template<typename TGameObject>
    static void populate(lua_State* L, TGameObject& obj, int class_table_idx)
    {
        int const top = lua_gettop(L);

        LuaObjectDebugInfo& info = obj.lua_debug;
        info = {};

        if (!lua_istable(L, class_table_idx))
        {
            info.class_name = "NotAClass";
            info.build_zone_names(obj.group, obj.layer);
            return;
        }

        if (class_table_idx < 0)
            class_table_idx = top + class_table_idx + 1;

        info.class_name = resolve_class_name(L, class_table_idx);

        lua_getfield(L, class_table_idx, "render");
        if (lua_isfunction(L, -1))
            info.render_src = get_func_location(L, lua_gettop(L));

        lua_getfield(L, class_table_idx, "frame");
        if (lua_isfunction(L, -1))
            info.frame_src = get_func_location(L, lua_gettop(L));

        info.build_zone_names(obj.group, obj.layer);

        lua_settop(L, top);
    }
}

#define LSTG_TRACY_RENDER_COLOR(p) \
    ((p)->features.has_callback_render ? LuaZoneColor::RenderCustom : LuaZoneColor::Render)

#define LSTG_TRACY_FRAME_COLOR(p) \
    ((p)->features.has_callback_update  ? LuaZoneColor::FrameCustom  : LuaZoneColor::Frame)

#define LSTG_TRACY_RENDER_ZONE(p) \
    ZoneNamed(_tracy_render_zone_, true); \
    ZoneColorV(_tracy_render_zone_, LSTG_TRACY_RENDER_COLOR(p)); \
    if ((p)->lua_debug.valid) { \
        ZoneNameV(_tracy_render_zone_, (p)->lua_debug.zone_render.c_str(), (p)->lua_debug.zone_render.size()); \
    }  else { \
        ZoneNameVF(_tracy_render_zone_, "Obj.Render id=%zu uid=%llu grp=%d lyr=%d", (p)->id, (p)->unique_id, (p)->group, (p)->layer); \
    }

#define LSTG_TRACY_FRAME_ZONE(p) \
    ZoneNamed(_tracy_frame_zone_, true); \
    ZoneColorV(_tracy_frame_zone_, LSTG_TRACY_FRAME_COLOR(p)); \
    if ((p)->lua_debug.valid) { \
        ZoneNameV(_tracy_frame_zone_, (p)->lua_debug.zone_frame.c_str(), (p)->lua_debug.zone_frame.size()); \
    }  else { \
        ZoneNameVF(_tracy_frame_zone_, "Obj.Frame id=%zu uid=%llu grp=%d lyr=%d", (p)->id, (p)->unique_id, (p)->group, (p)->layer); \
    }