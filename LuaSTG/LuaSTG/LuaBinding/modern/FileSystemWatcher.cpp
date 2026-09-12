#include "FileSystemWatcher.hpp"
#include "core/SmartReference.hpp"
#include "core/FileSystemWatcher.hpp"
#include "lua/plus.hpp"
#include <vector>

using std::string_view_literals::operator ""sv;

namespace luastg::binding {

	std::string_view const FileSystemWatcher::class_name{ "lstg.FileSystemWatcher"sv };

	struct FileSystemWatcherBinding : FileSystemWatcher {

		// meta methods

		// NOLINTBEGIN(*-reserved-identifier)

		static int __gc(lua_State* const vm) {
			if (auto const self = as(vm, 1); self->object != nullptr) {
				self->object->release();
				self->object = nullptr;
			}
			return 0;
		}

		static int __tostring(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const self = as(vm, 1);
			if (self->object != nullptr) {
				ctx.push_value(std::string(class_name) + "(" + std::string(self->object->getPath()) + ")");
			}
			else {
				ctx.push_value(class_name);
			}
			return 1;
		}

		static int __eq(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const self = as(vm, 1);
			if (is(vm, 2)) {
				auto const other = as(vm, 2);
				ctx.push_value(self->object == other->object);
			}
			else {
				ctx.push_value(false);
			}
			return 1;
		}

		// NOLINTEND(*-reserved-identifier)

		// instance methods

		static int next(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const self = as(vm, 1);
			if (self->object == nullptr) {
				ctx.push_value(false);
				return 1;
			}
			if (!ctx.is_table(1 + 1)) {
				return luaL_typerror(vm, 2, "table");
			}

			core::FileNotifyInformation info;
			if (self->object->next(&info)) {
				constexpr lua::stack_index_t info_table(1 + 1);
				ctx.set_map_value(info_table, "file_name"sv, std::string_view(info.file_name->c_str(), info.file_name->length()));
				ctx.set_map_value(info_table, "action"sv, static_cast<int32_t>(info.action));
				ctx.push_value(true);
			}
			else {
				ctx.push_value(false);
			}

			return 1;
		}

		static void pushNotificationTable(lua::stack_t const& ctx, core::FileNotifyInformation const& info) {
			auto const item_table = ctx.create_map(3);
			ctx.set_map_value(item_table, "file_name"sv, std::string_view(info.file_name->c_str(), info.file_name->length()));
			ctx.set_map_value(item_table, "action"sv, static_cast<int32_t>(info.action));
			if (info.old_file_name != nullptr) {
				ctx.set_map_value(item_table, "old_file_name"sv, std::string_view(info.old_file_name->c_str(), info.old_file_name->length()));
			}
		}

		static int poll(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const self = as(vm, 1);
			auto const result_array = ctx.create_array();

			if (self->object != nullptr) {
				std::vector<core::FileNotifyInformation> batch;
				self->object->drain(&batch);

				int32_t index = 1;
				for (auto const& info : batch) {
					pushNotificationTable(ctx, info);
					auto const item_table = ctx.index_of_top();
					ctx.set_array_value(result_array, lua::stack_index_t(index), item_table);
					ctx.pop_value();
					index += 1;
				}
			}

			return 1;
		}

		static int changesIterator(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const self = as(vm, 1);
			if (self->object == nullptr) {
				return 0;
			}

			core::FileNotifyInformation info;
			if (!self->object->next(&info)) {
				return 0;
			}

			ctx.push_value(std::string_view(info.file_name->c_str(), info.file_name->length()));
			ctx.push_value(static_cast<int32_t>(info.action));
			if (info.old_file_name != nullptr) {
				ctx.push_value(std::string_view(info.old_file_name->c_str(), info.old_file_name->length()));
			}
			else {
				ctx.push_value(std::nullopt);
			}
			return 3;
		}

		static int changes(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			ctx.push_value(static_cast<lua_CFunction>(&changesIterator));
			lua_pushvalue(vm, 1);
			ctx.push_value(std::nullopt);
			return 3;
		}

		static int getPath(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const self = as(vm, 1);
			if (self->object == nullptr) {
				ctx.push_value(std::nullopt);
			}
			else {
				ctx.push_value(self->object->getPath());
			}
			return 1;
		}

		static int isValid(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const self = as(vm, 1);
			ctx.push_value(self->object != nullptr);
			return 1;
		}

		static int close(lua_State* const vm) {
			if (auto const self = as(vm, 1); self->object != nullptr) {
				self->object->release();
				self->object = nullptr;
			}
			return 0;
		}

		// static methods

		static int create(lua_State* const vm) {
			lua::stack_t const ctx(vm);
			auto const path = ctx.get_value<std::string_view>(1);

			core::FileSystemWatcherOptions options;
			if (ctx.has_value(2) && !ctx.is_nil(2)) {
				if (!ctx.is_table(2)) {
					return luaL_typerror(vm, 2, "table");
				}
				constexpr lua::stack_index_t options_table(2);
				options.recursive = ctx.get_map_value(options_table, "recursive"sv, true);
				auto const filter_bits = ctx.get_map_value(options_table, "filter"sv, static_cast<int32_t>(core::FileNotifyFilter::legacy_default));
				options.filter = static_cast<core::FileNotifyFilter>(filter_bits);
			}

			core::SmartReference<core::IMessageQueueBasedFileSystemWatcher> object;
			if (!core::IMessageQueueBasedFileSystemWatcher::create(path, options, object.put())) {
				ctx.push_value(std::nullopt);
				return 1;
			}

			auto const self = FileSystemWatcher::create(vm);
			self->object = object.detach();
			return 1;
		}

	};

	bool FileSystemWatcher::is(lua_State* const vm, int const index) {
		return nullptr != luaL_testudata(vm, index, class_name.data());
	}

	FileSystemWatcher* FileSystemWatcher::as(lua_State* const vm, int const index) {
		return static_cast<FileSystemWatcher*>(luaL_checkudata(vm, index, class_name.data()));
	}

	FileSystemWatcher* FileSystemWatcher::create(lua_State* const vm) {
		lua::stack_t const ctx(vm);
		auto const self = ctx.create_userdata<FileSystemWatcher>();
		auto const self_index = ctx.index_of_top();
		ctx.set_metatable(self_index, class_name);
		self->object = nullptr;
		return self;
	}

	void FileSystemWatcher::registerClass(lua_State* const vm) {
		lua::stack_balancer_t const sb(vm);
		lua::stack_t const ctx(vm);

		// lstg.FileSystemWatcher.FileAction
		auto const action_table = ctx.create_module("lstg.FileSystemWatcher.FileAction"sv);
		ctx.set_map_value(action_table, "added", static_cast<int32_t>(core::FileAction::added));
		ctx.set_map_value(action_table, "removed", static_cast<int32_t>(core::FileAction::removed));
		ctx.set_map_value(action_table, "modified", static_cast<int32_t>(core::FileAction::modified));
		ctx.set_map_value(action_table, "renamed_old_name", static_cast<int32_t>(core::FileAction::renamed_old_name));
		ctx.set_map_value(action_table, "renamed_new_name", static_cast<int32_t>(core::FileAction::renamed_new_name));

		// lstg.FileSystemWatcher.NotifyFilter
		auto const filter_table = ctx.create_module("lstg.FileSystemWatcher.NotifyFilter"sv);
		ctx.set_map_value(filter_table, "file_name", static_cast<int32_t>(core::FileNotifyFilter::file_name));
		ctx.set_map_value(filter_table, "dir_name", static_cast<int32_t>(core::FileNotifyFilter::dir_name));
		ctx.set_map_value(filter_table, "attributes", static_cast<int32_t>(core::FileNotifyFilter::attributes));
		ctx.set_map_value(filter_table, "size", static_cast<int32_t>(core::FileNotifyFilter::size));
		ctx.set_map_value(filter_table, "last_write", static_cast<int32_t>(core::FileNotifyFilter::last_write));
		ctx.set_map_value(filter_table, "last_access", static_cast<int32_t>(core::FileNotifyFilter::last_access));
		ctx.set_map_value(filter_table, "creation", static_cast<int32_t>(core::FileNotifyFilter::creation));
		ctx.set_map_value(filter_table, "security", static_cast<int32_t>(core::FileNotifyFilter::security));
		ctx.set_map_value(filter_table, "all", static_cast<int32_t>(core::FileNotifyFilter::all));

		// method

		auto const method_table = ctx.create_module(class_name);
		ctx.set_map_value(method_table, "read", &FileSystemWatcherBinding::next);
		ctx.set_map_value(method_table, "poll", &FileSystemWatcherBinding::poll);
		ctx.set_map_value(method_table, "changes", &FileSystemWatcherBinding::changes);
		ctx.set_map_value(method_table, "getPath", &FileSystemWatcherBinding::getPath);
		ctx.set_map_value(method_table, "isValid", &FileSystemWatcherBinding::isValid);
		ctx.set_map_value(method_table, "close", &FileSystemWatcherBinding::close);
		ctx.set_map_value(method_table, "create", &FileSystemWatcherBinding::create);

		// metatable

		auto const metatable = ctx.create_metatable(class_name);
		ctx.set_map_value(metatable, "__gc", &FileSystemWatcherBinding::__gc);
		ctx.set_map_value(metatable, "__tostring", &FileSystemWatcherBinding::__tostring);
		ctx.set_map_value(metatable, "__eq", &FileSystemWatcherBinding::__eq);
		ctx.set_map_value(metatable, "__index", method_table);
	}

}
