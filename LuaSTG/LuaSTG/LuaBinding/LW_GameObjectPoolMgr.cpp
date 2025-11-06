#include "LuaBinding/LuaWrapper.hpp"
#include "AppFrame.h"
#include "LuaBinding/modern/GameObject.hpp"
#include "lua/plus.hpp"

void luastg::binding::GameObjectPoolMgr::Register(lua_State* L) noexcept
{
	struct Wrapper
	{
        static int CreateGameObjectPool(lua_State* L) noexcept
		{
			const char* s = luaL_checkstring(L, 1);
			::luastg::CreateGameObjectPool(s);
			return 0;
		}
		static int RemoveGameObjectPool(lua_State* L) noexcept
		{
			const char* s = luaL_checkstring(L, 1);
			::luastg::RemoveGameObjectPool(s);
			return 0;
		}
		static int EnumGameObjectPools(lua_State* L)
		{
			lua::stack_t S(L);
			auto pools = ::luastg::EnumGameObjectPools();
			lua_newtable(L);
			int index = 1;
			for (auto const& pname : pools)
			{
				lua_pushstring(L, pname.c_str());
				lua_rawseti(L, -2, index++);
			}
			return 1;
		}
		static int SetActiveGameObjectPool(lua_State* L) noexcept
		{
			const char* s = luaL_checkstring(L, 1);
			::luastg::SetActiveGameObjectPoolByName(s);
			return 0;
		}
		static int GetActiveGameObjectPool(lua_State* L) noexcept
		{
			lua_pushstring(L, ::luastg::GetActiveGameObjectPoolName().c_str());
			return 1;
		}
	};

	luaL_Reg const lib[] = {
		{ "CreateGameObjectPool", &Wrapper::CreateGameObjectPool },
		{ "RemoveGameObjectPool", &Wrapper::RemoveGameObjectPool },
		{ "EnumGameObjectPools", &Wrapper::EnumGameObjectPools },
		{ "SetActiveGameObjectPool", &Wrapper::SetActiveGameObjectPool },
		{ "GetActiveGameObjectPool", &Wrapper::GetActiveGameObjectPool },
		{ NULL, NULL },
	};

	luaL_Reg const lib_empty[] = {
		{ NULL, NULL },
	};

	luaL_register(L, LUASTG_LUA_LIBNAME, lib);                      // ??? lstg
	luaL_register(L, LUASTG_LUA_LIBNAME ".GameObjectPool", lib); // ??? lstg lstg.GameObjectPool
	lua_setfield(L, -1, "GameObjectPool");                       // ??? lstg
	lua_pop(L, 1);                                                  // ???
}
