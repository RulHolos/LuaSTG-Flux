#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0602
#ifdef NTDDI_VERSION
#undef NTDDI_VERSION
#endif
#define NTDDI_VERSION 0x06020000

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>
#include <Shlwapi.h>

#include "GameResource/Implement/ResourceVideoImpl.hpp"
#include "AppFrame.h"
#include "core/FileSystem.hpp"
#include <spdlog/spdlog.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")

namespace luastg
{
	struct VideoMFData
	{
		Microsoft::WRL::ComPtr<IMFSourceReader> source_reader;
		DWORD video_stream_index = 0;
		LONG stride = 0;
		bool use_argb32 = false;
	};

	ResourceVideoImpl::ResourceVideoImpl(
		const char* name,
		std::unique_ptr<VideoMFData> mf_data,
		core::Graphics::ITexture2D* texture,
		core::Graphics::ISprite* sprite,
		core::IAudioPlayer* audio_player,
		uint32_t video_width,
		uint32_t video_height,
		double duration)
		: ResourceBaseImpl(ResourceType::Video, name)
		, m_mf(std::move(mf_data))
		, m_texture(texture)
		, m_sprite(sprite)
		, m_audio_player(audio_player)
		, m_video_width(video_width)
		, m_video_height(video_height)
		, m_duration(duration)
	{
		size_t buf_size = static_cast<size_t>(video_width) * video_height * 4;
		m_frame_buffer_a.resize(buf_size);
		m_frame_buffer_b.resize(buf_size);
		m_decode_buffer = &m_frame_buffer_a;
		m_display_buffer = &m_frame_buffer_b;
	}

	bool ResourceVideoImpl::CreateFromFile(
		const char* name,
		const char* path,
		core::SmartReference<IResourceVideo>& out,
		std::string* out_error)
	{
		{
			static bool mf_initialized = false;
			if (!mf_initialized)
			{
				HRESULT hr = MFStartup(MF_VERSION);
				if (FAILED(hr))
				{
					spdlog::error("[luastg] LoadVideo: Failed to initialize Media Foundation (hr=0x{:08x})", (uint32_t)hr);
					return false;
				}
				mf_initialized = true;
			}
		}

		core::SmartReference<core::IData> file_data;
		if (!core::FileSystemManager::readFile(path, file_data.put()))
		{
			spdlog::error("[luastg] LoadVideo: Failed to read file '{}' from file system", path);
			return false;
		}

		HRESULT hr = S_OK;

		Microsoft::WRL::ComPtr<IMFAttributes> attributes;
		hr = MFCreateAttributes(&attributes, 1);
		if (FAILED(hr))
		{
			spdlog::error("[luastg] LoadVideo: Failed to create MF attributes (hr=0x{:08x})", (uint32_t)hr);
			return false;
		}
		attributes->SetUINT32(MF_LOW_LATENCY, TRUE);
		attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

		IStream* raw_stream = SHCreateMemStream(
			static_cast<const BYTE*>(file_data->data()),
			static_cast<UINT>(file_data->size()));
		if (!raw_stream)
		{
			spdlog::error("[luastg] LoadVideo: Failed to create memory stream for '{}'", path);
			return false;
		}

		Microsoft::WRL::ComPtr<IMFByteStream> byte_stream;
		hr = MFCreateMFByteStreamOnStream(raw_stream, &byte_stream);
		raw_stream->Release();
		if (FAILED(hr))
		{
			spdlog::error("[luastg] LoadVideo: Failed to create MF byte stream (hr=0x{:08x})", (uint32_t)hr);
			return false;
		}

		Microsoft::WRL::ComPtr<IMFSourceReader> source_reader;
		hr = MFCreateSourceReaderFromByteStream(byte_stream.Get(), attributes.Get(), &source_reader);
		if (FAILED(hr))
		{
			spdlog::error("[luastg] LoadVideo: Failed to create source reader from '{}' (hr=0x{:08x})", path, (uint32_t)hr);
			return false;
		}

		Microsoft::WRL::ComPtr<IMFMediaType> video_type;
		hr = MFCreateMediaType(&video_type);
		if (FAILED(hr))
		{
			spdlog::error("[luastg] LoadVideo: Failed to create media type");
			return false;
		}
		video_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);

		bool use_argb32 = false;
		video_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);
		hr = source_reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, video_type.Get());
		if (SUCCEEDED(hr))
		{
			use_argb32 = true;
			spdlog::info("[luastg] LoadVideo: Using ARGB32 output for '{}'", path);
		}
		else
		{
			video_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
			hr = source_reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, video_type.Get());
			if (FAILED(hr))
			{
				spdlog::error("[luastg] LoadVideo: Failed to set video output type (hr=0x{:08x})", (uint32_t)hr);
				if (out_error)
					*out_error = "Failed to set video output type";
				return false;
			}
			spdlog::info("[luastg] LoadVideo: Using RGB32 output for '{}' (will patch alpha)", path);
		}

		Microsoft::WRL::ComPtr<IMFMediaType> actual_type;
		hr = source_reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &actual_type);
		if (FAILED(hr))
		{
			spdlog::error("[luastg] LoadVideo: Failed to get current media type");
			return false;
		}

		UINT32 video_width = 0, video_height = 0;
		hr = MFGetAttributeSize(actual_type.Get(), MF_MT_FRAME_SIZE, &video_width, &video_height);
		if (FAILED(hr) || video_width == 0 || video_height == 0)
		{
			spdlog::error("[luastg] LoadVideo: Failed to get video dimensions");
			return false;
		}

		LONG stride = 0;
		hr = actual_type->GetUINT32(MF_MT_DEFAULT_STRIDE, reinterpret_cast<UINT32*>(&stride));
		if (FAILED(hr))
		{
			stride = static_cast<LONG>(video_width) * 4;
		}
		spdlog::info("[luastg] LoadVideo: Video stride = {} (width*4 = {})", stride, video_width * 4);

		PROPVARIANT duration_var;
		PropVariantInit(&duration_var);
		double duration = 0.0;
		hr = source_reader->GetPresentationAttribute(
			(DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &duration_var);
		if (SUCCEEDED(hr) && duration_var.vt == VT_UI8)
		{
			duration = static_cast<double>(duration_var.uhVal.QuadPart) / 10000000.0;
		}
		PropVariantClear(&duration_var);

		core::SmartReference<core::Graphics::ITexture2D> texture;
		if (!LAPP.GetAppModel()->getDevice()->createTexture(
			core::Vector2U(video_width, video_height), texture.put()))
		{
			spdlog::error("[luastg] LoadVideo: Failed to create texture for video '{}' ({}x{})", name, video_width, video_height);
			return false;
		}

		core::SmartReference<core::Graphics::ISprite> sprite;
		if (!core::Graphics::ISprite::create(
			LAPP.GetRenderer2D(), texture.get(), sprite.put()))
		{
			spdlog::error("[luastg] LoadVideo: Failed to create sprite for video '{}'", name);
			return false;
		}
		sprite->setTextureRect(core::RectF(0.0f, 0.0f, (float)video_width, (float)video_height));
		sprite->setTextureCenter(core::Vector2F((float)video_width * 0.5f, (float)video_height * 0.5f));

		core::SmartReference<core::IAudioPlayer> audio_player;
		{
			core::SmartReference<core::IAudioDecoder> audio_decoder;
			if (core::IAudioDecoder::create(path, audio_decoder.put()))
			{
				if (!LAPP.getAudioEngine()->createStreamAudioPlayer(
					audio_decoder.get(), core::AudioMixingChannel::music, audio_player.put()))
				{
					spdlog::warn("[luastg] LoadVideo: Failed to create audio player for '{}', video will play without sound", name);
					audio_player = nullptr;
				}
			}
			else
			{
				spdlog::info("[luastg] LoadVideo: No audio track in '{}' or unsupported audio format", name);
			}
		}

		auto mf_data = std::make_unique<VideoMFData>();
		mf_data->source_reader = source_reader;
		mf_data->video_stream_index = (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM;
		mf_data->stride = stride;
		mf_data->use_argb32 = use_argb32;

		out.attach(new ResourceVideoImpl(
			name, std::move(mf_data), texture.get(), sprite.get(), audio_player.get(),
			video_width, video_height, duration));

		spdlog::info("[luastg] LoadVideo: Loaded video '{}' from '{}' ({}x{}, {:.1f}s)",
			name, path, video_width, video_height, duration);

		return true;
	}

	ResourceVideoImpl::~ResourceVideoImpl()
	{
		stopDecoderThread();
		if (m_audio_player)
			m_audio_player->stop();
	}

	void ResourceVideoImpl::startDecoderThread()
	{
		if (m_decoder_running.load())
			return;
		m_decoder_running.store(true);
		m_decoder_wake.store(true);
		m_decoder_thread = std::thread(&ResourceVideoImpl::decoderThreadFunc, this);
	}

	void ResourceVideoImpl::stopDecoderThread()
	{
		if (!m_decoder_running.load())
			return;
		m_decoder_running.store(false);
		m_decoder_wake.store(true);
		m_decoder_cv.notify_one();
		if (m_decoder_thread.joinable())
			m_decoder_thread.join();
	}

	void ResourceVideoImpl::decoderThreadFunc()
	{
		while (m_decoder_running.load())
		{
			{
				std::unique_lock lock(m_decoder_mutex);
				m_decoder_cv.wait(lock, [this]() {
					return m_decoder_wake.load() || !m_decoder_running.load();
				});
				if (!m_decoder_running.load())
					break;
				m_decoder_wake.store(false);
			}

			if (m_seek_requested.load())
			{
				m_seek_requested.store(false);
				int64_t target = m_seek_target_100ns.load();
				PROPVARIANT var;
				PropVariantInit(&var);
				var.vt = VT_I8;
				var.hVal.QuadPart = target;
				m_mf->source_reader->SetCurrentPosition(GUID_NULL, var);
				PropVariantClear(&var);
				m_last_decoded_time_100ns.store(target);
			}

			while (m_decoder_running.load() && m_state == PlaybackState::Playing)
			{
				auto now = std::chrono::steady_clock::now();
				double elapsed = std::chrono::duration<double>(now - m_play_start_time).count();
				double current_time = m_play_start_position + elapsed;
				int64_t current_time_100ns = static_cast<int64_t>(current_time * 10000000.0);
				int64_t decoded_time = m_last_decoded_time_100ns.load();

				if (decoded_time > current_time_100ns + 660000)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
					continue;
				}

				if (m_seek_requested.load())
					break;

				DWORD stream_index = 0;
				DWORD flags = 0;
				LONGLONG timestamp = 0;
				Microsoft::WRL::ComPtr<IMFSample> sample;

				HRESULT hr = m_mf->source_reader->ReadSample(
					m_mf->video_stream_index, 0,
					&stream_index, &flags, &timestamp, &sample);

				if (FAILED(hr))
					break;

				if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
				{
					handleEndOfStream();
					if (!m_looping)
						break;
					continue;
				}

				if (!sample)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}

				Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
				hr = sample->ConvertToContiguousBuffer(&buffer);
				if (FAILED(hr))
					continue;

				BYTE* src_data = nullptr;
				LONG src_stride = 0;
				Microsoft::WRL::ComPtr<IMF2DBuffer> buffer2d;
				bool using_2d = false;

				if (SUCCEEDED(buffer.As(&buffer2d)))
				{
					hr = buffer2d->Lock2D(&src_data, &src_stride);
					if (SUCCEEDED(hr))
						using_2d = true;
				}

				if (!using_2d)
				{
					DWORD data_length = 0;
					hr = buffer->Lock(&src_data, nullptr, &data_length);
					if (FAILED(hr))
						continue;
					src_stride = m_mf->stride;
				}

				uint32_t dst_pitch = m_video_width * 4;
				bool need_alpha_fix = !m_mf->use_argb32;

				{
					std::lock_guard swap_lock(m_frame_swap_mutex);
					std::vector<uint8_t>& dest = *m_decode_buffer;

					if (static_cast<uint32_t>(std::abs(src_stride)) == dst_pitch && src_stride > 0 && !need_alpha_fix)
					{
						memcpy(dest.data(), src_data, static_cast<size_t>(dst_pitch) * m_video_height);
					}
					else
					{
						BYTE* src_row = (src_stride >= 0)
							? src_data
							: (src_data + std::abs(src_stride) * (m_video_height - 1));

						for (uint32_t y = 0; y < m_video_height; ++y)
						{
							uint8_t* dst_row = dest.data() + y * dst_pitch;
							memcpy(dst_row, src_row, dst_pitch);

							if (need_alpha_fix)
							{
								uint32_t* pixels = reinterpret_cast<uint32_t*>(dst_row);
								for (uint32_t x = 0; x < m_video_width; ++x)
									pixels[x] |= 0xFF000000u;
							}

							src_row += (src_stride >= 0) ? src_stride : -src_stride;
						}
					}

					std::swap(m_decode_buffer, m_display_buffer);
					m_new_frame_ready.store(true);
				}

				if (using_2d)
					buffer2d->Unlock2D();
				else
					buffer->Unlock();

				m_last_decoded_time_100ns.store(timestamp);

				if (timestamp >= current_time_100ns)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
			}
		}
	}

	bool ResourceVideoImpl::Play()
	{
		std::lock_guard lock(m_state_mutex);
		if (m_state == PlaybackState::Playing)
			return true;

		if (m_state == PlaybackState::Stopped)
		{
			m_seek_requested.store(true);
			m_seek_target_100ns.store(0);
			m_last_decoded_time_100ns.store(0);
			m_playback_position = 0.0;
		}

		m_state = PlaybackState::Playing;
		m_play_start_time = std::chrono::steady_clock::now();
		m_play_start_position = m_playback_position;

		if (m_audio_player)
		{
			m_audio_player->setVolume(m_volume);
			m_audio_player->play(m_playback_position);
		}

		startDecoderThread();
		m_decoder_wake.store(true);
		m_decoder_cv.notify_one();

		return true;
	}

	bool ResourceVideoImpl::Pause()
	{
		std::lock_guard lock(m_state_mutex);
		if (m_state != PlaybackState::Playing)
			return false;

		m_state = PlaybackState::Paused;

		auto now = std::chrono::steady_clock::now();
		double elapsed = std::chrono::duration<double>(now - m_play_start_time).count();
		m_playback_position = m_play_start_position + elapsed;

		if (m_audio_player)
			m_audio_player->pause();

		return true;
	}

	bool ResourceVideoImpl::Resume()
	{
		std::lock_guard lock(m_state_mutex);
		if (m_state != PlaybackState::Paused)
			return false;

		m_state = PlaybackState::Playing;
		m_play_start_time = std::chrono::steady_clock::now();
		m_play_start_position = m_playback_position;

		if (m_audio_player)
			m_audio_player->resume();

		m_decoder_wake.store(true);
		m_decoder_cv.notify_one();

		return true;
	}

	bool ResourceVideoImpl::Stop()
	{
		std::lock_guard lock(m_state_mutex);
		if (m_state == PlaybackState::Stopped)
			return true;

		m_state = PlaybackState::Stopped;
		m_playback_position = 0.0;
		m_last_decoded_time_100ns.store(0);

		if (m_audio_player)
			m_audio_player->stop();

		return true;
	}

	bool ResourceVideoImpl::IsPlaying()
	{
		return m_state == PlaybackState::Playing;
	}

	bool ResourceVideoImpl::IsPaused()
	{
		return m_state == PlaybackState::Paused;
	}

	bool ResourceVideoImpl::IsStopped()
	{
		return m_state == PlaybackState::Stopped;
	}

	double ResourceVideoImpl::GetTotalTime()
	{
		return m_duration;
	}

	double ResourceVideoImpl::GetTime()
	{
		if (m_state == PlaybackState::Playing)
		{
			auto now = std::chrono::steady_clock::now();
			double elapsed = std::chrono::duration<double>(now - m_play_start_time).count();
			return m_play_start_position + elapsed;
		}
		return m_playback_position;
	}

	bool ResourceVideoImpl::Seek(double seconds)
	{
		std::lock_guard lock(m_state_mutex);
		seconds = std::max(0.0, std::min(seconds, m_duration));

		int64_t target_100ns = static_cast<int64_t>(seconds * 10000000.0);
		m_seek_target_100ns.store(target_100ns);
		m_seek_requested.store(true);
		m_last_decoded_time_100ns.store(target_100ns);
		m_playback_position = seconds;

		if (m_state == PlaybackState::Playing)
		{
			m_play_start_time = std::chrono::steady_clock::now();
			m_play_start_position = seconds;
			if (m_audio_player)
			{
				m_audio_player->stop();
				m_audio_player->play(seconds);
			}
			m_decoder_wake.store(true);
			m_decoder_cv.notify_one();
		}

		return true;
	}

	void ResourceVideoImpl::SetVolume(float v)
	{
		m_volume = std::max(0.0f, std::min(v, 1.0f));
		if (m_audio_player)
			m_audio_player->setVolume(m_volume);
	}

	float ResourceVideoImpl::GetVolume()
	{
		return m_volume;
	}

	core::Vector2U ResourceVideoImpl::GetVideoSize()
	{
		return core::Vector2U(m_video_width, m_video_height);
	}

	bool ResourceVideoImpl::IsLooping()
	{
		return m_looping;
	}

	void ResourceVideoImpl::SetLooping(bool loop)
	{
		m_looping = loop;
	}

	bool ResourceVideoImpl::Update()
	{
		if (m_state != PlaybackState::Playing)
			return true;

		auto now = std::chrono::steady_clock::now();
		double elapsed = std::chrono::duration<double>(now - m_play_start_time).count();
		double current_time = m_play_start_position + elapsed;

		if (current_time >= m_duration && !m_looping)
		{
			if (m_state == PlaybackState::Stopped)
				return true;
		}

		if (m_new_frame_ready.load())
		{
			std::lock_guard swap_lock(m_frame_swap_mutex);
			m_new_frame_ready.store(false);

			uint32_t pitch = m_video_width * 4;
			core::RectU rc(0, 0, m_video_width, m_video_height);
			m_texture->uploadPixelData(rc, m_display_buffer->data(), pitch);
		}

		m_playback_position = current_time;
		return true;
	}

	void ResourceVideoImpl::handleEndOfStream()
	{
		if (m_looping)
		{
			PROPVARIANT var;
			PropVariantInit(&var);
			var.vt = VT_I8;
			var.hVal.QuadPart = 0;
			m_mf->source_reader->SetCurrentPosition(GUID_NULL, var);
			PropVariantClear(&var);

			m_last_decoded_time_100ns.store(0);
			m_playback_position = 0.0;
			m_play_start_time = std::chrono::steady_clock::now();
			m_play_start_position = 0.0;

			if (m_audio_player)
			{
				m_audio_player->stop();
				m_audio_player->setVolume(m_volume);
				m_audio_player->play(0.0);
			}
		}
		else
		{
			m_state = PlaybackState::Stopped;
			m_playback_position = m_duration;
			if (m_audio_player)
				m_audio_player->stop();
		}
	}

	void ResourceVideoImpl::Render(float x, float y, float rot, float hscale, float vscale, float z)
	{
		core::Graphics::ISprite* pSprite = GetSprite();
		float const z_backup = pSprite->getZ();
		pSprite->setZ(z);
		LAPP.updateGraph2DBlendMode(GetBlendMode());
		pSprite->draw(core::Vector2F(x, y), core::Vector2F(hscale, vscale), rot);
		pSprite->setZ(z_backup);
	}

	void ResourceVideoImpl::RenderRect(float l, float r, float b, float t, float z)
	{
		core::Graphics::ISprite* pSprite = GetSprite();
		float const z_backup = pSprite->getZ();
		pSprite->setZ(z);
		LAPP.updateGraph2DBlendMode(GetBlendMode());
		pSprite->draw(core::RectF(l, t, r, b));
		pSprite->setZ(z_backup);
	}

	void ResourceVideoImpl::Render4V(float x1, float y1, float z1, float x2, float y2, float z2,
	                                  float x3, float y3, float z3, float x4, float y4, float z4)
	{
		LAPP.updateGraph2DBlendMode(GetBlendMode());
		GetSprite()->draw(
			core::Vector3F(x1, y1, z1),
			core::Vector3F(x2, y2, z2),
			core::Vector3F(x3, y3, z3),
			core::Vector3F(x4, y4, z4));
	}
}