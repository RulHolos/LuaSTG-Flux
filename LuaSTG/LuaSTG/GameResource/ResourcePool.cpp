void ResourcePool::Clear() noexcept
{
    if (auto* loader = LAPP.GetAsyncResourceLoader())
    {
        loader->ClearTasksForPool(this);
    }

    m_TexturePool.clear();
    m_SpritePool.clear();
    m_AnimationPool.clear();
    m_MusicPool.clear();
    m_SoundSpritePool.clear();
    m_ParticlePool.clear();
    m_SpriteFontPool.clear();
    m_TTFFontPool.clear();
    m_FXPool.clear();
    m_ModelPool.clear();
    m_VideoPool.clear();
    spdlog::info("[luastg] Resource pool '{}' cleared", getResourcePoolTypeName());
}

bool ResourcePool::LoadVideo(const char* name, const char* path) noexcept
{
    if (m_VideoPool.find(std::string_view(name)) != m_VideoPool.end())
    {
        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::warn("[luastg] LoadVideo: Video '{}' already exists; loading operation canceled", name);
        }
        return true;
    }

    try
    {
        core::SmartReference<IResourceVideo> tRes;
        if (!ResourceVideoImpl::CreateFromFile(name, path, tRes))
        {
            return false;
        }
        m_VideoPool.emplace(name, tRes);
    }
    catch (std::exception const& e)
    {
        spdlog::error("[luastg] LoadVideo: Failed to load video '{}' ({})", name, e.what());
        return false;
    }

    if (ResourceMgr::GetResourceLoadingLog())
    {
        spdlog::info("[luastg] LoadVideo: Loaded video '{}' from '{}' ({})", name, path, getResourcePoolTypeName());
    }

    return true;
}

core::SmartReference<IResourceVideo> ResourcePool::GetVideo(std::string_view name) noexcept
{
    auto i = m_VideoPool.find(name);
    if (i == m_VideoPool.end())
        return {};
    return i->second;
}

bool ResourcePool::TransferResourceTo(ResourceType t, const char* name, ResourcePool* dest) noexcept
{
    if (!dest || dest == this)
        return false;
    bool result = false;
    switch (t)
    {
    case ResourceType::Texture:
        result = transferResource(m_TexturePool, dest->m_TexturePool, name);
        break;
    case ResourceType::Video:
        result = transferResource(m_VideoPool, dest->m_VideoPool, name);
        break;
    default:
        break;
    }
    return result;
}
