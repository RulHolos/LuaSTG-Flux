#include "DiscordRPC/DiscordRPCManager.hpp"
#include <spdlog/spdlog.h>

namespace luastg {
    static DiscordRPCManager g_discord_rpc_manager;

    DiscordRPCManager::~DiscordRPCManager() {
        Shutdown();
    }

    bool DiscordRPCManager::Initialize(const char* application_id) noexcept {
        if (!application_id || application_id[0] == '\0') {
            spdlog::warn("[Discord RPC] Initialize failed: empty application_id");
            return false;
        }

        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_initialized)
            return true;
        
        m_application_id = application_id;

        DiscordEventHandlers handlers = {};
        // Empty for now cuz I don't care for them yet.
        handlers.ready = nullptr;
        handlers.disconnected = nullptr;
        handlers.errored = nullptr;
        handlers.joinGame = nullptr;
        handlers.spectateGame = nullptr;
        handlers.joinRequest = nullptr;

        Discord_Initialize(application_id, &handlers, 1, nullptr);
        m_initialized = true;

        spdlog::info("[Discord RPC] Initialized with app id: {}", application_id);
        return true;
    }

    bool DiscordRPCManager::SetPresence(
        const char* state,
        const char* details,
        const char* large_image_key,
        const char* large_image_text,
        const char* small_image_key,
        const char* small_image_text,
        int64_t start_timestamp
    ) noexcept {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_initialized)
            return false;

        DiscordRichPresence presence = {};
        presence.state = state;
        presence.details = details;
        presence.largeImageKey = large_image_key;
        presence.largeImageText = large_image_text;
        presence.smallImageKey = small_image_key;
        presence.smallImageText = small_image_text;
        presence.startTimestamp = start_timestamp;
        presence.instance = 1;

        Discord_UpdatePresence(&presence);
        spdlog::info("[Discord RPC] Presence Changed '{}', '{}'", state, details);

        return true;
    }

    void DiscordRPCManager::ClearPresence() noexcept {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_initialized) return;
        Discord_ClearPresence();
    }

    void DiscordRPCManager::RunCallbacks() noexcept {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_initialized) return;
        Discord_RunCallbacks();
    }

    void DiscordRPCManager::Shutdown() noexcept {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_initialized) return;
        Discord_Shutdown();
        m_initialized = false;
        spdlog::info("[Discord RPC] RPC instance shutdown");
    }

    bool DiscordRPCManager::IsInitialized() const noexcept {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_initialized;
    }

    DiscordRPCManager& DiscordRPCManager::GetInstance() noexcept {
        return g_discord_rpc_manager;
    }
}