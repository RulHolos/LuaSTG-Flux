#include "lua_imgui_binding.hpp"
#include "lua/plus.hpp"

using std::string_view_literals::operator ""sv;

namespace {
	static ImVec2 const uv0_default{ 0.0f, 0.0f };
	static ImVec2 const uv1_default{ 1.0f, 1.0f };
	static ImVec4 const bg_col_default{ 0.0f, 0.0f, 0.0f, 0.0f };
	static ImVec4 const tint_col_default{ 1.0f, 1.0f, 1.0f, 1.0f };

	int Image(lua_State* const vm) {
		lua::stack_t const ctx(vm);
		auto const tex = imgui::binding::ImTextureRefBinding::as(vm, 1);
		auto const image_size = imgui::binding::ImVec2Binding::as(vm, 2);
		auto const uv0 = imgui::binding::ImVec2Binding::as(vm, 3, uv0_default);
		auto const uv1 = imgui::binding::ImVec2Binding::as(vm, 4, uv1_default);
		ImGui::Image(*tex, *image_size, *uv0, *uv1);
		return 0;
	}
	int ImageWithBg(lua_State* const vm) {
		lua::stack_t const ctx(vm);
		auto const tex = imgui::binding::ImTextureRefBinding::as(vm, 1);
		auto const image_size = imgui::binding::ImVec2Binding::as(vm, 2);
		auto const uv0 = imgui::binding::ImVec2Binding::as(vm, 3, uv0_default);
		auto const uv1 = imgui::binding::ImVec2Binding::as(vm, 4, uv1_default);
		ImVec4 const& bg_col = ctx.is_non_or_nil(5) ? bg_col_default : *imgui::binding::ImVec4Binding::as(vm, 5);
		ImVec4 const& tint_col = ctx.is_non_or_nil(6) ? tint_col_default : *imgui::binding::ImVec4Binding::as(vm, 6);
		ImGui::ImageWithBg(*tex, *image_size, *uv0, *uv1, bg_col, tint_col);
		return 0;
	}
	int ImageButton(lua_State* const vm) {
		lua::stack_t const ctx(vm);
		auto const str_id = ctx.get_value<std::string_view>(1);
		auto const tex = imgui::binding::ImTextureRefBinding::as(vm, 2);
		auto const image_size = imgui::binding::ImVec2Binding::as(vm, 3);
		auto const uv0 = imgui::binding::ImVec2Binding::as(vm, 4, uv0_default);
		auto const uv1 = imgui::binding::ImVec2Binding::as(vm, 5, uv1_default);
		ImVec4 const& bg_col = ctx.is_non_or_nil(6) ? bg_col_default : *imgui::binding::ImVec4Binding::as(vm, 6);
		ImVec4 const& tint_col = ctx.is_non_or_nil(7) ? tint_col_default : *imgui::binding::ImVec4Binding::as(vm, 7);
		auto const result = ImGui::ImageButton(str_id.data(), *tex, *image_size, *uv0, *uv1, bg_col, tint_col);
		ctx.push_value(result);
		return 1;
	}
}

namespace imgui::binding {
	void registerImGuiWidgetsImages(lua_State* const vm) {
		lua::stack_balancer_t const sb(vm);
		lua::stack_t const ctx(vm);

		auto const m = ctx.push_module(module_ImGui_name);
		ctx.set_map_value(m, "Image"sv, &Image);
		ctx.set_map_value(m, "ImageWithBg"sv, &ImageWithBg);
		ctx.set_map_value(m, "ImageButton"sv, &ImageButton);
	}
}
