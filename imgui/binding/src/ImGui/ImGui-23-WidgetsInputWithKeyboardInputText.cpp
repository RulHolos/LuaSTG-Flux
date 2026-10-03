#include "lua_imgui_binding.hpp"
#include "lua/plus.hpp"
#include <cstring>
#include <limits>

using std::string_view_literals::operator ""sv;

namespace {
	struct ImGuiInputTextCallbackDataBinding {
		static constexpr auto class_name{ "imgui.ImGuiInputTextCallbackData"sv };

		static ImGuiInputTextCallbackData** as(lua_State* const vm, int const index) {
			lua::stack_t const ctx(vm);
			return ctx.as_userdata<ImGuiInputTextCallbackData*>(index, class_name);
		}
		static void reference(lua_State* const vm, ImGuiInputTextCallbackData* const data) {
			lua::stack_t const ctx(vm);
			auto const self = ctx.create_userdata<ImGuiInputTextCallbackData*>();
			ctx.set_metatable(ctx.index_of_top(), class_name);
			*self = data;
		}
		static int toString(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			[[maybe_unused]] auto const self = as(vm, 1);
			ctx.push_value(class_name);
			return 1;
		}
		static int getter(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const data = *as(vm, 1);
			auto const key = ctx.get_value<std::string_view>(2);

		#define GET_FIELD(name) if (key == (#name ""sv)) { ctx.push_value(data->name); return 1; } (void)0
			GET_FIELD(EventFlag);
			GET_FIELD(Flags);
			GET_FIELD(EventChar);
			GET_FIELD(EventKey);
			GET_FIELD(BufTextLen);
			GET_FIELD(BufSize);
			GET_FIELD(BufDirty);
			GET_FIELD(CursorPos);
			GET_FIELD(SelectionStart);
			GET_FIELD(SelectionEnd);
		#undef GET_FIELD

			if (key == "Buf"sv) {
				ctx.push_value(std::string_view(data->Buf, static_cast<size_t>(data->BufTextLen)));
				return 1;
			}
			if (key == "UserData"sv) {
				lua_pushnil(vm);
				return 1;
			}
			luaL_getmetatable(vm, class_name.data());
			lua_getfield(vm, -1, "__methods");
			lua_pushvalue(vm, 2);
			lua_gettable(vm, -2);
			return 1;
		}
		static int setter(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const data = *as(vm, 1);
			auto const key = ctx.get_value<std::string_view>(2);

		#define SET_FIELD(name, type) if (key == (#name ""sv)) { data->name = ctx.get_value<type>(3); return 0; } (void)0
			SET_FIELD(EventChar, ImWchar);
			SET_FIELD(BufTextLen, int);
			SET_FIELD(BufDirty, bool);
			SET_FIELD(CursorPos, int);
			SET_FIELD(SelectionStart, int);
			SET_FIELD(SelectionEnd, int);
		#undef SET_FIELD

			if (key == "Buf"sv) {
				if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
					return luaL_error(vm, "Buf cannot be changed during a resize callback");
				auto const text = ctx.get_value<std::string_view>(3);
				data->DeleteChars(0, data->BufTextLen);
				data->InsertChars(0, text.data(), text.data() + text.size());
				return 0;
			}
			return luaL_error(vm, "field '%s' is read-only or does not exist", key.data());
		}
		static int deleteChars(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const data = *as(vm, 1);
			if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
				return luaL_error(vm, "text cannot be edited during a resize callback");
			data->DeleteChars(ctx.get_value<int>(2), ctx.get_value<int>(3));
			return 0;
		}
		static int insertChars(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const data = *as(vm, 1);
			if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
				return luaL_error(vm, "text cannot be edited during a resize callback");
			auto const pos = ctx.get_value<int>(2);
			auto const text = ctx.get_value<std::string_view>(3);
			data->InsertChars(pos, text.data(), text.data() + text.size());
			return 0;
		}
		static int selectAll(lua_State* const vm) {
			(*as(vm, 1))->SelectAll();
			return 0;
		}
		static int clearSelection(lua_State* const vm) {
			(*as(vm, 1))->ClearSelection();
			return 0;
		}
		static int hasSelection(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			ctx.push_value((*as(vm, 1))->HasSelection());
			return 1;
		}
		static void registerClass(lua_State* const vm) {
			lua::stack_balancer_t const sb(vm);
			lua::stack_t const ctx(vm);

			auto const methods = ctx.create_module(class_name);
			ctx.set_map_value(methods, "DeleteChars"sv, &deleteChars);
			ctx.set_map_value(methods, "InsertChars"sv, &insertChars);
			ctx.set_map_value(methods, "SelectAll"sv, &selectAll);
			ctx.set_map_value(methods, "ClearSelection"sv, &clearSelection);
			ctx.set_map_value(methods, "HasSelection"sv, &hasSelection);

			auto const mt = ctx.create_metatable(class_name);
			ctx.set_map_value(mt, "__methods"sv, methods);
			ctx.set_map_value(mt, "__tostring"sv, &toString);
			ctx.set_map_value(mt, "__index"sv, &getter);
			ctx.set_map_value(mt, "__newindex"sv, &setter);
		}
	};

	struct InputTextCallbackContext {
		lua_State* vm;
		int callback_index;
		ImGuiInputTextFlags user_flags;
		ImGuiTextBuffer* buffer;
		bool failed{};
	};

	int inputTextCallback(ImGuiInputTextCallbackData* const data) {
		auto const context = static_cast<InputTextCallbackContext*>(data->UserData);
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
			context->buffer->reserve(data->BufSize);
			data->Buf = const_cast<char*>(context->buffer->c_str());
		}
		if (context->callback_index == 0 || context->failed)
			return 0;
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize && !(context->user_flags & ImGuiInputTextFlags_CallbackResize))
			return 0;

		ImGuiInputTextCallbackDataBinding::reference(context->vm, data);
		lua_pushvalue(context->vm, context->callback_index);
		lua_insert(context->vm, -2);
		if (lua_pcall(context->vm, 1, 1, 0) != 0) {
			context->failed = true;
			return 0;
		}
		auto const result = static_cast<int>(lua_tointeger(context->vm, -1));
		lua_pop(context->vm, 1);
		return result;
	}

	bool isLuaFunction(lua_State* const vm, int const index) {
		if (lua_isnoneornil(vm, index))
			return false;
		if (!lua_isfunction(vm, index))
			luaL_typerror(vm, index, "function");
		return true;
	}
	size_t getBufferCapacity(lua_State* const vm, lua::stack_t const& ctx, int const index, ImGuiTextBuffer* const buffer) {
		auto const minimum_capacity = static_cast<size_t>(buffer->Buf.Size > 0 ? buffer->Buf.Size : 1);
		auto const capacity = ctx.get_value<size_t>(index, minimum_capacity);
		if (capacity > static_cast<size_t>(std::numeric_limits<int>::max())) {
			luaL_argerror(vm, index, "buffer size is out of range");
			return 0;
		}
		return capacity < minimum_capacity ? minimum_capacity : capacity;
	}

	void prepareTextBuffer(ImGuiTextBuffer* const buffer, size_t const capacity) {
		if (buffer->Buf.Size == 0)
			buffer->Buf.resize(1, 0);
		buffer->reserve(static_cast<int>(capacity));
	}

	void finishTextBuffer(ImGuiTextBuffer* const buffer) {
		buffer->Buf.Size = static_cast<int>(std::strlen(buffer->c_str())) + 1;
	}

	int InputText(lua_State* const vm) {
		lua::stack_t const ctx(vm);
		auto const label = ctx.get_value<std::string_view>(1);
		auto const buf = imgui::binding::ImGuiTextBufferBinding::as(vm, 2);
		auto const buf_size = getBufferCapacity(vm, ctx, 3, buf);
		auto const flags = ctx.get_value<ImGuiInputTextFlags>(4, 0);
		auto const has_callback = isLuaFunction(vm, 5);
		prepareTextBuffer(buf, buf_size);
		InputTextCallbackContext callback_context{ vm, has_callback ? 5 : 0, flags, buf };
		auto const result = ImGui::InputText(label.data(), const_cast<char*>(buf->c_str()), static_cast<int>(buf_size), flags | ImGuiInputTextFlags_CallbackResize, &inputTextCallback, &callback_context);
		finishTextBuffer(buf);
		if (callback_context.failed)
			return lua_error(vm);
		ctx.push_value(result);
		return 1;
	}
	int InputTextMultiline(lua_State* const vm) {
		lua::stack_t const ctx(vm);
		auto const label = ctx.get_value<std::string_view>(1);
		auto const buf = imgui::binding::ImGuiTextBufferBinding::as(vm, 2);
		auto const buf_size = getBufferCapacity(vm, ctx, 3, buf);
		auto const size = imgui::binding::ImVec2Binding::as(vm, 4, ImVec2());
		auto const flags = ctx.get_value<ImGuiInputTextFlags>(5, 0);
		auto const has_callback = isLuaFunction(vm, 6);
		prepareTextBuffer(buf, buf_size);
		InputTextCallbackContext callback_context{ vm, has_callback ? 6 : 0, flags, buf };
		auto const result = ImGui::InputTextMultiline(label.data(), const_cast<char*>(buf->c_str()), static_cast<int>(buf_size), *size, flags | ImGuiInputTextFlags_CallbackResize, &inputTextCallback, &callback_context);
		finishTextBuffer(buf);
		if (callback_context.failed)
			return lua_error(vm);
		ctx.push_value(result);
		return 1;
	}
	int InputTextWithHint(lua_State* const vm) {
		lua::stack_t const ctx(vm);
		auto const label = ctx.get_value<std::string_view>(1);
		auto const hint = ctx.get_value<std::string_view>(2);
		auto const buf = imgui::binding::ImGuiTextBufferBinding::as(vm, 3);
		auto const buf_size = getBufferCapacity(vm, ctx, 4, buf);
		auto const flags = ctx.get_value<ImGuiInputTextFlags>(5, 0);
		auto const has_callback = isLuaFunction(vm, 6);
		prepareTextBuffer(buf, buf_size);
		InputTextCallbackContext callback_context{ vm, has_callback ? 6 : 0, flags, buf };
		auto const result = ImGui::InputTextWithHint(label.data(), hint.data(), const_cast<char*>(buf->c_str()), static_cast<int>(buf_size), flags | ImGuiInputTextFlags_CallbackResize, &inputTextCallback, &callback_context);
		finishTextBuffer(buf);
		if (callback_context.failed)
			return lua_error(vm);
		ctx.push_value(result);
		return 1;
	}
}

namespace imgui::binding {
	void registerImGuiWidgetsInputWithKeyboardInputText(lua_State* const vm) {
		lua::stack_balancer_t const sb(vm);
		lua::stack_t const ctx(vm);
		ImGuiInputTextCallbackDataBinding::registerClass(vm);

		auto const m = ctx.push_module(module_ImGui_name);
		ctx.set_map_value(m, "InputText"sv, &InputText);
		ctx.set_map_value(m, "InputTextMultiline"sv, &InputTextMultiline);
		ctx.set_map_value(m, "InputTextWithHint"sv, &InputTextWithHint);
	}
}
