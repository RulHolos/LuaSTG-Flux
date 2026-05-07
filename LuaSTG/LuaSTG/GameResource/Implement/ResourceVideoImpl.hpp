#pragma once
#include "GameResource/ResourceVideo.hpp"
#include "GameResource/Implement/ResourceBaseImpl.hpp"
#include "Core/Graphics/Renderer.hpp"
#include "core/AudioDecoder.hpp"
#include "core/AudioPlayer.hpp"

#include <mutex>
#include <memory>
#include <chrono>
#include <vector>
#include <thread>
#include <condition_variable>
#include <atomic>

namespace luastg
{
	struct VideoMFData;

	class ResourceVideoImpl final : public ResourceBaseImpl<IResourceVideo>
	{
	public:
		bool Play() override;
		bool Pause() override;
		bool Resume() override;
		bool Stop() override;
		bool IsPlaying() override;
		bool IsPaused() override;
		bool IsStopped() override;

		double GetTotalTime() override;
		double GetTime() override;
		bool Seek(double seconds) override;

		void SetVolume(float v) override;
		float GetVolume() override;

		core::Vector2U GetVideoSize() override;
		bool IsLooping() override;
		void SetLooping(bool loop) override;

		bool Update() override;

		void Render(float x, float y, float rot, float hscale, float vscale, float z) override;
		void RenderRect(float l, float r, float b, float t, float z) override;
		void Render4V(float x1, float y1, float z1, float x2, float y2, float z2,
		              float x3, float y3, float z3, float x4, float y4, float z4) override;

		core::Graphics::ISprite* GetSprite() override { return m_sprite.get(); }
		BlendMode GetBlendMode() override { return m_blend_mode; }
		void SetBlendMode(BlendMode m) override { m_blend_mode = m; }

		static bool CreateFromFile(
			const char* name,
			const char* path,
			core::SmartReference<IResourceVideo>& out,
			std::string* out_error = nullptr);

		ResourceVideoImpl(
			const char* name,
			std::unique_ptr<VideoMFData> mf_data,
			core::Graphics::ITexture2D* texture,
			core::Graphics::ISprite* sprite,
			core::IAudioPlayer* audio_player,
			uint32_t video_width,
			uint32_t video_height,
			double duration);
		~ResourceVideoImpl();

	private:
		void decoderThreadFunc();
		void startDecoderThread();
		void stopDecoderThread();
		void handleEndOfStream();

		std::unique_ptr<VideoMFData> m_mf;
		core::SmartReference<core::Graphics::ITexture2D> m_texture;
		core::SmartReference<core::Graphics::ISprite> m_sprite;
		core::SmartReference<core::IAudioPlayer> m_audio_player;

		BlendMode m_blend_mode = BlendMode::MulAlpha;
		uint32_t m_video_width = 0;
		uint32_t m_video_height = 0;
		double m_duration = 0.0;

		enum class PlaybackState : uint8_t {
			Stopped,
			Playing,
			Paused,
		};
		PlaybackState m_state = PlaybackState::Stopped;
		bool m_looping = false;
		float m_volume = 1.0f;

		std::atomic<int64_t> m_last_decoded_time_100ns{ 0 };
		double m_playback_position = 0.0;
		std::chrono::steady_clock::time_point m_play_start_time;
		double m_play_start_position = 0.0;

		std::vector<uint8_t> m_frame_buffer_a;
		std::vector<uint8_t> m_frame_buffer_b;
		std::vector<uint8_t>* m_decode_buffer = nullptr;
		std::vector<uint8_t>* m_display_buffer = nullptr;
		std::atomic<bool> m_new_frame_ready{ false };
		std::mutex m_frame_swap_mutex;

		std::thread m_decoder_thread;
		std::mutex m_decoder_mutex;
		std::condition_variable m_decoder_cv;
		std::atomic<bool> m_decoder_running{ false };
		std::atomic<bool> m_decoder_wake{ false };
		std::atomic<bool> m_seek_requested{ false };
		std::atomic<int64_t> m_seek_target_100ns{ 0 };

		std::mutex m_state_mutex;
	};
}
