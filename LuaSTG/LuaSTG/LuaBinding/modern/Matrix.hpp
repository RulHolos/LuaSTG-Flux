#pragma once
#include "lua.hpp"
#include "Core/Type.hpp"

namespace luastg::binding {
	struct Matrix {
		static std::string_view class_name;

		core::Matrix<lua_Number> data;

		static bool is(lua_State* vm, int index);
		static Matrix* as(lua_State* vm, int index);
		static Matrix* create(lua_State* vm);
		static void registerClass(lua_State* vm);
	};
}
