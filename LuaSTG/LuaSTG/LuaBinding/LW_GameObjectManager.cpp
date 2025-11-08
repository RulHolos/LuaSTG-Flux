#include "LuaBinding/LuaWrapper.hpp"
#include "AppFrame.h"
#include "LuaBinding/modern/GameObject.hpp"

void luastg::binding::GameObjectManager::Register(lua_State* L) noexcept
{
	struct Wrapper
	{
		// 对象管理器
		static int GetnObj(lua_State* L) noexcept
		{
			lua_pushinteger(L, (lua_Integer)LPOOL.GetObjectCount());
			return 1;
		}
		static int SetBound(lua_State* L) noexcept
		{
			LPOOL.SetBound(
				luaL_checknumber(L, 1),
				luaL_checknumber(L, 2),
				luaL_checknumber(L, 3),
				luaL_checknumber(L, 4)
			);
			return 0;
		}
		static int GetBound(lua_State* L) noexcept
		{
			const auto [l, r, b, t] = LPOOL.GetBound();
			lua_pushnumber(L, l);
			lua_pushnumber(L, r);
			lua_pushnumber(L, b);
			lua_pushnumber(L, t);
			return 4;
		}
		static int UpdateXY(lua_State* L) noexcept
		{
			LPOOL.UpdateXY();
			return 0;
		}
		// EX+ 对象更新相关，影响 frame 回调函数以及对象更新
		static int GetSuperPause(lua_State* L) noexcept
		{
			lua_pushinteger(L, LPOOL.GetNextFrameSuperPauseTime());
			return 1;
		}
		static int SetSuperPause(lua_State* L) noexcept
		{
			LPOOL.SetNextFrameSuperPauseTime(luaL_checkinteger(L, 1));
			return 0;
		}
		static int AddSuperPause(lua_State* L) noexcept
		{
			lua_Integer a = luaL_checkinteger(L, 1);
			LPOOL.SetNextFrameSuperPauseTime(LPOOL.GetNextFrameSuperPauseTime() + a);
			return 0;
		}
		static int GetCurrentSuperPause(lua_State* L) noexcept
		{
			lua_pushinteger(L, LPOOL.GetSuperPauseTime());
			return 1;
		}
		// EX+ multi world  world mask
#ifdef USING_MULTI_GAME_WORLD
		static int GetWorldFlag(lua_State* L) noexcept
		{
			lua_pushinteger(L, LPOOL.GetWorldFlag());
			return 1;
		}
		static int SetWorldFlag(lua_State* L) noexcept
		{
			LPOOL.SetWorldFlag(luaL_checkinteger(L, 1));
			return 1;
		}
		static int IsSameWorld(lua_State* L) noexcept
		{
			int a = luaL_checkinteger(L, 1);
			int b = luaL_checkinteger(L, 2);
			lua_pushboolean(L, GameObjectPool::CheckWorlds(a, b));
			return 1;
		}
		static int SetActiveWorlds(lua_State* L) noexcept
		{
			int mask = luaL_optinteger(L, 1, 0);
			LPOOL.SetActiveWorlds(mask);
			return 0;
		}
		static int GetActiveWorlds(lua_State* L) noexcept
		{
			int32_t mask = LPOOL.GetActiveWorlds();
			lua_pushnumber(L, mask);
			return 1;
		}
#endif // USING_MULTI_GAME_WORLD
	};

	luaL_Reg const lib[] = {
		// 对象管理器
		{ "GetnObj", &Wrapper::GetnObj },
		{ "SetBound", &Wrapper::SetBound },
		{ "GetBound", &Wrapper::GetBound },
		{ "UpdateXY", &Wrapper::UpdateXY },
		// EX+
		{ "GetSuperPause", &Wrapper::GetSuperPause },
		{ "SetSuperPause", &Wrapper::SetSuperPause },
		{ "AddSuperPause", &Wrapper::AddSuperPause },
		{ "GetCurrentSuperPause", &Wrapper::GetCurrentSuperPause },
#ifdef USING_MULTI_GAME_WORLD
		{ "GetWorldFlag", &Wrapper::GetWorldFlag },
		{ "SetWorldFlag", &Wrapper::SetWorldFlag },
		{ "IsSameWorld", &Wrapper::IsSameWorld },
		{ "SetActiveWorlds", &Wrapper::SetActiveWorlds },
		{ "GetActiveWorlds", &Wrapper::GetActiveWorlds },
#endif // USING_MULTI_GAME_WORLD
		{ NULL, NULL },
	};

	luaL_Reg const lib_empty[] = {
		{ NULL, NULL },
	};

	luaL_register(L, LUASTG_LUA_LIBNAME, lib);                      // ??? lstg
	luaL_register(L, LUASTG_LUA_LIBNAME ".GameObjectManager", lib); // ??? lstg lstg.GameObjectManager
	lua_setfield(L, -1, "GameObjectManager");                       // ??? lstg
	lua_pop(L, 1);                                                  // ???
}
