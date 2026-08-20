#pragma once
#include "Core/ApplicationModel.hpp"
#include "Core/Graphics/Font.hpp"
#include "core/AudioEngine.hpp"
#include "GameResource/ResourceManager.h"
#include "GameResource/AsyncResourceLoader.hpp"
#include "GameObject/GameObjectPool.h"
#include "Platform/DirectInput.hpp"
#include <unordered_map>
#include <string>

namespace luastg {
	/// @brief 应用程序状态
	enum class AppStatus {
		NotInitialized,
		Initializing,
		Initialized,
		Running,
		Aborted,
		Destroyed,
	};

	struct IRenderTargetManager {
		virtual bool BeginRenderTargetStack() = 0;
		virtual bool EndRenderTargetStack() = 0;
		virtual bool PushRenderTarget(IResourceTexture* rt) = 0;
		virtual bool PopRenderTarget() = 0;
		virtual bool IsRenderTargetStackEmpty() = 0;
		virtual bool CheckRenderTargetInUse(IResourceTexture* rt) = 0;
		virtual core::Vector2U GetTopRenderTargetSize() = 0;

		virtual void AddAutoSizeRenderTarget(IResourceTexture* rt) = 0;
		virtual void RemoveAutoSizeRenderTarget(IResourceTexture* rt) = 0;
		virtual core::Vector2U GetAutoSizeRenderTargetSize() = 0;
		virtual bool ResizeAutoSizeRenderTarget(core::Vector2U size) = 0;
	};

	class AppFrame
		: public core::IApplicationEventListener
		, public core::Graphics::IWindowEventListener
		, public core::Graphics::ISwapChainEventListener
		, public IRenderTargetManager
	{
	private:
		core::SmartReference<core::IApplicationModel> m_pAppModel;
		core::SmartReference<core::Graphics::ITextRenderer> m_pTextRenderer;
		core::SmartReference<core::IAudioEngine> m_audio_engine;
		ResourceMgr m_ResourceMgr;
		std::unique_ptr<AsyncResourceLoader> m_async_resource_loader;
		std::unique_ptr<GameObjectPool> m_GameObjectPool;
		lua_State* L = nullptr;
		uint32_t m_target_fps{ 60 };
		double m_fFPS = 0.;
		double m_fAvgFPS = 0.;
		bool m_bRenderStarted = false;
		std::unique_ptr<Platform::DirectInput> m_DirectInput;

	public:
		bool SetDisplayModeBorderlessFullscreen(core::Vector2U canvas_size, bool vsync);
	};
}
