#include "AppFrame.h"
#include "Config.h"
#include "core/FileSystem.hpp"
#include "core/Configuration.hpp"

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
            spdlog::warn("[luastg] Launch file not found", file_name);
        }
#endif

        return true;
    };
    
    bool AppFrame::OnLoadMainScriptAndFiles()
    {
        std::string entry_point = core::ConfigurationLoader::getInstance().getFileSystem().getEntryPoint();
        if (entry_point.empty())
            entry_point = m_sEntryScriptPathOverride;

        if (!entry_point.empty())
        {
            core::SmartReference<core::IData> src;
            if (!core::FileSystemManager::readFile(entry_point, src.put()))
            {
                spdlog::error("[luastg] Unable to load entry script '{}'", entry_point);
                return false;
            }

            spdlog::info("[luastg] Loading script '{}'", entry_point);
            return SafeCallScript((char const*)src->data(), src->size(), entry_point.data());
        }

        spdlog::info("[luastg] Loading entry point candidates");
        constexpr std::string_view entry_scripts[] = {
            "core.lua",
            "main.lua",
            "src/main.lua",
            "src/core.lua",
        };

        for (std::string_view path : entry_scripts)
        {
            core::SmartReference<core::IData> src;
            if (core::FileSystemManager::readFile(path, src.put()))
            {
                spdlog::info("[luastg] Loading script '{}'", path);
                return SafeCallScript((char const*)src->data(), src->size(), path.data());
            }
        }

        spdlog::error("[luastg] Cannot find or load any entry point candidates at '{}', '{}', '{}', or '{}'",
            entry_scripts[0], entry_scripts[1], entry_scripts[2], entry_scripts[3]);
        return false;
    }
}
