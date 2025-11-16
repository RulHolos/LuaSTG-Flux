#pragma once
#include <string>
#include <discord_rpc.h>
#include <mutex>

namespace luastg {
    class DiscordRPCManager {
    private:
        bool m_initialized = false;
        std::string m_application_id;
        mutable std::mutex m_mutex;

    public:
        DiscordRPCManager() = default;
        ~DiscordRPCManager();

        bool Initialize(const char* application_id) noexcept;

        bool SetPresence(
            const char* state,
            const char* details,
            const char* large_image_key = nullptr,
            const char* large_image_text = nullptr,
            const char* small_image_key = nullptr,
            const char* small_image_text = nullptr,
            int64_t start_timestamp = 0
        ) noexcept;

        void ClearPresence() noexcept;
        void RunCallbacks() noexcept;
        void Shutdown() noexcept;
        bool IsInitialized() const noexcept;
        
        static DiscordRPCManager& GetInstance() noexcept;
    };
}