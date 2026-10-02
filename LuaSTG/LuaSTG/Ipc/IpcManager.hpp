#pragma once
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace luastg
{

    struct IpcRequest
    {
        uint64_t connection_id{};
        std::string message;
    };

    class IpcManager
    {
    public:
        static IpcManager &GetInstance() noexcept;

        bool start(std::string_view pipe_name);
        void stop();
        bool isRunning() const noexcept { return m_running.load(); }

        bool popRequest(IpcRequest *out);

        void sendResponse(uint64_t connection_id, std::string_view message);

        IpcManager(IpcManager const &) = delete;
        IpcManager(IpcManager &&) = delete;
        IpcManager &operator=(IpcManager const &) = delete;
        IpcManager &operator=(IpcManager &&) = delete;

    private:
        IpcManager() = default;
        ~IpcManager();

        struct Connection
        {
            void *handle{};
            std::mutex write_mutex;
        };

        void acceptLoop(std::wstring pipe_path);
        void clientLoop(uint64_t connection_id, std::shared_ptr<Connection> connection);

        std::atomic<bool> m_running{false};
        void *m_stop_event{};
        std::thread m_accept_thread;

        std::mutex m_clients_mutex;
        std::unordered_map<uint64_t, std::shared_ptr<Connection>> m_connections;
        std::vector<std::thread> m_client_threads;
        std::atomic<uint64_t> m_next_connection_id{1};

        std::mutex m_queue_mutex;
        std::deque<IpcRequest> m_queue;
    };

}
