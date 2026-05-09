#pragma once

struct xSteamRichPresence
{
    static int SetRichPresence(lua_State* L)
    {
        const char* pchKey = luaL_checkstring(L, 1);
        const char* pchValue = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
        const bool ret = SteamFriends()->SetRichPresence(pchKey, pchValue);
        lua_pushboolean(L, ret);
        return 1;
    }
    static int ClearRichPresence(lua_State* L)
    {
        SteamFriends()->ClearRichPresence();
        return 0;
    }
    
    // Friend rich presence
    
    static int GetFriendRichPresence(lua_State* L)
    {
        const uint64 ullID = (uint64)luaL_checkinteger(L, 1);
        const CSteamID steamIDFriend(ullID);
        const char* pchKey = luaL_checkstring(L, 2);
        const char* ret = SteamFriends()->GetFriendRichPresence(steamIDFriend, pchKey);
        lua_pushstring(L, ret);
        return 1;
    }
    static int GetFriendRichPresenceKeyCount(lua_State* L)
    {
        const uint64 ullID = (uint64)luaL_checkinteger(L, 1);
        const CSteamID steamIDFriend(ullID);
        const int ret = SteamFriends()->GetFriendRichPresenceKeyCount(steamIDFriend);
        lua_pushinteger(L, ret);
        return 1;
    }
    static int GetFriendRichPresenceKeyByIndex(lua_State* L)
    {
        const uint64 ullID = (uint64)luaL_checkinteger(L, 1);
        const CSteamID steamIDFriend(ullID);
        const int iKey = (int)luaL_checkinteger(L, 2);
        const char* ret = SteamFriends()->GetFriendRichPresenceKeyByIndex(steamIDFriend, iKey);
        lua_pushstring(L, ret);
        return 1;
    }
    static int RequestFriendRichPresence(lua_State* L)
    {
        const uint64 ullID = (uint64)luaL_checkinteger(L, 1);
        const CSteamID steamIDFriend(ullID);
        SteamFriends()->RequestFriendRichPresence(steamIDFriend);
        return 0;
    }

    static int xRegister(lua_State* L)
    {
        static const luaL_Reg lib[] = {
            xfbinding(SetRichPresence),
            xfbinding(ClearRichPresence),
            xfbinding(GetFriendRichPresence),
            xfbinding(GetFriendRichPresenceKeyCount),
            xfbinding(GetFriendRichPresenceKeyByIndex),
            xfbinding(RequestFriendRichPresence),
            {NULL, NULL},
        };
        lua_pushstring(L, "SteamRichPresence");
        lua_createtable(L, 0, 6);
        luaL_register(L, NULL, lib);
        lua_settable(L, -3);
        return 0;
    };
};