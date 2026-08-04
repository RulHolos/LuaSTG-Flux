#include "GameResource/ResourceManager.h"

namespace luastg
{
	ResourceMgr::ResourceMgr()
		: m_GlobalResourcePool(this, ResourcePoolType::Global)
		, m_StageResourcePool(this, ResourcePoolType::Stage)
	{
	}

	// ResourcePool

	bool ResourceMgr::CreatePool(std::string_view name)
	{
		if (name.empty()) return false;
		std::string key(name);

		// Reserved names (compatibility)
		if (key == "global" || key == "stage") return false;

		auto it = m_CustomPools.find(key);
		if (it != m_CustomPools.end()) return false; // Exists: skip

		std::lock_guard<std::mutex> lk(m_CustomPoolsMutex);

		m_CustomPools.emplace(key, std::make_unique<ResourcePool>(this, ResourcePoolType::None, key));

		spdlog::info("[luastg] Resource Pool '%s' created", name);

		return true;
	}

	bool ResourceMgr::RemovePool(std::string_view name)
	{
		if (name.empty()) return false;
		std::string key(name);

		auto it = m_CustomPools.find(key);
		if (it == m_CustomPools.end()) return false;

		std::lock_guard<std::mutex> lk(m_CustomPoolsMutex);

		if (m_pActiveCustomPool == it->second.get())
		{
			// If this pool was active, clear the current active pool (obviously)
			m_pActiveCustomPool = nullptr;
			m_ActiveCustomPoolName.clear();
		}
		it->second->Clear();
		m_CustomPools.erase(it);
		return true;
	}

	ResourcePool* ResourceMgr::GetPool(std::string_view name) noexcept
	{
		if (name.empty()) return nullptr;
		std::string key(name);

		if (key == "global") return &m_GlobalResourcePool;
		if (key == "stage") return &m_StageResourcePool;

		std::lock_guard<std::mutex> lk(m_CustomPoolsMutex);

		auto it = m_CustomPools.find(key);
		return it != m_CustomPools.end() ? it->second.get() : nullptr;
	}

	bool ResourceMgr::SetActivedPoolByName(std::string_view name) noexcept
	{
		// Disable custom pool if name is empty or null.
		if (name.empty()) {
			std::lock_guard<std::mutex> lk(m_CustomPoolsMutex);
			m_pActiveCustomPool = nullptr;
			m_ActiveCustomPoolName.clear();
			return true;
		}

		std::string key(name);

		#pragma region Reserved pools
		if (key == "global") {
			std::lock_guard<std::mutex> lk(m_CustomPoolsMutex);
			m_pActiveCustomPool = nullptr;
            m_ActiveCustomPoolName.clear();
            m_ActivedPool = ResourcePoolType::Global;
            return true;
		}
		if (key == "stage") {
			std::lock_guard<std::mutex> lk(m_CustomPoolsMutex);
            m_pActiveCustomPool = nullptr;
            m_ActiveCustomPoolName.clear();
            m_ActivedPool = ResourcePoolType::Stage;
            return true;
        }
		#pragma endregion

		{
			std::lock_guard<std::mutex> lk(m_CustomPoolsMutex);
			auto it = m_CustomPools.find(key);
			if (it == m_CustomPools.end()) return false;
			m_pActiveCustomPool = it->second.get();
			m_ActiveCustomPoolName = key;
		}

		return true;
	}

	// 资源池管理

	void ResourceMgr::ClearAllResource() noexcept {
		m_GlobalResourcePool.Clear();
		m_StageResourcePool.Clear();

		std::lock_guard<std::mutex> lk(m_CustomPoolsMutex);
		for (auto &kv : m_CustomPools)
		{
			if (kv.second)
				kv.second->Clear();
		}
		m_pActiveCustomPool = nullptr;
		m_ActiveCustomPoolName.clear();

		m_ActivedPool = ResourcePoolType::Global;
		m_GlobalImageScaleFactor = 1.0f;
	}

	ResourcePoolType ResourceMgr::GetActivedPoolType() noexcept {
		return m_ActivedPool;
	}

	void ResourceMgr::SetActivedPoolType(ResourcePoolType t) noexcept {
		m_ActivedPool = t;
	}

	ResourcePool* ResourceMgr::GetActivedPool() noexcept {
		if (m_pActiveCustomPool) return m_pActiveCustomPool;

		return GetResourcePool(m_ActivedPool);
	}

	ResourcePool* ResourceMgr::GetResourcePool(ResourcePoolType t) noexcept {
		switch (t) {
			case ResourcePoolType::Global:
				return &m_GlobalResourcePool;
			case ResourcePoolType::Stage:
				return &m_StageResourcePool;
			default:
				return nullptr;
		}
	}

	std::vector<std::string> ResourceMgr::EnumPools() const noexcept
	{
		std::vector<std::string> ret;
		ret.reserve(2 + m_CustomPools.size());
		ret.push_back("global");
		ret.push_back("stage");

		std::lock_guard<std::mutex> lk(m_CustomPoolsMutex);

		for (auto const& kv : m_CustomPools)
			ret.push_back(kv.first);
		return ret;
	}

	// 自动查找资源池资源

	void ResourceMgr::UpdateSpritesOnRenderTargetResize(core::Graphics::ITexture2D* texture, core::Vector2U old_size, core::Vector2U new_size) noexcept
	{
		m_GlobalResourcePool.UpdateSpritesOnRenderTargetResize(texture, old_size, new_size);
		m_StageResourcePool.UpdateSpritesOnRenderTargetResize(texture, old_size, new_size);
		std::lock_guard<std::mutex> lk(m_CustomPoolsMutex);
		for (auto& [name, pool] : m_CustomPools)
		{
			pool->UpdateSpritesOnRenderTargetResize(texture, old_size, new_size);
		}
	}

	// help.
	core::SmartReference<IResourceTexture> ResourceMgr::FindTexture(const char* name) noexcept {
		core::SmartReference<IResourceTexture> tRet;
		
		// Search for active resource pool first
		if (m_pActiveCustomPool)
		{
			if ((tRet = m_pActiveCustomPool->GetTexture(name)))
				return tRet;
		}

		// Hardcoded pools search
		if ((tRet = m_StageResourcePool.GetTexture(name)))
            return tRet;
        if ((tRet = m_GlobalResourcePool.GetTexture(name)))
            return tRet;
		
		// Last ditch effort, search this fucker in all custom pools!!!!
		for (auto const& kv : m_CustomPools)
		{
			auto p = kv.second.get();
			if (p == m_pActiveCustomPool) continue;
			if ((tRet = p->GetTexture(name)))
				return tRet;
		}
		return tRet;
	}

	core::SmartReference<IResourceSprite> ResourceMgr::FindSprite(const char* name) noexcept {
		core::SmartReference<IResourceSprite> tRet;
		if (m_pActiveCustomPool)
		{
			if ((tRet = m_pActiveCustomPool->GetSprite(name)))
				return tRet;
		}

		if ((tRet = m_StageResourcePool.GetSprite(name)))
            return tRet;
        if ((tRet = m_GlobalResourcePool.GetSprite(name)))
            return tRet;
		
		for (auto const& kv : m_CustomPools)
		{
			auto p = kv.second.get();
			if (p == m_pActiveCustomPool) continue;
			if ((tRet = p->GetSprite(name)))
				return tRet;
		}
		return tRet;
	}

	core::SmartReference<IResourceAnimation> ResourceMgr::FindAnimation(const char* name) noexcept {
		core::SmartReference<IResourceAnimation> tRet;
		if (m_pActiveCustomPool)
		{
			if ((tRet = m_pActiveCustomPool->GetAnimation(name)))
				return tRet;
		}

		if ((tRet = m_StageResourcePool.GetAnimation(name)))
            return tRet;
        if ((tRet = m_GlobalResourcePool.GetAnimation(name)))
            return tRet;
		
		for (auto const& kv : m_CustomPools)
		{
			auto p = kv.second.get();
			if (p == m_pActiveCustomPool) continue;
			if ((tRet = p->GetAnimation(name)))
				return tRet;
		}
		return tRet;
	}

	core::SmartReference<IResourceMusic> ResourceMgr::FindMusic(const char* name) noexcept {
		core::SmartReference<IResourceMusic> tRet;
		if (m_pActiveCustomPool)
		{
			if ((tRet = m_pActiveCustomPool->GetMusic(name)))
				return tRet;
		}

		if ((tRet = m_StageResourcePool.GetMusic(name)))
            return tRet;
        if ((tRet = m_GlobalResourcePool.GetMusic(name)))
            return tRet;
		
		for (auto const& kv : m_CustomPools)
		{
			auto p = kv.second.get();
			if (p == m_pActiveCustomPool) continue;
			if ((tRet = p->GetMusic(name)))
				return tRet;
		}
		return tRet;
	}

	core::SmartReference<IResourceSoundEffect> ResourceMgr::FindSound(const char* name) noexcept {
		core::SmartReference<IResourceSoundEffect> tRet;
		if (m_pActiveCustomPool)
		{
			if ((tRet = m_pActiveCustomPool->GetSound(name)))
				return tRet;
		}

		if ((tRet = m_StageResourcePool.GetSound(name)))
            return tRet;
        if ((tRet = m_GlobalResourcePool.GetSound(name)))
            return tRet;
		
		for (auto const& kv : m_CustomPools)
		{
			auto p = kv.second.get();
			if (p == m_pActiveCustomPool) continue;
			if ((tRet = p->GetSound(name)))
				return tRet;
		}
		return tRet;
	}

	core::SmartReference<IResourceParticle> ResourceMgr::FindParticle(const char* name) noexcept {
		core::SmartReference<IResourceParticle> tRet;
		if (m_pActiveCustomPool)
		{
			if ((tRet = m_pActiveCustomPool->GetParticle(name)))
				return tRet;
		}

		if ((tRet = m_StageResourcePool.GetParticle(name)))
            return tRet;
        if ((tRet = m_GlobalResourcePool.GetParticle(name)))
            return tRet;
		
		for (auto const& kv : m_CustomPools)
		{
			auto p = kv.second.get();
			if (p == m_pActiveCustomPool) continue;
			if ((tRet = p->GetParticle(name)))
				return tRet;
		}
		return tRet;
	}

	core::SmartReference<IResourceFont> ResourceMgr::FindSpriteFont(const char* name) noexcept {
		core::SmartReference<IResourceFont> tRet;
		if (m_pActiveCustomPool)
		{
			if ((tRet = m_pActiveCustomPool->GetSpriteFont(name)))
				return tRet;
		}

		if ((tRet = m_StageResourcePool.GetSpriteFont(name)))
            return tRet;
        if ((tRet = m_GlobalResourcePool.GetSpriteFont(name)))
            return tRet;
		
		for (auto const& kv : m_CustomPools)
		{
			auto p = kv.second.get();
			if (p == m_pActiveCustomPool) continue;
			if ((tRet = p->GetSpriteFont(name)))
				return tRet;
		}
		return tRet;
	}

	core::SmartReference<IResourceFont> ResourceMgr::FindTTFFont(const char* name) noexcept {
		core::SmartReference<IResourceFont> tRet;
		if (m_pActiveCustomPool)
		{
			if ((tRet = m_pActiveCustomPool->GetTTFFont(name)))
				return tRet;
		}

		if ((tRet = m_StageResourcePool.GetTTFFont(name)))
            return tRet;
        if ((tRet = m_GlobalResourcePool.GetTTFFont(name)))
            return tRet;
		
		for (auto const& kv : m_CustomPools)
		{
			auto p = kv.second.get();
			if (p == m_pActiveCustomPool) continue;
			if ((tRet = p->GetTTFFont(name)))
				return tRet;
		}
		return tRet;
	}

	core::SmartReference<IResourcePostEffectShader> ResourceMgr::FindFX(const char* name) noexcept {
		core::SmartReference<IResourcePostEffectShader> tRet;
		if (m_pActiveCustomPool)
		{
			if ((tRet = m_pActiveCustomPool->GetFX(name)))
				return tRet;
		}

		if ((tRet = m_StageResourcePool.GetFX(name)))
            return tRet;
        if ((tRet = m_GlobalResourcePool.GetFX(name)))
            return tRet;
		
		for (auto const& kv : m_CustomPools)
		{
			auto p = kv.second.get();
			if (p == m_pActiveCustomPool) continue;
			if ((tRet = p->GetFX(name)))
				return tRet;
		}
		return tRet;
	}

	core::SmartReference<IResourceModel> ResourceMgr::FindModel(const char* name) noexcept
	{
		core::SmartReference<IResourceModel> tRet;
		if (m_pActiveCustomPool)
		{
			if ((tRet = m_pActiveCustomPool->GetModel(name)))
				return tRet;
		}

		if ((tRet = m_StageResourcePool.GetModel(name)))
            return tRet;
        if ((tRet = m_GlobalResourcePool.GetModel(name)))
            return tRet;
		
		for (auto const& kv : m_CustomPools)
		{
			auto p = kv.second.get();
			if (p == m_pActiveCustomPool) continue;
			if ((tRet = p->GetModel(name)))
				return tRet;
		}
		return tRet;
	}

	core::SmartReference<IResourceVideo> ResourceMgr::FindVideo(const char* name) noexcept
	{
		core::SmartReference<IResourceVideo> tRet;
		if (m_pActiveCustomPool)
		{
			if ((tRet = m_pActiveCustomPool->GetVideo(name)))
				return tRet;
		}

		if ((tRet = m_StageResourcePool.GetVideo(name)))
            return tRet;
        if ((tRet = m_GlobalResourcePool.GetVideo(name)))
            return tRet;
		
		for (auto const& kv : m_CustomPools)
		{
			auto p = kv.second.get();
			if (p == m_pActiveCustomPool)
				continue;
			if ((tRet = p->GetVideo(name)))
				return tRet;
		}
		return tRet;
	}

	// 其他资源操作

	bool ResourceMgr::GetTextureSize(const char* name, core::Vector2U& out) noexcept {
		core::SmartReference<IResourceTexture> tRet = FindTexture(name);
		if (!tRet)
			return false;
		out = tRet->GetTexture()->getSize();
		return true;
	}
	bool ResourceMgr::GetTextureHandle(const char* name, size_t& out) noexcept {
		core::SmartReference<IResourceTexture> tRet = FindTexture(name);
		if (!tRet)
			return false;
		out = reinterpret_cast<size_t>(tRet->GetTexture()->getNativeHandle());
		return true;
	}

	void ResourceMgr::CacheTTFFontString(const char* name, const char* text, size_t len) noexcept {
		core::SmartReference<IResourceFont> f = FindTTFFont(name);
		if (f)
			f->GetGlyphManager()->cacheString(core::StringView(text, len));
		else
			spdlog::error("[luastg] CacheTTFFontString: The specified font was not found when caching glyphs: '{}'", name);
	}

	void ResourceMgr::UpdateSound()
	{
		for (auto& snd : m_GlobalResourcePool.m_SoundSpritePool)
		{
			snd.second->FlushCommand();
		}
		for (auto& snd : m_StageResourcePool.m_SoundSpritePool)
		{
			snd.second->FlushCommand();
		}
	}

	void ResourceMgr::UpdateVideo()
	{
		auto update_pool = [](ResourcePool& pool) {
			for (auto& v : pool.m_VideoPool)
			{
				v.second->Update();
			}
		};
		update_pool(m_GlobalResourcePool);
		update_pool(m_StageResourcePool);
		std::lock_guard lock(m_CustomPoolsMutex);
		for (auto& kv : m_CustomPools)
		{
			update_pool(*kv.second);
		}
	}

	// 其他

	#ifdef LDEVVERSION
	bool ResourceMgr::g_ResourceLoadingLog = true;
	#else
	bool ResourceMgr::g_ResourceLoadingLog = false;
	#endif

	void ResourceMgr::SetResourceLoadingLog(bool b) { g_ResourceLoadingLog = b; }

	bool ResourceMgr::GetResourceLoadingLog() { return g_ResourceLoadingLog; }
}
