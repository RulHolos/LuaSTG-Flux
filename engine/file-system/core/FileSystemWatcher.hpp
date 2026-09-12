#pragma once
#include "core/ReferenceCounted.hpp"
#include "core/ImmutableString.hpp"
#include <vector>

namespace core {
	enum class FileAction : int32_t {  // NOLINT(performance-enum-size)
		unknown = 0,
		added = 1,
		removed = 2,
		modified = 3,
		renamed_old_name = 4,
		renamed_new_name = 5,
	};

	enum class FileNotifyFilter : uint32_t {  // NOLINT(performance-enum-size)
		file_name = 0x00000001,
		dir_name = 0x00000002,
		attributes = 0x00000004,
		size = 0x00000008,
		last_write = 0x00000010,
		last_access = 0x00000020,
		creation = 0x00000040,
		security = 0x00000100,
		all = file_name | dir_name | attributes | size | last_write | last_access | creation | security,
		legacy_default = file_name | dir_name | size | last_write | creation,
	};
	constexpr FileNotifyFilter operator|(FileNotifyFilter const left, FileNotifyFilter const right) {
		return static_cast<FileNotifyFilter>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
	}
	constexpr FileNotifyFilter operator&(FileNotifyFilter const left, FileNotifyFilter const right) {
		return static_cast<FileNotifyFilter>(static_cast<uint32_t>(left) & static_cast<uint32_t>(right));
	}

	struct FileSystemWatcherOptions {
		FileNotifyFilter filter{ FileNotifyFilter::legacy_default };
		bool recursive{ true };
	};

	struct FileNotifyInformation {
		IImmutableString* file_name{};
		IImmutableString* old_file_name{};
		FileAction action{};

		FileNotifyInformation() = default;
		FileNotifyInformation(FileNotifyInformation const& other) : file_name(other.file_name), old_file_name(other.old_file_name), action(other.action) {
			if (file_name != nullptr) {
				file_name->retain();
			}
			if (old_file_name != nullptr) {
				old_file_name->retain();
			}
		}
		FileNotifyInformation(FileNotifyInformation&& other) noexcept
			: file_name(std::exchange(other.file_name, nullptr))
			, old_file_name(std::exchange(other.old_file_name, nullptr))
			, action(std::exchange(other.action, FileAction::unknown)) {
		}
		~FileNotifyInformation() {
			reset();
		}

		FileNotifyInformation& operator=(FileNotifyInformation const& other) {
			if (this == &other) {
				return *this;
			}

			reset();

			assignFileName(other.file_name);
			assignOldFileName(other.old_file_name);
			action = other.action;

			return *this;
		}
		FileNotifyInformation& operator=(FileNotifyInformation&& other) noexcept {
			if (this == &other) {
				return *this;
			}
			reset();
			std::swap(file_name, other.file_name);
			std::swap(old_file_name, other.old_file_name);
			std::swap(action, other.action);
			return *this;
		}

		void assignFileName(IImmutableString* const new_file_name) {
			if (file_name != nullptr) {
				file_name->release();
				file_name = nullptr;
			}
			this->file_name = new_file_name;
			if (file_name != nullptr) {
				file_name->retain();
			}
		}
		void assignOldFileName(IImmutableString* const new_old_file_name) {
			if (old_file_name != nullptr) {
				old_file_name->release();
				old_file_name = nullptr;
			}
			this->old_file_name = new_old_file_name;
			if (old_file_name != nullptr) {
				old_file_name->retain();
			}
		}
		void reset() {
			if (file_name != nullptr) {
				file_name->release();
				file_name = nullptr;
			}
			if (old_file_name != nullptr) {
				old_file_name->release();
				old_file_name = nullptr;
			}
			action = FileAction::unknown;
		}
	};

	struct CORE_NO_VIRTUAL_TABLE IMessageQueueBasedFileSystemWatcher : IReferenceCounted {
		virtual bool next(FileNotifyInformation* info) = 0;
		virtual size_t drain(std::vector<FileNotifyInformation>* infos) = 0;
		virtual std::string_view getPath() = 0;

		static bool create(std::string_view const& path, IMessageQueueBasedFileSystemWatcher** object);
		static bool create(std::string_view const& path, FileSystemWatcherOptions const& options, IMessageQueueBasedFileSystemWatcher** object);
	};

	// UUID v5
	// ns:URL
	// https://www.luastg-sub.com/core.IMessageQueueBasedFileSystemWatcher
	template<> constexpr InterfaceId getInterfaceId<IMessageQueueBasedFileSystemWatcher>() { return UUID::parse("30b35341-08d1-5144-b3fe-7f96bd29978b"); }
}
