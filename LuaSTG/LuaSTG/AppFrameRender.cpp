#include "AppFrame.h"
#include "GameResource/LegacyBlendStateHelper.hpp"

namespace luastg {
	void AppFrame::updateGraph2DBlendMode(BlendMode const blend) {
		auto const [v, b] = translateLegacyBlendState(blend);
		auto const renderer = m_pAppModel->getRenderer();
		renderer->setVertexColorBlendState(v);
		renderer->setBlendState(b);
	}

	bool AppFrame::Render(IParticlePool* p, float hscale, float vscale) noexcept {
		assert(p);

		// 设置混合
		updateGraph2DBlendMode(p->GetBlendMode());

		// 渲染
		p->Render(hscale, vscale);
		return true;
	}

	void AppFrame::SnapShot(const char* path) noexcept {
		if (!GetAppModel()->getSwapChain()->saveSnapshotToFile(path)) {
			spdlog::error("[luastg] SnapShot: Failed to save screenshot to file '{}'", path);
			return;
		}
	}
	void AppFrame::SaveTexture(const char* tex_name, const char* path) noexcept {
		core::SmartReference<IResourceTexture> resTex = LRES.FindTexture(tex_name);
		if (!resTex) {
			spdlog::error("[luastg] SaveTexture: Texture resource '{}' doesn't exist", tex_name);
			return;
		}
		if (!resTex->GetTexture()->saveToFile(path)) {
			spdlog::error("[luastg] SaveTexture: Failed to save texture '{}' to file '{}'", tex_name, path);
			return;
		}
	}
};
