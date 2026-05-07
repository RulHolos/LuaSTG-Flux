#pragma once
#include "GameResource/ResourceBase.hpp"
#include "Core/Graphics/Device.hpp"
#include "Core/Graphics/Sprite.hpp"

namespace luastg
{
	struct IResourceVideo : public IResourceBase
	{
		virtual bool Play() = 0;
		virtual bool Pause() = 0;
		virtual bool Resume() = 0;
		virtual bool Stop() = 0;
		virtual bool IsPlaying() = 0;
		virtual bool IsPaused() = 0;
		virtual bool IsStopped() = 0;

		virtual double GetTotalTime() = 0;
		virtual double GetTime() = 0;
		virtual bool Seek(double seconds) = 0;

		virtual void SetVolume(float v) = 0;
		virtual float GetVolume() = 0;

		virtual core::Vector2U GetVideoSize() = 0;
		virtual bool IsLooping() = 0;
		virtual void SetLooping(bool loop) = 0;

		virtual bool Update() = 0;

		virtual void Render(float x, float y, float rot, float hscale, float vscale, float z = 0.5f) = 0;
		virtual void RenderRect(float l, float r, float b, float t, float z = 0.5f) = 0;
		virtual void Render4V(float x1, float y1, float z1, float x2, float y2, float z2,
		                      float x3, float y3, float z3, float x4, float y4, float z4) = 0;

		virtual core::Graphics::ISprite* GetSprite() = 0;
		virtual BlendMode GetBlendMode() = 0;
		virtual void SetBlendMode(BlendMode m) = 0;
	};
}

namespace core {
    // idk what this does tbh, I just... winged it cuz I'm lazy.
	template<> constexpr InterfaceId getInterfaceId<luastg::IResourceVideo>() { return UUID::parse("b7e3a1d5-8f42-5c9e-a6b1-d4e7f2c89a03"); }
}
