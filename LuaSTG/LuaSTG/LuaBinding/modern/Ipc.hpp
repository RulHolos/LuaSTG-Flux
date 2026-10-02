#pragma once
#include "lua.hpp"

namespace luastg::binding
{

    struct Ipc
    {

        static std::string_view const class_name;

        static void registerClass(lua_State *vm);

        static void update(lua_State *vm);
    };

}
