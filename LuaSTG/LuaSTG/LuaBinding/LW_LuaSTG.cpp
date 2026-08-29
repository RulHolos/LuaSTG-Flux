#include "LuaBinding/LuaWrapper.hpp"
#include "Platform/CleanWindows.hpp"
#include "lua/plus.hpp"
#include "AppFrame.h"
//#include "core/Configuration.hpp"
#include <psapi.h>
#include <dxgi1_4.h>

inline core::RectI lua_to_Core_RectI(lua_State* L, int idx)
{
	if (!lua_istable(L, idx))
	{
		return core::RectI();
	}

	core::Vector2I pos;

	lua_getfield(L, idx, "x");
	pos.x = (int32_t)luaL_checkinteger(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, idx, "y");
	pos.y = (int32_t)luaL_checkinteger(L, -1);
	lua_pop(L, 1);

	core::Vector2I size;

	lua_getfield(L, idx, "width");
	size.x = (int32_t)luaL_checkinteger(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, idx, "height");
	size.y = (int32_t)luaL_checkinteger(L, -1);
	lua_pop(L, 1);

	return core::RectI(pos, pos + size);
}

void luastg::binding::BuiltInFunction::Register(lua_State* L)noexcept
{
	struct Wrapper
	{
		static int SetEntryScript(lua_State* L)noexcept
		{
			if (LAPP.m_iStatus != AppStatus::Initializing)
			{
				spdlog::warn("lstg.SetEntryScript() was called outside of the engine initialization step. This call will result in a no-op");
				return 0;
			}

			LAPP.m_sEntryScriptPathOverride = luaL_checkstring(L, 1);
			return 0;
		}

		#pragma region 框架函数
		// 框架函数
		static int GetVersionNumber(lua_State* L)noexcept
		{
			lua_pushinteger(L, LUASTG_VERSION_MAJOR);
			lua_pushinteger(L, LUASTG_VERSION_MINOR);
			lua_pushinteger(L, LUASTG_VERSION_PATCH);
			return 3;
		}
		static int GetVersionName(lua_State* L)noexcept
		{
			lua_pushstring(L, LUASTG_INFO);
			return 1;
		}
		static int GetBranchName(lua_State* L)noexcept
		{
			lua_pushstring(L, LUASTG_BRANCH);
			return 1;
		}
		static int SetWindowed(lua_State* L)noexcept
		{
			LAPP.SetWindowed(lua_toboolean(L, 1));
			return 0;
		}
		static int SetBorderless(lua_State* L)noexcept
		{
			LAPP.SetBorderless(lua_toboolean(L, 1));
			return 0;
		}
		static int SetVsync(lua_State* L)noexcept
		{
			LAPP.SetVsync(lua_toboolean(L, 1));
			return 0;
		}
		static int SetResolution(lua_State* L)noexcept
		{
			LAPP.SetResolution(
				(uint32_t)luaL_checkinteger(L, 1),
				(uint32_t)luaL_checkinteger(L, 2)
			);
			return 0;
		}
		static int SetPreferenceGPU(lua_State* L)noexcept
		{
			LAPP.SetPreferenceGPU(luaL_checkstring(L, 1));
			return 0;
		}
		static int SetFPS(lua_State* L)noexcept
		{
			int v = (int)luaL_checkinteger(L, 1);
			if (v <= 0)
				v = 60;
			LAPP.SetFPS((uint32_t)v);
			return 0;
		}
		static int GetFPS(lua_State* L)noexcept
		{
			lua_pushnumber(L, LAPP.GetFPS());
			return 1;
		}
		static int Log(lua_State* L)noexcept
		{
			lua::stack_t S(L);
			auto const level = S.get_value<int32_t>(1);
			auto const message = S.get_value<std::string_view>(2);
			spdlog::log(static_cast<spdlog::level::level_enum>(level), "[lua] {}", message);
			return 0;
		}
		static int DoFile(lua_State* L)noexcept
		{
			int args = lua_gettop(L);//获取此时栈上的值的数量
			LAPP.LoadScript(L, luaL_checkstring(L, 1), luaL_optstring(L, 2, NULL));
			return (lua_gettop(L) - args);
		}
		static int LoadTextFile(lua_State* L)noexcept
		{
			return LAPP.LoadTextFile(L, luaL_checkstring(L, 1), luaL_optstring(L, 2, NULL));
		}
		static int LoadCompressedTextFile(lua_State* L)
		{
			return LAPP.LoadCompressedTextFile(L, luaL_checkstring(L, 1), luaL_optstring(L, 2, NULL));
		}
		#pragma endregion
		
		#pragma region 窗口与交换链控制函数
		// 窗口与交换链控制函数
		static int ChangeVideoMode(lua_State* L)noexcept
		{
			lua::stack_t S(L);
			uint32_t const width = S.get_value<uint32_t>(1);
			uint32_t const height = S.get_value<uint32_t>(2);
			auto const window_mode = S.get_value<std::string_view>(3);
			bool const vsync = S.get_value<bool>(4);

			auto const size = core::Vector2U(width, height);

			if (window_mode == "windowed")
			{
				bool const result = LAPP.SetDisplayModeWindow(size, vsync);
				lua_pushboolean(L, result);
			}
			else if (window_mode == "borderless")
			{
				bool const result = LAPP.SetDisplayModeBorderlessFullscreen(size, vsync);
				lua_pushboolean(L, result);
			}
			else if (window_mode == "fullscreen")
			{
				bool const result = LAPP.SetDisplayModeExclusiveFullscreen(size, vsync, core::Rational());
				lua_pushboolean(L, result);
			}
			else {
				return luaL_error(L, "unknown window mode.");
			}

			return 1;
		}
		static int EnumResolutions(lua_State* L)
		{
			lua_createtable(L, 5, 0);		// t
			core::Graphics::DisplayMode mode_list[5] = {
				{ 640, 480, { 60, 1 }, core::Graphics::Format::B8G8R8A8_UNORM },
				{ 800, 600, { 60, 1 }, core::Graphics::Format::B8G8R8A8_UNORM },
				{ 960, 720, { 60, 1 }, core::Graphics::Format::B8G8R8A8_UNORM },
				{ 1024, 768, { 60, 1 }, core::Graphics::Format::B8G8R8A8_UNORM },
				{ 1280, 960, { 60, 1 }, core::Graphics::Format::B8G8R8A8_UNORM },
			};
			for (int index = 0; index < 5; index += 1)
			{
				auto mode = mode_list[index];

				lua_createtable(L, 4, 0);		// t t

				lua_pushinteger(L, (lua_Integer)mode.width);
				lua_rawseti(L, -2, 1);

				lua_pushinteger(L, (lua_Integer)mode.height);
				lua_rawseti(L, -2, 2);

				lua_pushnumber(L, (lua_Number)mode.refresh_rate.numerator); // 有点担心存不下
				lua_rawseti(L, -2, 3);

				lua_pushnumber(L, (lua_Number)mode.refresh_rate.denominator); // 有点担心存不下
				lua_rawseti(L, -2, 4);

				lua_rawseti(L, -2, index + 1);	// t
			}
			return 1;
		}
		static int EnumGPUs(lua_State* L) {
			lua::stack_t S(L);
			if (LAPP.GetAppModel())
			{
				auto* p_device = LAPP.GetAppModel()->getDevice();
				auto count = p_device->getGpuCount();
				lua_createtable(L, count, 0);		// t
				for (int index = 0; index < (int)count; index += 1)
				{
					S.push_value(p_device->getGpuName((uint32_t)index)); // t name
					lua_rawseti(L, -2, index + 1);	// t
				}
				return 1;
			}
			else
			{
				return luaL_error(L, "render device is not avilable.");
			}
		}
		static int ChangeGPU(lua_State* L) {
			lua::stack_t S(L);
			if (LAPP.GetAppModel())
			{
				auto const gpu = S.get_value<std::string_view>(1);
				auto* p_device = LAPP.GetAppModel()->getDevice();
				p_device->setPreferenceGpu(gpu);
				if (!p_device->recreate())
					return luaL_error(L, "ChangeGPU failed.");
				return 0;
			}
			else
			{
				return luaL_error(L, "render device is not avilable.");
			}
		}
		static int GetCurrentGpuName(lua_State* L)
		{
			if (!LAPP.GetAppModel()->getDevice())
			{
				return luaL_error(L, "render device is not avilable.");
			}
			lua::stack_t S(L);
			auto const name = LAPP.GetAppModel()->getDevice()->getCurrentGpuName();
			S.push_value<std::string_view>(name);
			return 1;
		}
		static int SetSwapChainScalingMode(lua_State* L)noexcept
		{
			LAPP.GetAppModel()->getSwapChain()->setScalingMode(
				(core::Graphics::SwapChainScalingMode)luaL_checkinteger(L, 1));
			return 0;
		}
		#pragma endregion

		#pragma region Profiling
		static int CurrentTick(lua_State* L)
		{
			LARGE_INTEGER counter;
			QueryPerformanceCounter(&counter);

			lua::stack_t S(L);
			S.push_value(counter.QuadPart);
			return 1;
		}

		static int TimeElapsedMs(lua_State* L)
		{
			LARGE_INTEGER counter;
			QueryPerformanceCounter(&counter);

			lua::stack_t S(L);
			const int64_t startTick = S.get_value<int64_t>(1);
			S.push_value(1000 * (double)(counter.QuadPart - startTick) / (double)LAPP.GetAppModel()->getQPF());
			return 1;
		}

		static int GetMemoryUsage(lua_State* L)
		{
			PROCESS_MEMORY_COUNTERS_EX info = {};
			info.cb = sizeof(info);

			if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&info, sizeof(info))) {
				lua_newtable(L);

				lua_pushinteger(L, info.WorkingSetSize);
				lua_setfield(L, -2, "activeMemoryUsage");

				lua_pushinteger(L, info.PrivateUsage);
				lua_setfield(L, -2, "privMemoryUsage");

				MEMORYSTATUSEX systemInfo = { sizeof(MEMORYSTATUSEX) };
				if (GlobalMemoryStatusEx(&systemInfo)) {
					lua_pushinteger(L, systemInfo.dwMemoryLoad);
					lua_setfield(L, -2, "systemMemoryLoad");

					lua_pushnumber(L, 100.0 * info.WorkingSetSize / systemInfo.ullTotalPhys);
					lua_setfield(L, -2, "ramUsagePercent");
				}

				return 1;
			}

			return 0;
		}

		static int GetGameObjectStats(lua_State* L)
		{
			auto objInfo = LPOOL.DebugGetFrameStatistics();

			lua_newtable(L);
			lua_pushinteger(L, objInfo.object_alloc);
			lua_setfield(L, -2, "allocated");
			lua_pushinteger(L, objInfo.object_free);
			lua_setfield(L, -2, "freed");
			lua_pushinteger(L, objInfo.object_alive);
			lua_setfield(L, -2, "active");
			lua_pushinteger(L, objInfo.object_colli_check);
			lua_setfield(L, -2, "collisionChecks");
			lua_pushinteger(L, objInfo.object_colli_callback);
			lua_setfield(L, -2, "collisionCallbacks");

			return 1;
		}

		static int GetGPUStats(lua_State* L)
		{
			core::Graphics::DeviceMemoryUsageStatistics stats = LAPP.GetAppModel()->getDevice()->getMemoryUsageStatistics();
			
			lua_newtable(L);
			lua_pushinteger(L, stats.local.current_usage);
			lua_setfield(L, -2, "localUsage");
			lua_pushinteger(L, stats.local.budget);
			lua_setfield(L, -2, "localBudget");
			lua_pushinteger(L, stats.non_local.current_usage);
			lua_setfield(L, -2, "nonLocalUsage");
			lua_pushinteger(L, stats.non_local.budget);
			lua_setfield(L, -2, "nonLocalBudget");

			return 1;
		}
		#pragma endregion
	};
	
	luaL_Reg tFunctions[] = {
		{ "SetEntryScript", &Wrapper::SetEntryScript },

		#pragma region 框架函数
		{ "GetVersionNumber", &Wrapper::GetVersionNumber },
		{ "GetVersionName", &Wrapper::GetVersionName },
		{ "GetBranchName", &Wrapper::GetBranchName },
		{ "SetWindowed", &Wrapper::SetWindowed },
		{ "SetBorderless", &Wrapper::SetBorderless },
		{ "SetFPS", &Wrapper::SetFPS },
		{ "GetFPS", &Wrapper::GetFPS },
		{ "SetVsync", &Wrapper::SetVsync },
		{ "SetPreferenceGPU", &Wrapper::SetPreferenceGPU },
		{ "SetResolution", &Wrapper::SetResolution },
		{ "Log", &Wrapper::Log },
		{ "DoFile", &Wrapper::DoFile },
		{ "LoadTextFile", &Wrapper::LoadTextFile },
		{ "LoadCompressedTextFile", &Wrapper::LoadCompressedTextFile },
		#pragma endregion
		
		#pragma region 窗口与交换链控制函数
		{ "ChangeVideoMode", &Wrapper::ChangeVideoMode },
		{ "EnumResolutions", &Wrapper::EnumResolutions },
		{ "EnumGPUs", &Wrapper::EnumGPUs },
		{ "ChangeGPU", &Wrapper::ChangeGPU },
		{ "GetCurrentGpuName", &Wrapper::GetCurrentGpuName },
		{ "SetSwapChainScalingMode", &Wrapper::SetSwapChainScalingMode },
		#pragma endregion

		#pragma region Profiling
		{ "CurrentTick", &Wrapper::CurrentTick },
		{ "TimeElapsedMs", &Wrapper::TimeElapsedMs },
		{ "GetMemoryUsage", &Wrapper::GetMemoryUsage },
		{ "GetGameObjectStats", &Wrapper::GetGameObjectStats },
		{ "GetGPUStats", &Wrapper::GetGPUStats },
		#pragma endregion
		
		{ NULL, NULL },
	};
	
	luaL_register(L, LUASTG_LUA_LIBNAME, tFunctions);	// ? t
	lua_pop(L, 1);										// ?
}
