#include "GameResource/ResourceManager.h"
#include "GameResource/Implement/ResourceTextureImpl.hpp"
#include "GameResource/Implement/ResourceSpriteImpl.hpp"
#include "GameResource/Implement/ResourceAnimationImpl.hpp"
#include "GameResource/Implement/ResourceMusicImpl.hpp"
#include "GameResource/Implement/ResourceSoundEffectImpl.hpp"
#include "GameResource/Implement/ResourceParticleImpl.hpp"
#include "GameResource/Implement/ResourceFontImpl.hpp"
#include "GameResource/Implement/ResourcePostEffectShaderImpl.hpp"
#include "GameResource/Implement/ResourceModelImpl.hpp"
#include "GameResource/Implement/ResourceVideoImpl.hpp"
#include "core/FileSystem.hpp"
#include "AppFrame.h"
#include "lua/plus.hpp"

namespace luastg
{
    // 总体管理

    void ResourcePool::Clear() noexcept
    {
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

    template<typename T>
    inline void removeResource(T& pool, const char* name)
    {
        auto i = pool.find(std::string_view(name));
        if (i == pool.end())
        {
            spdlog::warn("[luastg] RemoveResource: Attempted to remove non-existing resource '{}'", name);
            return;
        }
        pool.erase(i);
        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] RemoveResource: Resource '{}' unloaded", name);
        }
    }

    const char* ResourcePool::getResourcePoolTypeName()
    {
        switch (m_iType) {
            case ResourcePoolType::Global:
                return "global";
            case ResourcePoolType::Stage:
                return "stage";
            default:
                return "none";
        }
    }

    void ResourcePool::RemoveResource(ResourceType t, const char* name) noexcept
    {
        switch (t)
        {
        case ResourceType::Texture:
            removeResource(m_TexturePool, name);
            break;
        case ResourceType::Sprite:
            removeResource(m_SpritePool, name);
            break;
        case ResourceType::Animation:
            removeResource(m_AnimationPool, name);
            break;
        case ResourceType::Music:
            removeResource(m_MusicPool, name);
            break;
        case ResourceType::SoundEffect:
            removeResource(m_SoundSpritePool, name);
            break;
        case ResourceType::Particle:
            removeResource(m_ParticlePool, name);
            break;
        case ResourceType::SpriteFont:
            removeResource(m_SpriteFontPool, name);
            break;
        case ResourceType::TrueTypeFont:
            removeResource(m_TTFFontPool, name);
            break;
        case ResourceType::FX:
            removeResource(m_FXPool, name);
            break;
        case ResourceType::Model:
            removeResource(m_ModelPool, name);
            break;
        case ResourceType::Video:
            removeResource(m_VideoPool, name);
            break;
        default:
            spdlog::warn("[luastg] RemoveResource: Attempted to remove non-existing resource ({})", (int)t);
            return;
        }
    }

    bool ResourcePool::CheckResourceExists(ResourceType t, std::string_view name) const noexcept
    {
        switch (t)
        {
        case ResourceType::Texture:
            return m_TexturePool.find(name) != m_TexturePool.end();
        case ResourceType::Sprite:
            return m_SpritePool.find(name) != m_SpritePool.end();
        case ResourceType::Animation:
            return m_AnimationPool.find(name) != m_AnimationPool.end();
        case ResourceType::Music:
            return m_MusicPool.find(name) != m_MusicPool.end();
        case ResourceType::SoundEffect:
            return m_SoundSpritePool.find(name) != m_SoundSpritePool.end();
        case ResourceType::Particle:
            return m_ParticlePool.find(name) != m_ParticlePool.end();
        case ResourceType::SpriteFont:
            return m_SpriteFontPool.find(name) != m_SpriteFontPool.end();
        case ResourceType::TrueTypeFont:
            return m_TTFFontPool.find(name) != m_TTFFontPool.end();
        case ResourceType::FX:
            return m_FXPool.find(name) != m_FXPool.end();
        case ResourceType::Model:
            return m_ModelPool.find(name) != m_ModelPool.end();
        case ResourceType::Video:
            return m_VideoPool.find(name) != m_VideoPool.end();
        default:
            spdlog::warn("[luastg] CheckRes: Attempted to index non-existing resource type ({})", (int)t);
            break;
        }
        return false;
    }

    template<typename T>
    inline bool transferResource(ResourcePool::dictionary_t<T>& src, ResourcePool::dictionary_t<T>& dst, const char* name)
    {
        auto it = src.find(std::string_view(name));
        if (it == src.end())
            return false;
        auto [dit, inserted] = dst.emplace(it->first, it->second);
        if (!inserted)
            return false;
        src.erase(it);
        return true;
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
        case ResourceType::Sprite:
            result = transferResource(m_SpritePool, dest->m_SpritePool, name);
            break;
        case ResourceType::Animation:
            result = transferResource(m_AnimationPool, dest->m_AnimationPool, name);
            break;
        case ResourceType::Music:
            result = transferResource(m_MusicPool, dest->m_MusicPool, name);
            break;
        case ResourceType::SoundEffect:
            result = transferResource(m_SoundSpritePool, dest->m_SoundSpritePool, name);
            break;
        case ResourceType::Particle:
            result = transferResource(m_ParticlePool, dest->m_ParticlePool, name);
            break;
        case ResourceType::SpriteFont:
            result = transferResource(m_SpriteFontPool, dest->m_SpriteFontPool, name);
            break;
        case ResourceType::TrueTypeFont:
            result = transferResource(m_TTFFontPool, dest->m_TTFFontPool, name);
            break;
        case ResourceType::FX:
            result = transferResource(m_FXPool, dest->m_FXPool, name);
            break;
        case ResourceType::Model:
            result = transferResource(m_ModelPool, dest->m_ModelPool, name);
            break;
        case ResourceType::Video:
            result = transferResource(m_VideoPool, dest->m_VideoPool, name);
            break;
        default:
            spdlog::warn("[luastg] TransferResource: Unknown resource type ({})", (int)t);
            return false;
        }
        if (result)
        {
            if (ResourceMgr::GetResourceLoadingLog())
                spdlog::info("[luastg] TransferResource: '{}' transferred from '{}' to '{}'", name, GetName(), dest->GetName());
        }
        else
        {
            spdlog::warn("[luastg] TransferResource: Failed to transfer '{}' from '{}' to '{}' (not found or already exists in destination)", name, GetName(), dest->GetName());
        }
        return result;
    }

    template<typename T>
    inline void listResourceName(lua_State* L, T& resource_set)
    {
        lua::stack_t S(L);
        int index = 0;
        S.create_array(resource_set.size());
        for (auto& i : resource_set)
        {
            auto ptr = i.second;
            index += 1;
            S.set_array_value<std::string_view>(index, ptr->GetResName());
        }
    }

    int ResourcePool::ExportResourceList(lua_State* L, ResourceType t) const noexcept
    {
        lua::stack_t S(L);
        switch (t)
        {
        case ResourceType::Texture:
            listResourceName(L, m_TexturePool);
            break;
        case ResourceType::Sprite:
            listResourceName(L, m_SpritePool);
            break;
        case ResourceType::Animation:
            listResourceName(L, m_AnimationPool);
            break;
        case ResourceType::Music:
            listResourceName(L, m_MusicPool);
            break;
        case ResourceType::SoundEffect:
            listResourceName(L, m_SoundSpritePool);
            break;
        case ResourceType::Particle:
            listResourceName(L, m_ParticlePool);
            break;
        case ResourceType::SpriteFont:
            listResourceName(L, m_SpriteFontPool);
            break;
        case ResourceType::TrueTypeFont:
            listResourceName(L, m_TTFFontPool);
            break;
        case ResourceType::FX:
            listResourceName(L, m_FXPool);
            break;
        case ResourceType::Model:
            listResourceName(L, m_ModelPool);
            break;
        case ResourceType::Video:
            listResourceName(L, m_VideoPool);
            break;
        default:
            spdlog::warn("[luastg] EnumRes: Attempted to enumerate through non-existing resource type ({})", (int)t);
            S.create_array(0);
            break;
        }
        return 1;
    }

    // 加载纹理

    bool ResourcePool::LoadTexture(const char* name, const char* path, bool mipmaps) noexcept
    {
        if (m_TexturePool.find(std::string_view(name)) != m_TexturePool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] LoadTexture: Texture '{}' already exists. Skipping loading", name);
            }
            return true;
        }
    
        core::SmartReference<core::Graphics::ITexture2D> p_texture;
        if (!LAPP.GetAppModel()->getDevice()->createTextureFromFile(path, mipmaps, p_texture.put()))
        {
            spdlog::error("[luastg] Failed to create texture '{}' from '{}'", path, name);
            return false;
        }

        try
        {
            core::SmartReference<IResourceTexture> tRes;
            tRes.attach(new ResourceTextureImpl(name, p_texture.get()));
            m_TexturePool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] LoadTexture: Failed to create texture '{}' ({})", name, e.what());
            return false;
        }
    
        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] LoadTexture: Texture '{}' loaded from '{}' ({})", path, name, getResourcePoolTypeName());
        }
    
        return true;
    }

    bool ResourcePool::CreateTexture(const char* name, int width, int height) noexcept
    {
        if (m_TexturePool.find(std::string_view(name)) != m_TexturePool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] LoadTexture: Texture '{}' already exists. Skipping loading", name);
            }
            return true;
        }

        core::SmartReference<core::Graphics::ITexture2D> p_texture;
        if (!LAPP.GetAppModel()->getDevice()->createTexture(core::Vector2U((uint32_t)width, (uint32_t)height), p_texture.put()))
        {
            spdlog::error("[luastg] Unable to create texture '{}' ({}x{})", name, width, height);
            return false;
        }

        try
        {
            core::SmartReference<IResourceTexture> tRes;
            tRes.attach(new ResourceTextureImpl(name, p_texture.get()));
            m_TexturePool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] LoadTexture: {}", e.what());
            return false;
        }

        if (ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadTexture: Texture created '{}' ({}x{}) ({})", name, width, height, getResourcePoolTypeName());
        }

        return true;
    }

    bool ResourcePool::StoreTexture(const char* name, core::Graphics::ITexture2D* p_texture) noexcept
    {
        m_TexturePool.erase(dictionary_key_t(name));

        try
        {
            core::SmartReference<IResourceTexture> tRes;
            tRes.attach(new ResourceTextureImpl(name, p_texture));
            m_TexturePool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] StoreTexture: Failed to store texture '{}' ({})", name, e.what());
            return false;
        }

        if (ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] StoreTexture: Texture '{}' stored ({})", name, getResourcePoolTypeName());
        }

        return true;
    }

    // 创建渲染目标

    bool ResourcePool::CreateRenderTarget(const char* name, int width, int height, bool depth_buffer) noexcept
    {
        if (m_TexturePool.find(std::string_view(name)) != m_TexturePool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] CreateRenderTarget: RenderTarget '{}' already exists. Skipping creating", name);
            }
            return true;
        }
    
        std::string_view ds_info("Depth buffer");

        try
        {
            core::SmartReference<IResourceTexture> tRes;
            if (width <= 0 || height <= 0)
            {
                tRes.attach(new ResourceTextureImpl(name, depth_buffer));
            }
            else
            {
                tRes.attach(new ResourceTextureImpl(name, width, height, depth_buffer));
            }
            m_TexturePool.emplace(name, tRes);
        }
        catch (std::runtime_error const& e)
        {
            spdlog::error("[luastg] CreateRenderTarget: Failed to create render target '{}' ({})", name, e.what());
            return false;
        }
    
        if (ResourceMgr::GetResourceLoadingLog())
        {
            if (width <= 0 || height <= 0)
            {
                spdlog::info("[luastg] CreateRenderTarget: Render target created {} '{}' ({})", ds_info, name, getResourcePoolTypeName());
            }
            else
            {
                spdlog::info("[luastg] CreateRenderTarget: Render target created {} '{}' ({}x{}) ({})", ds_info, name, width, height, getResourcePoolTypeName());
            }
        }
    
        return true;
    }

    // 创建图片精灵

    bool ResourcePool::CreateSprite(const char* name, const char* texname,
                                    double x, double y, double w, double h,
                                    double a, double b, bool rect) noexcept
    {
        if (m_SpritePool.find(std::string_view(name)) != m_SpritePool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] CreateSprite: The sprite '{}' already exists. The creation operation has been canceled", name);
            }
            return true;
        }
    
        core::SmartReference<IResourceTexture> pTex = m_pMgr->FindTexture(texname);
        if (!pTex)
        {
            spdlog::error("[luastg] CreateSprite: Unable to create sprite sheet '{}'; texture '{}' not found", name, texname);
            return false;
        }
    
        core::SmartReference<core::Graphics::ISprite> p_sprite;
        if (!core::Graphics::ISprite::create(
            LAPP.GetAppModel()->getRenderer(),
            pTex->GetTexture(),
            p_sprite.put()
        ))
        {
            spdlog::error("[luastg] Failed to create sprite sheet '{}' from texture '{}'", texname, name);
            return false;
        }
        p_sprite->setTextureRect(core::RectF((float)x, (float)y, (float)(x + w), (float)(y + h)));
        p_sprite->setTextureCenter(core::Vector2F((float)(x + w * 0.5), (float)(y + h * 0.5)));
    
        try
        {
            core::SmartReference<IResourceSprite> tRes;
            tRes.attach(new ResourceSpriteImpl(name, p_sprite.get(), a, b, rect));
            m_SpritePool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] CreateSprite: Failed to create sprite sheet '{}' ({})", name, e.what());
            return false;
        }
    
        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] CreateSprite: An image sprite '{}' has been created from the texture '{}' ({})", texname, name, getResourcePoolTypeName());
        }
    
        return true;
    }

    // 创建动画精灵

    bool ResourcePool::CreateAnimation(const char* name, const char* texname,
                                       double x, double y, double w, double h, int n, int m, int intv,
                                       double a, double b, bool rect) noexcept
    {
        if (m_AnimationPool.find(std::string_view(name)) != m_AnimationPool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] CreateAnimation: The animation sprite '{}' already exists. The creation operation has been canceled", name);
            }
            return true;
        }
    
        core::SmartReference<IResourceTexture> pTex = m_pMgr->FindTexture(texname);
        if (!pTex)
        {
            spdlog::error("[luastg] CreateAnimation: Unable to create animation sprite '{}'; texture '{}' not found", name, texname);
            return false;
        }
    
        try {
            core::SmartReference<IResourceAnimation> tRes;
            tRes.attach(
                new ResourceAnimationImpl(name, pTex,
                    (float) x, (float) y,
                    (float) w, (float) h,
                    n, m, intv,
                    a, b, rect)
            );
            m_AnimationPool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] CreateAnimation: Failed to create animation sprite '{}' ({})", name, e.what());
            return false;
        }
    
        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] CreateAnimation: Animation sprite '{}' created from '{}' ({})", texname, name, getResourcePoolTypeName());
        }
    
        return true;
    }

    bool ResourcePool::CreateAnimation(const char* name,
        std::vector<core::SmartReference<IResourceSprite>> const& sprite_list,
        int intv,
        double a, double b, bool rect) noexcept
    {
        if (m_AnimationPool.find(std::string_view(name)) != m_AnimationPool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] CreateAnimation: The animation sprite '{}' already exists. The creation operation has been canceled.", name);
            }
            return true;
        }

        try {
            core::SmartReference<IResourceAnimation> tRes;
            tRes.attach(
                new ResourceAnimationImpl(name, sprite_list, intv, a, b, rect)
            );
            m_AnimationPool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] CreateAnimation: Failed to create animation sprite '{}' ({})", name, e.what());
            return false;
        }

        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] CreateAnimation: Animation sprite '{}' has been created ({})", name, getResourcePoolTypeName());
        }

        return true;
    }

    // 加载音乐

    bool ResourcePool::LoadMusic(const char* name, const char* path, double start, double end, bool once_decode) noexcept
    {
        if (m_MusicPool.find(std::string_view(name)) != m_MusicPool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] LoadMusic: Music '{}' already exists; creation operation canceled", name);
            }
            //m_MusicPool.find(name)->second->Stop(); // 注:以前确实不判断同名资源是否存在，但是 emplace 失败了，所以没有打断旧 BGM
            return true;
        }
    
        using namespace core;

        // 创建解码器
        SmartReference<IAudioDecoder> p_decoder;
        if (!IAudioDecoder::create(path, p_decoder.put()))
        {
            spdlog::error("[luastg] LoadMusic: Unable to decode file '{}'. Requires file format to be WAV/OGG/FLAC", path);
            return false;
        }
        auto to_sample = [&p_decoder](double t) -> uint32_t
        {
            return (uint32_t)(t * (double)p_decoder->getSampleRate());
        };

        // 检查循环节
        if (0 == to_sample(start) && to_sample(start) == to_sample(end))
        {
            end = (double)p_decoder->getFrameCount() / (double)p_decoder->getSampleRate();
            spdlog::info("[luastg] LoadMusic: Set the looping range to the entire background music track (start = {}, end = {})", start, end);
        }
        if (to_sample(start) >= to_sample(end))
        {
            spdlog::error("[luastg] LoadMusic: The loop range format is incorrect; the end position cannot be equal to or before the start position (start = {}, end = {})", start, end);
            return false;
        }
    
        // 创建播放器
        SmartReference<IAudioPlayer> p_player;
        if (!once_decode)
        {
            // 流式播放器
            if (!LAPP.getAudioEngine()->createStreamAudioPlayer(p_decoder.get(), AudioMixingChannel::music, p_player.put()))
            {
                spdlog::error("[luastg] LoadMusic: Unable to create audio player");
                return false;
            }
        }
        else
        {
            // 一次性解码的播放器
            if (!LAPP.getAudioEngine()->createAudioPlayer(p_decoder.get(), AudioMixingChannel::music, p_player.put()))
            {
                spdlog::error("[luastg] LoadMusic: Unable to create audio player");
                return false;
            }
        }
        p_player->setLoop(true, start, end - start);

        try
        {
            //存入资源池
            core::SmartReference<IResourceMusic> tRes;
            tRes.attach(new ResourceMusicImpl(name, p_decoder.get(), p_player.get()));
            m_MusicPool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] LoadMusic: Failed to load music '{}' ({})", name, e.what());
            return false;
        }
    
        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] LoadMusic: Music loaded from '{}' '{}'{} ({})", path, name, once_decode ? " and decode in one go" : "", getResourcePoolTypeName());
        }
    
        return true;
    }

    // 加载音效

    bool ResourcePool::LoadSoundEffect(const char* name, const char* path) noexcept
    {
        if (m_SoundSpritePool.find(std::string_view(name)) != m_SoundSpritePool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] LoadSoundEffect: Sound effect '{}' already exists; creation operation canceled", name);
            }
            return true;
        }

        using namespace core;

        // 创建解码器
        SmartReference<IAudioDecoder> p_decoder;
        if (!IAudioDecoder::create(path, p_decoder.put()))
        {
            spdlog::error("[luastg] LoadSoundEffect: Unable to decode file '{}'. Requires file format to be WAV/OGG/FLAC", path);
            return false;
        }

        // 创建播放器
        SmartReference<IAudioPlayer> p_player;
        if (!LAPP.getAudioEngine()->createAudioPlayer(p_decoder.get(), AudioMixingChannel::sound_effect, p_player.put()))
        {
            spdlog::error("[luastg] LoadSoundEffect: 无法创建音频播放器");
            return false;
        }

        try
        {
            core::SmartReference<IResourceSoundEffect> tRes;
            tRes.attach(new ResourceSoundEffectImpl(name, p_player.get()));
            m_SoundSpritePool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] LoadSoundEffect: 加载音效 '{}' 失败 ({})", name, e.what());
            return false;
        }
    
        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] LoadSoundEffect: 已从 '{}' 加载音效 '{}' ({})", path, name, getResourcePoolTypeName());
        }
    
        return true;
    }

    // 创建粒子特效

    bool ResourcePool::LoadParticle(const char* name, const hgeParticleSystemInfo& info, const char* img_name,
                                    double a,double b, bool rect, bool _nolog) noexcept
    {
        if (m_ParticlePool.find(std::string_view(name)) != m_ParticlePool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] LoadParticle: 粒子特效 '{}' 已存在，创建操作已取消", name);
            }
            return true;
        }
    
        core::SmartReference<IResourceSprite> pSprite = m_pMgr->FindSprite(img_name);
        if (!pSprite)
        {
            spdlog::error("[luastg] LoadParticle: 无法创建粒子特效 '{}'，找不到图片精灵 '{}'", name, img_name);
            return false;
        }
    
        core::SmartReference<core::Graphics::ISprite> p_sprite;
        if (!pSprite->GetSprite()->clone(p_sprite.put()))
        {
            spdlog::error("[luastg] LoadParticle: 无法创建粒子特效 '{}'，复制图片精灵 '{}' 失败", name, img_name);
            return false;
        }

        try
        {
            core::SmartReference<IResourceParticle> tRes;
            tRes.attach(new ResourceParticleImpl(name, info, p_sprite.get(), a, b, rect));
            m_ParticlePool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] LoadParticle: 创建粒子特效 '{}' 失败 ({})", name, e.what());
            return false;
        }
    
        if (!_nolog && ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] LoadParticle: 已创建粒子特效 '{}' ({})", name, getResourcePoolTypeName());
        }
    
        return true;
    }

    bool ResourcePool::LoadParticle(const char* name, const char* path, const char* img_name,
                                    double a, double b,bool rect) noexcept
    {
        core::SmartReference<core::IData> src;
        if (!core::FileSystemManager::readFile(path, src.put()))
        {
            spdlog::error("[luastg] LoadParticle:无法从 '{}' 加载粒子特效 '{}'，读取文件失败", path, name);
            return false;
        }
    
        if (src->size() != sizeof(hgeParticleSystemInfo))
        {
            spdlog::error("[luastg] LoadParticle: 粒子特效定义文件 '{}' 格式不正确", path);
            return false;
        }
        hgeParticleSystemInfo tInfo;
        std::memcpy(&tInfo, src->data(), sizeof(hgeParticleSystemInfo));
    
        if (!LoadParticle(name, tInfo, img_name, a, b, rect, /* _nolog */ true))
        {
            return false;
        }
    
        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] LoadParticle: 已从 '{}' 创建粒子特效 '{}' ({})", path, name, getResourcePoolTypeName());
        }
    
        return true;
    }

    // 加载纹理字体（HGE）

    bool ResourcePool::LoadSpriteFont(const char* name, const char* path, bool mipmaps) noexcept
    {
        if (m_SpriteFontPool.find(std::string_view(name)) != m_SpriteFontPool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] LoadSpriteFont: 纹理字体 '{}' 已存在，加载操作已取消", name);
            }
            return true;
        }
    
        // 创建定义
        try
        {
            core::SmartReference<IResourceFont> tRes;
            tRes.attach(new ResourceFontImpl(name, path, mipmaps));
            m_SpriteFontPool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] LoadSpriteFont: 无法加载 HGE 纹理字体 '{}' ({})", name, e.what());
            return false;
        }
    
        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] LoadSpriteFont: 已从 '{}' 加载 HGE 纹理字体 '{}' ({})", path, name, getResourcePoolTypeName());
        }
    
        return true;
    }

    // 加载纹理字体（fancy2d）

    bool ResourcePool::LoadSpriteFont(const char* name, const char* path, const char* tex_path, bool mipmaps) noexcept
    {
        if (m_SpriteFontPool.find(std::string_view(name)) != m_SpriteFontPool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] LoadSpriteFont: 纹理字体 '{}' 已存在，加载操作已取消", name);
            }
            return true;
        }
    
        // 创建定义
        try
        {
            core::SmartReference<IResourceFont> tRes;
            tRes.attach(new ResourceFontImpl(name, path, tex_path, mipmaps));
            m_SpriteFontPool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] LoadSpriteFont: 无法加载 fancy2d 纹理字体 '{}' ({})", name, e.what());
            return false;
        }
    
        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] LoadSpriteFont: 已从 '{}' 和 '{}' 加载 fancy2d 纹理字体 '{}' ({})", path, tex_path, name, getResourcePoolTypeName());
        }
    
        return true;
    }

    // 加载TrueType字体

    bool ResourcePool::LoadTTFFont(const char* name, const char* path, float width, float height) noexcept
    {
        if (m_TTFFontPool.find(std::string_view(name)) != m_TTFFontPool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] LoadTTFFont: 矢量字体 '{}' 已存在，加载操作已取消", name);
            }
            return true;
        }
    
        core::SmartReference<core::Graphics::IGlyphManager> p_glyphmgr;
        core::Graphics::TrueTypeFontInfo create_info = {
            .source = path,
            .font_face = 0,
            .font_size = core::Vector2F(width, height),
            .is_force_to_file = false,
            .is_buffer = false,
        };
        if (!core::Graphics::IGlyphManager::create(LAPP.GetAppModel()->getDevice(), &create_info, 1, p_glyphmgr.put()))
        {
            spdlog::error("[luastg] LoadTTFFont: 加载矢量字体 '{}' 失败", name);
            return false;
        }

        // 创建定义
        try
        {
            core::SmartReference<IResourceFont> tRes;
            tRes.attach(new ResourceFontImpl(name, p_glyphmgr.get()));
            m_TTFFontPool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] LoadTTFFont: 无法加载矢量字体 '{}' ({})", name, e.what());
            return false;
        }
    
        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] LoadTTFFont: 已从 '{}' 加载矢量字体 '{}' ({})", path, name, getResourcePoolTypeName());
        }
    
        return true;
    }

    bool ResourcePool::LoadTrueTypeFont(const char* name, core::Graphics::TrueTypeFontInfo* fonts, size_t count) noexcept
    {
        if (m_TTFFontPool.find(std::string_view(name)) != m_TTFFontPool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] LoadTrueTypeFont: 矢量字体组 '{}' 已存在，加载操作已取消", name);
            }
            return true;
        }
    
        core::SmartReference<core::Graphics::IGlyphManager> p_glyphmgr;
        if (!core::Graphics::IGlyphManager::create(LAPP.GetAppModel()->getDevice(), fonts, count, p_glyphmgr.put()))
        {
            spdlog::error("[luastg] LoadTrueTypeFont: 加载矢量字体组 '{}' 失败", name);
            return false;
        }

        // 创建定义
        try
        {
            core::SmartReference<IResourceFont> tRes;
            tRes.attach(new ResourceFontImpl(name, p_glyphmgr.get()));
            m_TTFFontPool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] LoadTrueTypeFont: 无法加载矢量字体组 '{}' ({})", name, e.what());
            return false;
        }
    
        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] LoadTrueTypeFont: 已加载矢量字体组 '{}' ({})", name, getResourcePoolTypeName());
        }
    
        return true;
    }

    // 加载后处理特效

    bool ResourcePool::LoadFX(const char* name, const char* path) noexcept
    {
        if (m_FXPool.find(std::string_view(name)) != m_FXPool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] LoadFX: 后处理特效 '{}' 已存在，加载操作已取消", name);
            }
            return true;
        }
    
        try
        {
            core::SmartReference<IResourcePostEffectShader> tRes;
            tRes.attach(new ResourcePostEffectShaderImpl(name, path));
            if (!tRes->GetPostEffectShader())
            {
                spdlog::error("[luastg] LoadFX: 从 '{}' 加载后处理特效 '{}' 失败", path, name);
                return false;
            }
            m_FXPool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] LoadFX: 无法加载后处理特效 '{}' ({})", name, e.what());
            return false;
        }

        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] LoadFX: 已从 '{}' 加载后处理特效 '{}' ({})", path, name, getResourcePoolTypeName());
        }
    
        return true;
    }

    // 加载模型

    bool ResourcePool::LoadModel(const char* name, const char* path) noexcept
    {
        if (m_ModelPool.find(std::string_view(name)) != m_ModelPool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] LoadModel: 模型 '{}' 已存在，加载操作已取消", name);
            }
            return true;
        }
    
        try
        {
            core::SmartReference<IResourceModel> tRes;
            tRes.attach(new ResourceModelImpl(name, path));
            m_ModelPool.emplace(name, tRes);
        }
        catch (std::exception const& e)
        {
            spdlog::error("[luastg] LoadModel: 无法加载模型 '{}' ({})", name, e.what());
            return false;
        }
    
        if (ResourceMgr::GetResourceLoadingLog())
        {
            spdlog::info("[luastg] LoadModel: 已从 '{}' 加载模型 '{}' ({})", path, name, getResourcePoolTypeName());
        }
    
        return true;
    }

    bool ResourcePool::LoadVideo(const char* name, const char* path) noexcept
    {
        if (m_VideoPool.find(std::string_view(name)) != m_VideoPool.end())
        {
            if (ResourceMgr::GetResourceLoadingLog())
            {
                spdlog::warn("[luastg] LoadVideo: Video '{}' already exists. Skipping loading", name);
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
            spdlog::info("[luastg] LoadVideo: Loaded video '{}' from '{}' ({})",
                name, path, getResourcePoolTypeName());
        }

        return true;
    }

    // 查找并获取

    template<typename T>
    inline T::value_type::second_type findResource(T& resource_set, std::string_view name)
    {
        auto i = resource_set.find(name);
        if (i == resource_set.end())
            return {};
        else
            return i->second;
    }

	core::SmartReference<IResourceTexture> ResourcePool::GetTexture(std::string_view name) noexcept
    {
        return findResource(m_TexturePool, name);
	}

	core::SmartReference<IResourceSprite> ResourcePool::GetSprite(std::string_view name) noexcept
    {
        return findResource(m_SpritePool, name);
	}

	core::SmartReference<IResourceAnimation> ResourcePool::GetAnimation(std::string_view name) noexcept
    {
        return findResource(m_AnimationPool, name);
	}

	core::SmartReference<IResourceMusic> ResourcePool::GetMusic(std::string_view name) noexcept
    {
        return findResource(m_MusicPool, name);
	}

	core::SmartReference<IResourceSoundEffect> ResourcePool::GetSound(std::string_view name) noexcept
    {
        return findResource(m_SoundSpritePool, name);
	}

	core::SmartReference<IResourceParticle> ResourcePool::GetParticle(std::string_view name) noexcept
    {
        return findResource(m_ParticlePool, name);
	}

	core::SmartReference<IResourceFont> ResourcePool::GetSpriteFont(std::string_view name) noexcept
    {
        return findResource(m_SpriteFontPool, name);
	}

	core::SmartReference<IResourceFont> ResourcePool::GetTTFFont(std::string_view name) noexcept
    {
        return findResource(m_TTFFontPool, name);
	}

	core::SmartReference<IResourcePostEffectShader> ResourcePool::GetFX(std::string_view name) noexcept
    {
        return findResource(m_FXPool, name);
	}

	core::SmartReference<IResourceModel> ResourcePool::GetModel(std::string_view name) noexcept
	{
        return findResource(m_ModelPool, name);
	}

    core::SmartReference<IResourceVideo> ResourcePool::GetVideo(std::string_view name) noexcept
	{
        return findResource(m_VideoPool, name);
	}

    ResourcePool::ResourcePool(ResourceMgr* mgr, ResourcePoolType t, std::string_view name)
        : m_pMgr(mgr)
        , m_iType(t)
        , m_name(name.empty()
			? (t == ResourcePoolType::Global ? "global"
			: (t == ResourcePoolType::Stage ? "stage" : ""))
			: std::string(name))
        , m_memory_resource()
        , m_TexturePool(&m_memory_resource)
        , m_SpritePool(&m_memory_resource)
        , m_AnimationPool(&m_memory_resource)
        , m_MusicPool(&m_memory_resource)
        , m_SoundSpritePool(&m_memory_resource)
        , m_ParticlePool(&m_memory_resource)
        , m_SpriteFontPool(&m_memory_resource)
        , m_TTFFontPool(&m_memory_resource)
        , m_FXPool(&m_memory_resource)
        , m_ModelPool(&m_memory_resource)
        , m_VideoPool(&m_memory_resource)
    {

    }
}
