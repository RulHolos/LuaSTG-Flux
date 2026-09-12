#include "core/FileSystemWatcher.hpp"
#include "core/SmartReference.hpp"
#include "core/implement/ReferenceCounted.hpp"
#include "core/Logger.hpp"
#include "core/FileSystemCommon.hpp"
#include "utf8.hpp"
#include <cassert>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <list>
#include <vector>
#include <windows.h>
#include <wil/resource.h>

namespace core {
	class MessageQueueBasedFileSystemWatcher final : public implement::ReferenceCounted<IMessageQueueBasedFileSystemWatcher> {
	public:
		// IMessageQueueBasedFileSystemWatcher

		bool next(FileNotifyInformation* info) override {
			assert(info != nullptr);
			if (info == nullptr) {
				return false;
			}
			std::lock_guard notify_lock(m_notify_mutex);
			if (m_notify.empty()) {
				return false;
			}
			*info = std::move(m_notify.front());
			m_notify.pop_front();
			return true;
		}

		size_t drain(std::vector<FileNotifyInformation>* infos) override {
			assert(infos != nullptr);
			if (infos == nullptr) {
				return 0;
			}
			std::lock_guard notify_lock(m_notify_mutex);
			size_t const count = m_notify.size();
			if (count == 0) {
				return 0;
			}
			infos->reserve(infos->size() + count);
			for (auto& info : m_notify) {
				infos->emplace_back(std::move(info));
			}
			m_notify.clear();
			return count;
		}

		std::string_view getPath() override {
			return m_path;
		}

		// MessageQueueBasedFileSystemWatcher

		MessageQueueBasedFileSystemWatcher() = default;
		MessageQueueBasedFileSystemWatcher(MessageQueueBasedFileSystemWatcher const&) = delete;
		MessageQueueBasedFileSystemWatcher(MessageQueueBasedFileSystemWatcher&&) = delete;
		~MessageQueueBasedFileSystemWatcher() override {
			SetEvent(m_exit_event.get());
			if (m_worker.joinable()) {
				m_worker.join();
			}
		}

		MessageQueueBasedFileSystemWatcher& operator=(MessageQueueBasedFileSystemWatcher const&) = delete;
		MessageQueueBasedFileSystemWatcher& operator=(MessageQueueBasedFileSystemWatcher&&) = delete;

		bool open(std::string_view const& path, FileSystemWatcherOptions const& options) {
			m_exit_event.reset(CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS));
			if (!m_exit_event.is_valid()) {
				return false;
			}

			m_complete_event.reset(CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS));
			if (!m_complete_event.is_valid()) {
				return false;
			}

			auto const path_w = utf8::to_wstring(path);
			m_file.reset(CreateFileW(
				path_w.c_str(),
				FILE_LIST_DIRECTORY | GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr,
				OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
				nullptr
			));
			if (!m_file.is_valid()) {
				return false;
			}

			m_path = getStringView(normalizePath(path));
			m_notify_filter = static_cast<DWORD>(options.filter);
			m_recursive = options.recursive;

			m_worker = std::thread(&worker, this);
			return true;
		}

		static void worker(MessageQueueBasedFileSystemWatcher* self) {
			static constexpr size_t buffer_size = 65536;
			auto const buffer = std::make_unique<uint8_t[]>(buffer_size);
			IImmutableString* pending_old_name{};

			auto const release_pending_old_name = [&pending_old_name] {
				if (pending_old_name != nullptr) {
					pending_old_name->release();
					pending_old_name = nullptr;
				}
			};

			for (;;) {
				std::memset(buffer.get(), 0, buffer_size);
				if (!ResetEvent(self->m_complete_event.get())) {
					Logger::error("core::MessageQueueBasedFileSystemWatcher::worker (ResetEvent)");
					release_pending_old_name();
					return;
				}

				OVERLAPPED overlapped{};
				overlapped.hEvent = self->m_complete_event.get();
				if (!ReadDirectoryChangesW(
					self->m_file.get(),
					buffer.get(),
					static_cast<DWORD>(buffer_size),
					self->m_recursive ? TRUE : FALSE,
					self->m_notify_filter,
					nullptr,
					&overlapped,
					nullptr
				)) {
					Logger::error("core::MessageQueueBasedFileSystemWatcher::worker (ReadDirectoryChangesW)");
					release_pending_old_name();
					return;
				}

				HANDLE const wait_events[2]{ self->m_exit_event.get() , self->m_complete_event.get() };
				DWORD const wait_result = WaitForMultipleObjects(2, wait_events, FALSE, INFINITE);
				if (wait_result == WAIT_FAILED || wait_result == WAIT_TIMEOUT || wait_result == WAIT_ABANDONED) {
					Logger::error("core::MessageQueueBasedFileSystemWatcher::worker (WaitForMultipleObjects: WAIT_FAILED|WAIT_TIMEOUT|WAIT_ABANDONED)");
					release_pending_old_name();
					return;
				}
				if (wait_result == WAIT_OBJECT_0) {
					release_pending_old_name();
					return;
				}
				if (wait_result != (WAIT_OBJECT_0 + 1)) {
					Logger::error("core::MessageQueueBasedFileSystemWatcher::worker (WaitForMultipleObjects: UNKNOWN)");
					release_pending_old_name();
					return;
				}

				DWORD transferred_bytes{};
				if (!GetOverlappedResult(self->m_file.get(), &overlapped, &transferred_bytes, TRUE)) {
					Logger::error("core::MessageQueueBasedFileSystemWatcher::worker (GetOverlappedResult)");
					release_pending_old_name();
					return;
				}

				std::vector<FileNotifyInformation> batch;

				auto const begin = buffer.get();
				auto const end = begin + transferred_bytes;
				auto ptr = begin;
				while (ptr < end) {
					auto const cur = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(ptr);
					ptr += cur->NextEntryOffset;

					FileNotifyInformation info{};
					auto const file_name = utf8::to_string(std::wstring_view(cur->FileName));
					auto const normalized = normalizePath(file_name);
					IImmutableString::create(getStringView(normalized), &info.file_name);
					info.action = static_cast<FileAction>(cur->Action);

					if (info.action == FileAction::renamed_old_name) {
						release_pending_old_name();
						pending_old_name = info.file_name;
						pending_old_name->retain();
					}
					else if (info.action == FileAction::renamed_new_name && pending_old_name != nullptr) {
						info.assignOldFileName(pending_old_name);
						release_pending_old_name();
					}

					batch.emplace_back(std::move(info));

					if (cur->NextEntryOffset == 0) {
						break;
					}
				}

				if (!batch.empty()) {
					std::lock_guard notify_lock(self->m_notify_mutex);
					for (auto& info : batch) {
						self->m_notify.emplace_back(std::move(info));
					}
				}
			}
		}

	private:
		wil::unique_event m_exit_event;
		wil::unique_event m_complete_event;
		wil::unique_hfile m_file;
		std::thread m_worker;
		std::list<FileNotifyInformation> m_notify;
		std::mutex m_notify_mutex;
		DWORD m_notify_filter{};
		bool m_recursive{ true };
		std::string m_path;
	};

	bool IMessageQueueBasedFileSystemWatcher::create(std::string_view const& path, IMessageQueueBasedFileSystemWatcher** const object) {
		return create(path, FileSystemWatcherOptions{}, object);
	}

	bool IMessageQueueBasedFileSystemWatcher::create(std::string_view const& path, FileSystemWatcherOptions const& options, IMessageQueueBasedFileSystemWatcher** const object) {
		assert(object != nullptr);
		if (object == nullptr) {
			return false;
		}
		SmartReference<MessageQueueBasedFileSystemWatcher> temp;
		temp.attach(new MessageQueueBasedFileSystemWatcher);
		if (!temp->open(path, options)) {
			return false;
		}
		*object = temp.detach();
		return true;
	}
}
