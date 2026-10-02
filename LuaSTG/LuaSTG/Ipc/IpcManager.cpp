#include "Ipc/IpcManager.hpp"
#include "utf8.hpp"
#include <spdlog/spdlog.h>
#include <windows.h>
#include <wil/resource.h>

namespace luastg
{

    namespace
    {
        constexpr size_t kPipeBufferSize = 65536;

        std::wstring makePipePath(std::string_view const name)
        {
            return LR"(\\.\pipe\)" + utf8::to_wstring(name);
        }

        wil::unique_event createManualResetEvent()
        {
            return wil::unique_event(CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS));
        }
    }

    IpcManager &IpcManager::GetInstance() noexcept
    {
        static IpcManager instance;
        return instance;
    }

    IpcManager::~IpcManager()
    {
        stop();
    }

    bool IpcManager::start(std::string_view const pipe_name)
    {
        if (m_running.load())
        {
            stop();
        }

        auto stop_event = createManualResetEvent();
        if (!stop_event.is_valid())
        {
            spdlog::error("[ipc] IpcManager::start (CreateEventExW)");
            return false;
        }
        m_stop_event = stop_event.release();

        m_running.store(true);
        m_accept_thread = std::thread(&IpcManager::acceptLoop, this, makePipePath(pipe_name));
        return true;
    }

    void IpcManager::stop()
    {
        if (!m_running.exchange(false))
        {
            return;
        }

        if (m_stop_event)
        {
            SetEvent(static_cast<HANDLE>(m_stop_event));
        }
        if (m_accept_thread.joinable())
        {
            m_accept_thread.join();
        }

        {
            std::lock_guard lock(m_clients_mutex);
            for (auto &[id, connection] : m_connections)
            {
                CancelIoEx(static_cast<HANDLE>(connection->handle), nullptr);
            }
        }
        for (auto &thread : m_client_threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        {
            std::lock_guard lock(m_clients_mutex);
            for (auto &[id, connection] : m_connections)
            {
                CloseHandle(static_cast<HANDLE>(connection->handle));
            }
            m_connections.clear();
            m_client_threads.clear();
        }

        if (m_stop_event)
        {
            CloseHandle(static_cast<HANDLE>(m_stop_event));
            m_stop_event = nullptr;
        }

        {
            std::lock_guard lock(m_queue_mutex);
            m_queue.clear();
        }
    }

    bool IpcManager::popRequest(IpcRequest *const out)
    {
        std::lock_guard lock(m_queue_mutex);
        if (m_queue.empty())
        {
            return false;
        }
        *out = std::move(m_queue.front());
        m_queue.pop_front();
        return true;
    }

    void IpcManager::sendResponse(uint64_t const connection_id, std::string_view const message)
    {
        std::shared_ptr<Connection> connection;
        {
            std::lock_guard lock(m_clients_mutex);
            auto const it = m_connections.find(connection_id);
            if (it == m_connections.end())
            {
                return;
            }
            connection = it->second;
        }

        std::lock_guard write_lock(connection->write_mutex);
        auto const handle = static_cast<HANDLE>(connection->handle);

        auto write_event = createManualResetEvent();
        if (!write_event.is_valid())
        {
            spdlog::error("[ipc] IpcManager::sendResponse (CreateEventExW)");
            return;
        }

        OVERLAPPED overlapped{};
        overlapped.hEvent = write_event.get();
        DWORD written{};
        BOOL const ok = WriteFile(handle, message.data(), static_cast<DWORD>(message.size()), &written, &overlapped);
        if (!ok)
        {
            DWORD const err = GetLastError();
            if (err == ERROR_IO_PENDING)
            {
                GetOverlappedResult(handle, &overlapped, &written, TRUE);
            }
            else if (err != ERROR_NO_DATA)
            {
                spdlog::error("[ipc] IpcManager::sendResponse (WriteFile)");
            }
        }
    }

    void IpcManager::acceptLoop(std::wstring const pipe_path)
    {
        auto const stop_event = static_cast<HANDLE>(m_stop_event);

        for (;;)
        {
            wil::unique_hfile pipe(CreateNamedPipeW(
                pipe_path.c_str(),
                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                static_cast<DWORD>(kPipeBufferSize),
                static_cast<DWORD>(kPipeBufferSize),
                0,
                nullptr));
            if (!pipe.is_valid())
            {
                spdlog::error("[ipc] IpcManager::acceptLoop (CreateNamedPipeW)");
                break;
            }

            auto connect_event = createManualResetEvent();
            if (!connect_event.is_valid())
            {
                spdlog::error("[ipc] IpcManager::acceptLoop (CreateEventExW)");
                break;
            }

            OVERLAPPED overlapped{};
            overlapped.hEvent = connect_event.get();
            BOOL const connected_immediately = ConnectNamedPipe(pipe.get(), &overlapped);
            DWORD const err = connected_immediately ? ERROR_SUCCESS : GetLastError();

            bool connected;
            if (connected_immediately || err == ERROR_PIPE_CONNECTED)
            {
                connected = true;
            }
            else if (err == ERROR_IO_PENDING)
            {
                HANDLE const wait_handles[2]{connect_event.get(), stop_event};
                DWORD const wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);
                if (wait_result != WAIT_OBJECT_0)
                {
                    CancelIoEx(pipe.get(), &overlapped);
                    break;
                }
                DWORD transferred{};
                connected = GetOverlappedResult(pipe.get(), &overlapped, &transferred, FALSE) != 0;
            }
            else
            {
                spdlog::error("[ipc] IpcManager::acceptLoop (ConnectNamedPipe)");
                continue;
            }

            if (!m_running.load())
            {
                break;
            }
            if (!connected)
            {
                continue;
            }

            uint64_t const connection_id = m_next_connection_id.fetch_add(1);
            auto connection = std::make_shared<Connection>();
            connection->handle = pipe.release();
            {
                std::lock_guard lock(m_clients_mutex);
                m_connections.emplace(connection_id, connection);
                m_client_threads.emplace_back(&IpcManager::clientLoop, this, connection_id, connection);
            }
        }
    }

    void IpcManager::clientLoop(uint64_t const connection_id, std::shared_ptr<Connection> connection)
    {
        auto const handle = static_cast<HANDLE>(connection->handle);
        auto const stop_event = static_cast<HANDLE>(m_stop_event);
        auto read_event = createManualResetEvent();

        if (read_event.is_valid())
        {
            std::vector<char> buffer(kPipeBufferSize);
            for (;;)
            {
                std::string message;
                bool disconnected = false;

                for (;;)
                {
                    ResetEvent(read_event.get());
                    OVERLAPPED overlapped{};
                    overlapped.hEvent = read_event.get();
                    DWORD read_bytes{};
                    BOOL const ok = ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read_bytes, &overlapped);
                    DWORD err = ok ? ERROR_SUCCESS : GetLastError();

                    if (!ok && err == ERROR_IO_PENDING)
                    {
                        HANDLE const wait_handles[2]{read_event.get(), stop_event};
                        DWORD const wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);
                        if (wait_result != WAIT_OBJECT_0)
                        {
                            CancelIoEx(handle, &overlapped);
                            disconnected = true;
                            break;
                        }
                        if (!GetOverlappedResult(handle, &overlapped, &read_bytes, FALSE))
                        {
                            err = GetLastError();
                            if (err != ERROR_MORE_DATA)
                            {
                                disconnected = true;
                                break;
                            }
                        }
                        else
                        {
                            err = ERROR_SUCCESS;
                        }
                    }
                    else if (!ok && err != ERROR_MORE_DATA)
                    {
                        disconnected = true;
                        break;
                    }

                    message.append(buffer.data(), read_bytes);
                    if (err != ERROR_MORE_DATA)
                    {
                        break;
                    }
                }

                if (disconnected)
                {
                    break;
                }
                if (!message.empty())
                {
                    std::lock_guard lock(m_queue_mutex);
                    m_queue.push_back(IpcRequest{connection_id, std::move(message)});
                }
            }
        }

        DisconnectNamedPipe(handle);
        CloseHandle(handle);
        {
            std::lock_guard lock(m_clients_mutex);
            m_connections.erase(connection_id);
        }
    }

}
