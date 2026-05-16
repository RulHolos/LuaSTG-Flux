#pragma once
#include "lua.hpp"

namespace luastg::binding {
	struct RichText {
		static std::string_view const class_name;

		struct Impl;
		Impl* data{};

		static bool is(lua_State* vm, int index);
		static RichText* as(lua_State* vm, int index);
		static RichText* create(lua_State* vm);
		static void registerClass(lua_State* vm);
	};
}
