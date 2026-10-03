#include "lua_imgui_binding.hpp"
#include "lua/plus.hpp"
#include <limits>

using std::string_view_literals::operator ""sv;

namespace {
	std::string_view getPayloadType(lua_State* const vm, lua::stack_t const& ctx, int const index) {
		auto const type = ctx.get_value<std::string_view>(index);
		if (type.empty() || type.size() > 32 || type.find('\0') != std::string_view::npos) {
			luaL_argerror(vm, index, "payload type must contain 1 to 32 non-null bytes");
			return {};
		}
		return type;
	}
	void pushPayloadSnapshot(lua_State* const vm, ImGuiPayload const* const payload) {
		if (payload == nullptr) {
			lua_pushnil(vm);
			return;
		}
		lua::stack_t const ctx(vm);
		auto const result = ctx.create_map(5);
		auto const data = payload->DataSize > 0
			? std::string_view(static_cast<char const*>(payload->Data), static_cast<size_t>(payload->DataSize))
			: std::string_view{};
		ctx.set_map_value(result, "Data"sv, data);
		ctx.set_map_value(result, "DataSize"sv, payload->DataSize);
		ctx.set_map_value(result, "DataType"sv, std::string_view(payload->DataType));
		ctx.set_map_value(result, "Preview"sv, payload->Preview);
		ctx.set_map_value(result, "Delivery"sv, payload->Delivery);
	}
	int BeginDragDropSource(lua_State* const vm) {
		lua::stack_t const ctx(vm);
		auto const flags = ctx.get_value<ImGuiDragDropFlags>(1, 0);
		ctx.push_value(ImGui::BeginDragDropSource(flags));
		return 1;
	}
	int SetDragDropPayload(lua_State* const vm) {
		lua::stack_t const ctx(vm);
		auto const type = getPayloadType(vm, ctx, 1);
		auto const data = ctx.get_value<std::string_view>(2);
		auto const cond = ctx.get_value<ImGuiCond>(3, 0);
		if (cond != 0 && cond != ImGuiCond_Always && cond != ImGuiCond_Once)
			return luaL_argerror(vm, 3, "condition must be ImGuiCond.Always or ImGuiCond.Once");
		if (data.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
			return luaL_argerror(vm, 2, "payload data is too large");
		auto const result = ImGui::SetDragDropPayload(type.data(), data.empty() ? nullptr : data.data(), data.size(), cond);
		ctx.push_value(result);
		return 1;
	}
	int EndDragDropSource(lua_State*) {
		ImGui::EndDragDropSource();
		return 0;
	}
	int BeginDragDropTarget(lua_State* const vm) {
		lua::stack_t const ctx(vm);
		ctx.push_value(ImGui::BeginDragDropTarget());
		return 1;
	}
	int AcceptDragDropPayload(lua_State* const vm) {
		lua::stack_t const ctx(vm);
		auto const type = getPayloadType(vm, ctx, 1);
		auto const flags = ctx.get_value<ImGuiDragDropFlags>(2, 0);
		pushPayloadSnapshot(vm, ImGui::AcceptDragDropPayload(type.data(), flags));
		return 1;
	}
	int EndDragDropTarget(lua_State*) {
		ImGui::EndDragDropTarget();
		return 0;
	}
	int GetDragDropPayload(lua_State* const vm) {
		pushPayloadSnapshot(vm, ImGui::GetDragDropPayload());
		return 1;
	}
}

namespace imgui::binding {
	void registerImGuiDragAndDrop(lua_State* const vm) {
		lua::stack_balancer_t const sb(vm);
		lua::stack_t const ctx(vm);

		auto const m = ctx.push_module(module_ImGui_name);
		ctx.set_map_value(m, "BeginDragDropSource"sv, &BeginDragDropSource);
		ctx.set_map_value(m, "SetDragDropPayload"sv, &SetDragDropPayload);
		ctx.set_map_value(m, "EndDragDropSource"sv, &EndDragDropSource);
		ctx.set_map_value(m, "BeginDragDropTarget"sv, &BeginDragDropTarget);
		ctx.set_map_value(m, "AcceptDragDropPayload"sv, &AcceptDragDropPayload);
		ctx.set_map_value(m, "EndDragDropTarget"sv, &EndDragDropTarget);
		ctx.set_map_value(m, "GetDragDropPayload"sv, &GetDragDropPayload);
	}
}
