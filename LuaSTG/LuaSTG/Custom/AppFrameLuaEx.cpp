#include "AppFrame.h"
#include "Config.h"
#include "core/FileSystem.hpp"

#include "Platform/HResultChecker.hpp"

namespace luastg
{
    bool AppFrame::OnLoadLaunchScriptAndFiles()
    {
        bool is_launch_loaded = false;
        #ifdef USING_LAUNCH_FILE
        spdlog::info("[luastg] Loading launch file");
        core::SmartReference<core::IData> src;
        const char* file_name = LUASTG_LAUNCH_SCRIPT;
        if (core::FileSystemManager::hasFile(LUASTG_LAUNCH_SCRIPT ".lua"))
        {
            file_name = LUASTG_LAUNCH_SCRIPT ".lua";
        }
        if (core::FileSystemManager::readFile(file_name, src.put()))
        {
            spdlog::info("[luastg] Found '{}'", file_name);

            if (SafeCallScript((char const*)src->data(), src->size(), file_name))
            {
                is_launch_loaded = true;
                spdlog::info("[luastg] Loading script '{}'", file_name);
            }
            else
            {
                spdlog::error("[luastg] Failed to load launch file '{}'", file_name);
            }
        }
        if (!is_launch_loaded)
        {
            spdlog::error("[luastg] Launch file not found", file_name);
        }
        #endif

        return true;
    };
    
    bool AppFrame::OnLoadMainScriptAndFiles()
    {
        spdlog::info("[luastg] Loading entry point candidates");
        std::string_view entry_scripts[3] = {
            "core.lua",
            "main.lua",
            "src/main.lua",
        };
        std::string_view entry_script;
        for (auto& v : entry_scripts)
        {
            if (core::FileSystemManager::hasFile(v))
            {
                entry_script = v;
                break;
            }
        }
        if (entry_script.empty())
        {
            spdlog::error("[luastg] Cannot find any entry point candidates at '{}', '{}' or '{}'", entry_scripts[0], entry_scripts[1], entry_scripts[2]);
            return true;
        }
        core::SmartReference<core::IData> src;
        if (core::FileSystemManager::readFile(entry_script, src.put()))
        {
            spdlog::info("[luastg] Loading script '{}'", entry_script);
            SafeCallScript((char const*)src->data(), src->size(), entry_script.data());
        }
        return true;
    }
}
