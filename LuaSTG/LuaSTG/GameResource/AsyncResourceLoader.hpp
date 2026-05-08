#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <memory>
#include <atomic>
#include <functional>
#include <variant>
#include "core/SmartReference.hpp"
#include "core/Graphics/Sprite.hpp"
#include "GameResource/ResourceBase.hpp"
#include "core/Graphics/Device.hpp"

namespace core { struct IAudioDecoder; }
namespace luastg { struct ResourcePool; }

namespace luastg {
    enum class LoadingTaskStatus {
        Pending,
        Loading,
        Completed,
        Failed,
        Cancelled
    };

    struct TextureLoadParams {
        std::string path;
        bool mipmaps = true;
        int width = 0;
        int height = 0;
    };

    struct SpriteLoadParams {
        std::string texture_name;
        core::Graphics::ITexture2D* texture_object = nullptr;
        double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
        double anchor_x = 0.0, anchor_y = 0.0;
        bool is_rect = false;
    };

    struct AnimationLoadParams {
        std::string texture_name;
        double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
        int n = 1, m = 1;
        int interval = 1;
        double anchor_x = 0.0, anchor_y = 0.0;
        bool is_rect = false;
        std::vector<std::string> sprite_names;
    };

    struct MusicLoadParams {
        std::string path;
        double loop_start = 0.0;
        double loop_end = 0.0;
        bool once_decode = false;
    };

    struct SoundEffectLoadParams {
        std::string path;
    };

    struct SpriteFontLoadParams {
        std::string path;
        std::string font_tex_path;
        bool mipmaps = true;
    };

    struct TrueTypeFontLoadParams {
        std::string path;
        float font_width = 0.0f;
        float font_height = 0.0f;
    };

    struct FXLoadParams {
        std::string path;
    };

    struct ModelLoadParams {
        std::string path;
    };

    struct ParticleLoadParams {
        std::string path;
        std::string particle_img_name;
        double anchor_x = 0.0, anchor_y = 0.0;
        bool is_rect = false;
    };

    using ResourceLoadParams = std::variant<
        TextureLoadParams,
        SpriteLoadParams,
        AnimationLoadParams,
        MusicLoadParams,
        SoundEffectLoadParams,
        SpriteFontLoadParams,
        TrueTypeFontLoadParams,
        FXLoadParams,
        ModelLoadParams,
        ParticleLoadParams
    >;

    struct ResourceLoadRequest {
        ResourceType type;
        std::string name;
        ResourceLoadParams params;
        ResourcePool* target_pool = nullptr;
    };

    struct ResourceLoadResult {
        std::string name;
        ResourceType type;
        bool success = false;
        std::string error_message;

        bool registered_to_pool = false;

        core::SmartReference<IResourceBase> resource;
        core::SmartReference<core::Graphics::ITexture2D> texture;
        core::SmartReference<core::Graphics::ISprite> sprite;

        core::SmartReference<core::IAudioDecoder> audio_decoder;
        std::vector<uint8_t> file_data;

        bool requires_gpu = true;
    };

    class ResourceLoadingTask
    {
    public:
        using TaskId = uint64_t;
    
    private:
        TaskId m_id;
        std::vector<ResourceLoadRequest> m_requests;
        std::vector<ResourceLoadResult> m_results;
        std::atomic<size_t> m_completed_count;
        std::atomic<LoadingTaskStatus> m_status{LoadingTaskStatus::Pending};
        std::atomic<bool> m_cancelled{false};
        std::mutex m_mutex;

        bool m_use_resource_pool = true;
        ResourcePool* m_target_pool = nullptr;

    public:
        ResourceLoadingTask(TaskId id, std::vector<ResourceLoadRequest> requests, bool use_pool = true, ResourcePool* target_pool = nullptr);

        TaskId GetId() const { return m_id; }
        size_t GetTotalCount() const { return m_requests.size(); }
        size_t GetCompletedCount() const { return m_completed_count.load(); }
        LoadingTaskStatus GetStatus() const { return m_status.load(); }
        bool IsCancelled() const { return m_cancelled.load(); }
        bool IsCompleted() const { return m_status.load() == LoadingTaskStatus::Completed; }
        bool UseResourcePool() const { return m_use_resource_pool; }
        ResourcePool* GetTargetPool() const { return m_target_pool; }

        void Cancel() { m_cancelled.store(true); }
        
        const std::vector<ResourceLoadRequest>& GetRequests() const { return m_requests; }
        std::vector<ResourceLoadResult> GetResults();
        
        void SetStatus(LoadingTaskStatus status) { m_status.store(status); }
        void SetResult(size_t index, ResourceLoadResult result);
        void IncrementCompleted() { m_completed_count.fetch_add(1); }
    };

    class AsyncResourceLoader
    {
    private:
        static size_t GetOptimalThreadCount() noexcept;

        static constexpr size_t MIN_THREAD_COUNT = 1;
        static constexpr size_t MAX_THREAD_COUNT = 16;
        static constexpr size_t DEFAULT_THREAD_COUNT = 0;

        std::vector<std::thread> m_worker_threads;
        std::queue<std::shared_ptr<ResourceLoadingTask>> m_task_queue;
        std::mutex m_queue_mutex;
        std::condition_variable m_queue_cv;
        std::atomic<bool> m_shutdown{false};

        std::unordered_map<ResourceLoadingTask::TaskId, std::shared_ptr<ResourceLoadingTask>> m_active_tasks;
        std::mutex m_tasks_mutex;

        std::atomic<ResourceLoadingTask::TaskId> m_next_task_id{1};

        struct CompletionItem
        {
            std::shared_ptr<ResourceLoadingTask> task;
            size_t request_index;
            ResourceLoadResult result;
        };
        std::queue<CompletionItem> m_completion_queue;
        std::mutex m_completion_mutex;

        size_t m_max_gpu_items_per_frame = 5;
    public:
        AsyncResourceLoader(size_t thread_count = DEFAULT_THREAD_COUNT);
        ~AsyncResourceLoader();

        size_t GetThreadCount() const noexcept { return m_worker_threads.size(); }

        void SetMaxGPUItemsPerFrame(size_t count) noexcept { m_max_gpu_items_per_frame = count; }
        size_t GetMaxGPUItemsPerFrame() const noexcept { return m_max_gpu_items_per_frame; }

        std::shared_ptr<ResourceLoadingTask> SubmitTask(
            std::vector<ResourceLoadRequest> requests,
            bool use_resource_pool = true,
            ResourcePool* target_pool = nullptr
        );

        std::shared_ptr<ResourceLoadingTask> GetTask(ResourceLoadingTask::TaskId id);

        void Update();
        void CancelTask(ResourceLoadingTask::TaskId id);
        void WaitAll();
        void ClearAllTasks();
        void ClearTasksForPool(ResourcePool* pool);
    
    private:
        void WorkerThread();
        void ProcessRequest(
            std::shared_ptr<ResourceLoadingTask> task,
            size_t request_index
        );

        ResourceLoadResult LoadTextureWorker(const ResourceLoadRequest& request);
        ResourceLoadResult LoadSpriteWorker(const ResourceLoadRequest& request);
        ResourceLoadResult LoadAnimationWorker(const ResourceLoadRequest& request);
        ResourceLoadResult LoadMusicWorker(const ResourceLoadRequest& request);
        ResourceLoadResult LoadSoundEffectWorker(const ResourceLoadRequest& request);
        ResourceLoadResult LoadSpriteFontWorker(const ResourceLoadRequest& request);
        ResourceLoadResult LoadTrueTypeFontWorker(const ResourceLoadRequest& request);
        ResourceLoadResult LoadFXWorker(const ResourceLoadRequest& request);
        ResourceLoadResult LoadModelWorker(const ResourceLoadRequest& request);
        ResourceLoadResult LoadParticleWorker(const ResourceLoadRequest& request);
        
        void CompleteTexture(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result);
        void CompleteSprite(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result);
        void CompleteAnimation(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result);
        void CompleteMusic(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result);
        void CompleteSoundEffect(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result);
        void CompleteSpriteFont(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result);
        void CompleteTrueTypeFont(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result);
        void CompleteFX(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result);
        void CompleteModel(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result);
        void CompleteParticle(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result);
    };
}