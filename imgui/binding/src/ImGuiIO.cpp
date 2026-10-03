#include "lua_imgui_binding.hpp"
#include "lua/plus.hpp"

using std::string_view_literals::operator ""sv;

namespace imgui::binding {
	std::string_view const ImGuiIOBinding::class_name{ "imgui.ImGuiIO"sv };

	struct ImGuiIOWrapper {
		static int toString(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			[[maybe_unused]] auto const self = ImGuiIOBinding::as(vm, 1);
			ctx.push_value(ImGuiIOBinding::class_name);
			return 1;
		}
		static int getter(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const self = ImGuiIOBinding::as(vm, 1);
			auto const key = ctx.get_value<std::string_view>(2);

		#define GET_FLAG(name) if (key == (#name ""sv)) { ctx.push_value(static_cast<int32_t>(self->name)); return 1; } (void)0
		#define GET_SCALAR(name) if (key == (#name ""sv)) { ctx.push_value(self->name); return 1; } (void)0
		#define GET_VEC2(name) if (key == (#name ""sv)) { ImVec2Binding::create(vm, self->name); return 1; } (void)0
		#define GET_METHOD(name) if (key == (#name ""sv)) { ctx.push_value(&ImGuiIOWrapper::name); return 1; } (void)0

			GET_FLAG(ConfigFlags);
			GET_FLAG(BackendFlags);
			GET_VEC2(DisplaySize);
			GET_VEC2(DisplayFramebufferScale);
			GET_SCALAR(DeltaTime);
			GET_SCALAR(IniSavingRate);
			GET_SCALAR(ConfigNavSwapGamepadButtons);
			GET_SCALAR(ConfigNavMoveSetMousePos);
			GET_SCALAR(ConfigNavCaptureKeyboard);
			GET_SCALAR(ConfigNavEscapeClearFocusItem);
			GET_SCALAR(ConfigNavEscapeClearFocusWindow);
			GET_SCALAR(ConfigNavCursorVisibleAuto);
			GET_SCALAR(ConfigNavCursorVisibleAlways);
			GET_SCALAR(MouseDrawCursor);
			GET_SCALAR(ConfigMacOSXBehaviors);
			GET_SCALAR(ConfigInputTrickleEventQueue);
			GET_SCALAR(ConfigInputTextCursorBlink);
			GET_SCALAR(ConfigInputTextEnterKeepActive);
			GET_SCALAR(ConfigDragClickToInputText);
			GET_SCALAR(ConfigWindowsResizeFromEdges);
			GET_SCALAR(ConfigWindowsMoveFromTitleBarOnly);
			GET_SCALAR(ConfigWindowsCopyContentsWithCtrlC);
			GET_SCALAR(ConfigScrollbarScrollByPage);
			GET_SCALAR(ConfigMemoryCompactTimer);
			GET_SCALAR(MouseDoubleClickTime);
			GET_SCALAR(MouseDoubleClickMaxDist);
			GET_SCALAR(MouseDragThreshold);
			GET_SCALAR(KeyRepeatDelay);
			GET_SCALAR(KeyRepeatRate);
			GET_SCALAR(ConfigErrorRecovery);
			GET_SCALAR(ConfigErrorRecoveryEnableAssert);
			GET_SCALAR(ConfigErrorRecoveryEnableDebugLog);
			GET_SCALAR(ConfigErrorRecoveryEnableTooltip);
			GET_SCALAR(ConfigDebugIsDebuggerPresent);
			GET_SCALAR(ConfigDebugHighlightIdConflicts);
			GET_SCALAR(ConfigDebugHighlightIdConflictsShowItemPicker);
			GET_SCALAR(ConfigDebugBeginReturnValueOnce);
			GET_SCALAR(ConfigDebugBeginReturnValueLoop);
			GET_SCALAR(ConfigDebugIgnoreFocusLoss);
			GET_SCALAR(ConfigDebugIniSettings);

			GET_SCALAR(WantCaptureMouse);
			GET_SCALAR(WantCaptureKeyboard);
			GET_SCALAR(WantTextInput);
			GET_SCALAR(WantSetMousePos);
			GET_SCALAR(WantSaveIniSettings);
			GET_SCALAR(NavActive);
			GET_SCALAR(NavVisible);
			GET_SCALAR(Framerate);
			GET_SCALAR(MetricsRenderVertices);
			GET_SCALAR(MetricsRenderIndices);
			GET_SCALAR(MetricsRenderWindows);
			GET_SCALAR(MetricsActiveWindows);
			GET_VEC2(MouseDelta);
			GET_VEC2(MousePos);
			GET_SCALAR(MouseWheel);
			GET_SCALAR(MouseWheelH);
			GET_FLAG(MouseSource);
			GET_SCALAR(KeyCtrl);
			GET_SCALAR(KeyShift);
			GET_SCALAR(KeyAlt);
			GET_SCALAR(KeySuper);
			GET_FLAG(KeyMods);

			GET_METHOD(AddKeyEvent);
			GET_METHOD(AddKeyAnalogEvent);
			GET_METHOD(AddMousePosEvent);
			GET_METHOD(AddMouseButtonEvent);
			GET_METHOD(AddMouseWheelEvent);
			GET_METHOD(AddMouseSourceEvent);
			GET_METHOD(AddFocusEvent);
			GET_METHOD(AddInputCharacter);
			GET_METHOD(AddInputCharacterUTF16);
			GET_METHOD(AddInputCharactersUTF8);
			GET_METHOD(SetKeyEventNativeData);
			GET_METHOD(SetAppAcceptingEvents);
			GET_METHOD(ClearEventsQueue);
			GET_METHOD(ClearInputKeys);
			GET_METHOD(ClearInputMouse);

		#undef GET_FLAG
		#undef GET_SCALAR
		#undef GET_VEC2
		#undef GET_METHOD
			return luaL_error(vm, "field '%s' does not exist", key.data());
		}
		static int setter(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const self = ImGuiIOBinding::as(vm, 1);
			auto const key = ctx.get_value<std::string_view>(2);

		#define SET_FLAG(name) if (key == (#name ""sv)) { self->name = static_cast<decltype(self->name)>(ctx.get_value<int32_t>(3)); return 0; } (void)0
		#define SET_SCALAR(name) if (key == (#name ""sv)) { self->name = ctx.get_value<decltype(self->name)>(3); return 0; } (void)0
		#define SET_VEC2(name) if (key == (#name ""sv)) { self->name = *ImVec2Binding::as(vm, 3); return 0; } (void)0

			SET_FLAG(ConfigFlags);
			SET_FLAG(BackendFlags);
			SET_VEC2(DisplaySize);
			SET_VEC2(DisplayFramebufferScale);
			SET_SCALAR(DeltaTime);
			SET_SCALAR(IniSavingRate);
			SET_SCALAR(ConfigNavSwapGamepadButtons);
			SET_SCALAR(ConfigNavMoveSetMousePos);
			SET_SCALAR(ConfigNavCaptureKeyboard);
			SET_SCALAR(ConfigNavEscapeClearFocusItem);
			SET_SCALAR(ConfigNavEscapeClearFocusWindow);
			SET_SCALAR(ConfigNavCursorVisibleAuto);
			SET_SCALAR(ConfigNavCursorVisibleAlways);
			SET_SCALAR(MouseDrawCursor);
			SET_SCALAR(ConfigMacOSXBehaviors);
			SET_SCALAR(ConfigInputTrickleEventQueue);
			SET_SCALAR(ConfigInputTextCursorBlink);
			SET_SCALAR(ConfigInputTextEnterKeepActive);
			SET_SCALAR(ConfigDragClickToInputText);
			SET_SCALAR(ConfigWindowsResizeFromEdges);
			SET_SCALAR(ConfigWindowsMoveFromTitleBarOnly);
			SET_SCALAR(ConfigWindowsCopyContentsWithCtrlC);
			SET_SCALAR(ConfigScrollbarScrollByPage);
			SET_SCALAR(ConfigMemoryCompactTimer);
			SET_SCALAR(MouseDoubleClickTime);
			SET_SCALAR(MouseDoubleClickMaxDist);
			SET_SCALAR(MouseDragThreshold);
			SET_SCALAR(KeyRepeatDelay);
			SET_SCALAR(KeyRepeatRate);
			SET_SCALAR(ConfigErrorRecovery);
			SET_SCALAR(ConfigErrorRecoveryEnableAssert);
			SET_SCALAR(ConfigErrorRecoveryEnableDebugLog);
			SET_SCALAR(ConfigErrorRecoveryEnableTooltip);
			SET_SCALAR(ConfigDebugIsDebuggerPresent);
			SET_SCALAR(ConfigDebugHighlightIdConflicts);
			SET_SCALAR(ConfigDebugHighlightIdConflictsShowItemPicker);
			SET_SCALAR(ConfigDebugBeginReturnValueOnce);
			SET_SCALAR(ConfigDebugBeginReturnValueLoop);
			SET_SCALAR(ConfigDebugIgnoreFocusLoss);
			SET_SCALAR(ConfigDebugIniSettings);

		#undef SET_FLAG
		#undef SET_SCALAR
		#undef SET_VEC2
			return luaL_error(vm, "field '%s' is read-only or does not exist", key.data());
		}

		static int AddKeyEvent(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			ImGuiIOBinding::as(vm, 1)->AddKeyEvent(ctx.get_value<ImGuiKey>(2), ctx.get_value<bool>(3));
			return 0;
		}
		static int AddKeyAnalogEvent(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			ImGuiIOBinding::as(vm, 1)->AddKeyAnalogEvent(ctx.get_value<ImGuiKey>(2), ctx.get_value<bool>(3), ctx.get_value<float>(4));
			return 0;
		}
		static int AddMousePosEvent(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			ImGuiIOBinding::as(vm, 1)->AddMousePosEvent(ctx.get_value<float>(2), ctx.get_value<float>(3));
			return 0;
		}
		static int AddMouseButtonEvent(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			ImGuiIOBinding::as(vm, 1)->AddMouseButtonEvent(ctx.get_value<int>(2), ctx.get_value<bool>(3));
			return 0;
		}
		static int AddMouseWheelEvent(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			ImGuiIOBinding::as(vm, 1)->AddMouseWheelEvent(ctx.get_value<float>(2), ctx.get_value<float>(3));
			return 0;
		}
		static int AddMouseSourceEvent(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			ImGuiIOBinding::as(vm, 1)->AddMouseSourceEvent(ctx.get_value<ImGuiMouseSource>(2));
			return 0;
		}
		static int AddFocusEvent(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			ImGuiIOBinding::as(vm, 1)->AddFocusEvent(ctx.get_value<bool>(2));
			return 0;
		}
		static int AddInputCharacter(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			ImGuiIOBinding::as(vm, 1)->AddInputCharacter(ctx.get_value<unsigned int>(2));
			return 0;
		}
		static int AddInputCharacterUTF16(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			ImGuiIOBinding::as(vm, 1)->AddInputCharacterUTF16(ctx.get_value<ImWchar16>(2));
			return 0;
		}
		static int AddInputCharactersUTF8(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const text = ctx.get_value<std::string_view>(2);
			ImGuiIOBinding::as(vm, 1)->AddInputCharactersUTF8(text.data());
			return 0;
		}
		static int SetKeyEventNativeData(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			ImGuiIOBinding::as(vm, 1)->SetKeyEventNativeData(ctx.get_value<ImGuiKey>(2), ctx.get_value<int>(3), ctx.get_value<int>(4), ctx.get_value<int>(5, -1));
			return 0;
		}
		static int SetAppAcceptingEvents(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			ImGuiIOBinding::as(vm, 1)->SetAppAcceptingEvents(ctx.get_value<bool>(2));
			return 0;
		}
		static int ClearEventsQueue(lua_State* const vm) {
			ImGuiIOBinding::as(vm, 1)->ClearEventsQueue();
			return 0;
		}
		static int ClearInputKeys(lua_State* const vm) {
			ImGuiIOBinding::as(vm, 1)->ClearInputKeys();
			return 0;
		}
		static int ClearInputMouse(lua_State* const vm) {
			ImGuiIOBinding::as(vm, 1)->ClearInputMouse();
			return 0;
		}
	};

	bool ImGuiIOBinding::is(lua_State* const vm, int const index) {
		lua::stack_t const ctx(vm);
		return ctx.is_metatable(index, class_name);
	}
	ImGuiIO* ImGuiIOBinding::as(lua_State* const vm, int const index) {
		lua::stack_t const ctx(vm);
		return ctx.as_userdata<ImGuiIOBinding>(index)->data;
	}
	ImGuiIO* ImGuiIOBinding::reference(lua_State* const vm, ImGuiIO* const value) {
		lua::stack_t const ctx(vm);
		auto const self = ctx.create_userdata<ImGuiIOBinding>();
		ctx.set_metatable(ctx.index_of_top(), class_name);
		self->data = value;
		return value;
	}
	void ImGuiIOBinding::registerClass(lua_State* const vm) {
		lua::stack_balancer_t const sb(vm);
		lua::stack_t const ctx(vm);
		auto const mt = ctx.create_metatable(class_name);
		ctx.set_map_value(mt, "__tostring"sv, &ImGuiIOWrapper::toString);
		ctx.set_map_value(mt, "__index"sv, &ImGuiIOWrapper::getter);
		ctx.set_map_value(mt, "__newindex"sv, &ImGuiIOWrapper::setter);
	}
}