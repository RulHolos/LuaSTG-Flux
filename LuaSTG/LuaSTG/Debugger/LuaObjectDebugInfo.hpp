#pragma once
#include <string>
#include <cstdint>

namespace LuaZoneColor {
    constexpr uint32_t Render = 0x4FC3F7; // light blue
    constexpr uint32_t Frame = 0x81C784; // light green
    constexpr uint32_t RenderCustom = 0x29B6F6; // blue  (has_callback_render)
    constexpr uint32_t FrameCustom = 0x66BB6A; // green (has_callback_frame)

    constexpr uint32_t Unknown = 0x9E9E9E; // grey
}

struct LuaObjectDebugInfo {
    std::string class_name;

    std::string zone_render;
    std::string zone_frame;

    std::string render_src;
    std::string frame_src;

    bool valid = false;

    void build_zone_names(int group, int layer)
    {
        auto loc_r = render_src.empty() ? "?" : render_src;
        auto loc_f = frame_src.empty() ? "?" : frame_src;
        auto cls = class_name.empty() ? "?" : class_name;

        zone_render = cls + "::render @ " + loc_r + " [grp=" + std::to_string(group) + ", lyr=" + std::to_string(layer) + "]";
        zone_frame = cls + "::frame @ " + loc_f + " [grp=" + std::to_string(group) + ", lyr=" + std::to_string(layer) + "]";

        valid = true;
    }
};