#include "lua_imgui_binding.hpp"
#include "lua/plus.hpp"

using std::string_view_literals::operator ""sv;

namespace {
	int notSupported(lua_State* const vm) {
		return luaL_error(vm, "not supported");
	}
	int DockSpace(lua_State* const vm) {
		lua::stack_t const ctx(vm);
		auto const dockspace_id = ctx.get_value<ImGuiID>(1);
		auto const size = imgui::binding::ImVec2Binding::as(vm, 2, ImVec2());
		auto const flags = ctx.get_value<ImGuiDockNodeFlags>(3, 0);
		ctx.push_value(ImGui::DockSpace(dockspace_id, *size, flags));
		return 1;
	}
	int DockSpaceOverViewport(lua_State* const vm) {
		lua::stack_t const ctx(vm);
		auto const dockspace_id = ctx.get_value<ImGuiID>(1, 0);
		auto const flags = ctx.get_value<ImGuiDockNodeFlags>(2, 0);
		ctx.push_value(ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport(), flags));
		return 1;
	}
}

namespace imgui::binding {
	void registerImGuiViewports(lua_State* const vm) {
		lua::stack_balancer_t const sb(vm);
		lua::stack_t const ctx(vm);

		auto const m = ctx.push_module(module_ImGui_name);
		ctx.set_map_value(m, "GetMainViewport"sv, &notSupported);
		ctx.set_map_value(m, "DockSpace"sv, &DockSpace);
		ctx.set_map_value(m, "DockSpaceOverViewport"sv, &DockSpaceOverViewport);
	}
}
