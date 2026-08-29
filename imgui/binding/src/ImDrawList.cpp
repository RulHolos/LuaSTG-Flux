#include "lua_imgui_binding.hpp"
#include "lua/plus.hpp"
#include <cassert>

using std::string_view_literals::operator ""sv;

namespace imgui::binding {
	std::string_view const ImDrawListBinding::class_name{ "imgui.ImDrawList"sv };

	struct ImDrawListWrapper : ImDrawListBinding {
		static int toString(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			[[maybe_unused]] auto const self = as(vm, 1);
			ctx.push_value(class_name);
			return 1;
		}

		static int notSupported(lua_State* const vm) {
			return luaL_error(vm, "not yet supported");
		}

		static int getter(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const key = ctx.get_value<std::string_view>(2);

			#define SUPPORTED(name) \
				if (key == (#name ""sv)) { ctx.push_value(&name); return 1; } (void)0

			SUPPORTED(AddLine);
			SUPPORTED(AddRect);
			SUPPORTED(AddRectFilled);
			#undef SUPPORTED

			#define UNSUPPORTED(name) \
				if (key == (#name ""sv)) { ctx.push_value(&notSupported); return 1; } (void)0

			UNSUPPORTED(AddBezierCurve);
			UNSUPPORTED(AddCallback);
			UNSUPPORTED(AddCircle);
			UNSUPPORTED(AddCircleFilled);
			UNSUPPORTED(AddConvexPolyFilled);
			UNSUPPORTED(AddDrawCmd);
			UNSUPPORTED(AddImage);
			UNSUPPORTED(AddImageQuad);
			UNSUPPORTED(AddImageRounded);
			UNSUPPORTED(AddPolyline);
			UNSUPPORTED(AddQuad);
			UNSUPPORTED(AddQuadFilled);
			UNSUPPORTED(AddRectFilledMultiColor);
			UNSUPPORTED(AddText);
			UNSUPPORTED(AddTriangle);
			UNSUPPORTED(AddTriangleFilled);
			UNSUPPORTED(ChannelsMerge);
			UNSUPPORTED(ChannelsSetCurrent);
			UNSUPPORTED(ChannelsSplit);
			UNSUPPORTED(Clear);
			UNSUPPORTED(ClearFreeMemory);
			UNSUPPORTED(CloneOutput);
			UNSUPPORTED(GetClipRectMax);
			UNSUPPORTED(GetClipRectMin);
			UNSUPPORTED(PathArcTo);
			UNSUPPORTED(PathArcToFast);
			UNSUPPORTED(PathBezierCurveTo);
			UNSUPPORTED(PathClear);
			UNSUPPORTED(PathFillConvex);
			UNSUPPORTED(PathLineTo);
			UNSUPPORTED(PathLineToMergeDuplicate);
			UNSUPPORTED(PathRect);
			UNSUPPORTED(PathStroke);
			UNSUPPORTED(PushClipRect);
			UNSUPPORTED(PushClipRectFullScreen);
			UNSUPPORTED(PopClipRect);
			UNSUPPORTED(PushTextureID);
			UNSUPPORTED(PopTextureID);
			UNSUPPORTED(PrimQuadUV);
			UNSUPPORTED(PrimRect);
			UNSUPPORTED(PrimRectUV);
			UNSUPPORTED(PrimReserve);
			UNSUPPORTED(PrimVtx);
			UNSUPPORTED(PrimWriteIdx);
			UNSUPPORTED(PrimWriteVtx);
			UNSUPPORTED(UpdateClipRect);
			UNSUPPORTED(UpdateTextureID);
			#undef UNSUPPORTED

			return luaL_error(vm, "field '%s' does not exist", key.data());
		}

		static int AddLine(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const self = as(vm, 1);

			auto const p1 = *ImVec2Binding::as(vm, 2);
			auto const p2 = *ImVec2Binding::as(vm, 3);
			auto const col = ctx.get_value<ImU32>(4);
			auto const thickness = ctx.get_value<float>(5, 1.0f);

			self->AddLine(p1, p2, col, thickness);
			return 0;
		}

		static int AddRect(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const self = as(vm, 1);

			auto const p_min = *ImVec2Binding::as(vm, 2);
			auto const p_max = *ImVec2Binding::as(vm, 3);
			auto const col = ctx.get_value<ImU32>(4);
			auto const rounding = ctx.get_value<float>(5, 0.0f);
			auto const flags = ctx.get_value<ImDrawFlags>(6, 0);
			auto const thickness = ctx.get_value<float>(7, 1.0f);

			self->AddRect(p_min, p_max, col, rounding, flags, thickness);
			return 0;
		}

		static int AddRectFilled(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const self = as(vm, 1);

			auto const p_min = *ImVec2Binding::as(vm, 2);
			auto const p_max = *ImVec2Binding::as(vm, 3);
			auto const col = ctx.get_value<ImU32>(4);
			auto const rounding = ctx.get_value<float>(5, 0.0f);
			auto const flags = ctx.get_value<ImDrawFlags>(6, 0);

			self->AddRectFilled(p_min, p_max, col, rounding, flags);
			return 0;
		}
	};

	void ImDrawListBinding::set(ImDrawList* const ptr) {
		data = ptr;
	}

	ImDrawList* ImDrawListBinding::get() {
		return data;
	}

	bool ImDrawListBinding::is(lua_State* const vm, int const index) {
		lua::stack_t const ctx(vm);
		return ctx.is_metatable(index, class_name);
	}

	ImDrawList* ImDrawListBinding::as(lua_State* const vm, int const index) {
		lua::stack_t const ctx(vm);
		return ctx.as_userdata<ImDrawListBinding>(index)->get();
	}

	ImDrawList* ImDrawListBinding::reference(lua_State* const vm, ImDrawList* const value) {
		lua::stack_t const ctx(vm);
		auto const self = ctx.create_userdata<ImDrawListBinding>();
		auto const self_index = ctx.index_of_top();

		ctx.set_metatable(self_index, class_name);
		self->set(value);
		return self->get();
	}

	void ImDrawListBinding::registerClass(lua_State* vm) {
		lua::stack_balancer_t const sb(vm);
		lua::stack_t const ctx(vm);

		auto const mt = ctx.create_metatable(class_name);
		ctx.set_map_value(mt, "__tostring"sv, &ImDrawListWrapper::toString);
		ctx.set_map_value(mt, "__index"sv, &ImDrawListWrapper::getter);
	}
}