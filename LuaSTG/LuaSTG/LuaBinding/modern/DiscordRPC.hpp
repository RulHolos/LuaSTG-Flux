#pragma once
#include "lua.hpp"

namespace luastg::binding {

	struct DiscordRPC {

		static std::string_view const class_name;

		static void registerClass(lua_State* L);

	};

}
