#pragma once
#include "GameResource/ResourceTexture.hpp"
#include "GameResource/ResourceSprite.hpp"
#include "GameResource/ResourceAnimation.hpp"
#include "GameResource/ResourceMusic.hpp"
#include "GameResource/ResourceSoundEffect.hpp"
#include "GameResource/ResourceParticle.hpp"
#include "GameResource/ResourceFont.hpp"
#include "GameResource/ResourcePostEffectShader.hpp"
#include "GameResource/ResourceModel.hpp"
#include "GameResource/ResourceVideo.hpp"
#include "lua.hpp"
#include "xxhash.h"

namespace luastg
{
    class ResourceMgr;

    enum class ResourcePoolType
    {
        None = 0,
        Global,
        Stage
    };

    class ResourcePool
    {
        friend class ResourceMgr;
        friend class AsyncResourceLoader;
    public:
        struct dictionary_key_t
        {
            XXH32_hash_t const hash{};
            XXH32_hash_t const check{};
        };
        struct dictionary_key_hash_t
        {
            inline size_t operator()(dictionary_key_t const& key) const noexcept
            {
                return key.hash;
            }
        };
        template<typename T>
        using dictionary_t = std::pmr::unordered_map<dictionary_key_t, T, dictionary_key_hash_t>;
    private:
        ResourceMgr* m_pMgr;
        ResourcePoolType m_iType;
        std::string m_name;
        std::pmr::unsynchronized_pool_resource m_memory_resource;
        dictionary_t<core::SmartReference<IResourceTexture>> m_TexturePool;
        dictionary_t<core::SmartReference<IResourceSprite>> m_SpritePool;
        dictionary_t<core::SmartReference<IResourceAnimation>> m_AnimationPool;
        dictionary_t<core::SmartReference<IResourceMusic>> m_MusicPool;
        dictionary_t<core::SmartReference<IResourceSoundEffect>> m_SoundSpritePool;
        dictionary_t<core::SmartReference<IResourceParticle>> m_ParticlePool;
        dictionary_t<core::SmartReference<IResourceFont>> m_SpriteFontPool;
        dictionary_t<core::SmartReference<IResourceFont>> m_TTFFontPool;
        dictionary_t<core::SmartReference<IResourcePostEffectShader>> m_FXPool;
        dictionary_t<core::SmartReference<IResourceModel>> m_ModelPool;
        dictionary_t<core::SmartReference<IResourceVideo>> m_VideoPool;
    public:
        void Clear() noexcept;
        void RemoveResource(ResourceType t, const char* name) noexcept;
        bool CheckResourceExists(ResourceType t, std::string_view name) const noexcept;
        int ExportResourceList(lua_State* L, ResourceType t) const noexcept;
        bool LoadVideo(const char* name, const char* path) noexcept;
        core::SmartReference<IResourceVideo> GetVideo(std::string_view name) noexcept;
        void UpdateSpritesOnRenderTargetResize(core::Graphics::ITexture2D* texture, core::Vector2U old_size, core::Vector2U new_size) noexcept;
    };

    class ResourceMgr
    {
    private:
        ResourcePoolType m_ActivedPool = ResourcePoolType::Global;
        ResourcePool m_GlobalResourcePool;
        ResourcePool m_StageResourcePool;
        std::unordered_map<std::string, std::unique_ptr<ResourcePool>> m_CustomPools;
        ResourcePool* m_pActiveCustomPool = nullptr;
        std::string m_ActiveCustomPoolName;
        mutable std::mutex m_CustomPoolsMutex;
    public:
        ResourcePoolType GetActivedPoolType() noexcept;
        void SetActivedPoolType(ResourcePoolType t) noexcept;
        ResourcePool* GetActivedPool() noexcept;
        ResourcePool* GetResourcePool(ResourcePoolType t) noexcept;
        void ClearAllResource() noexcept;
        bool CreatePool(std::string_view name);
        bool RemovePool(std::string_view name);
        ResourcePool* GetPool(std::string_view name) noexcept;
        bool SetActivedPoolByName(std::string_view name) noexcept;
        std::string GetActivedPoolName() const noexcept { return m_ActiveCustomPoolName; }
        core::SmartReference<IResourceVideo> FindVideo(const char* name) noexcept;
        void UpdateVideo();
    };
}
