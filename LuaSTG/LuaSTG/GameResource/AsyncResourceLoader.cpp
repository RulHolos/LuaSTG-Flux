#include "AsyncResourceLoader.hpp"
#include "GameResource/ResourceManager.h"
#include "GameResource/Implement/ResourceTextureImpl.hpp"
#include "GameResource/Implement/ResourceSpriteImpl.hpp"
#include "GameResource/Implement/ResourceAnimationImpl.hpp"
#include "GameResource/Implement/ResourceMusicImpl.hpp"
#include "GameResource/Implement/ResourceSoundEffectImpl.hpp"
#include "GameResource/Implement/ResourceFontImpl.hpp"
#include "GameResource/Implement/ResourceParticleImpl.hpp"
#include "core/FileSystem.hpp"
#include "core/AudioDecoder.hpp"
#include "AppFrame.h"
#include <spdlog/spdlog.h>

namespace luastg
{
    // ResourceLoadingTask 实现
    
    ResourceLoadingTask::ResourceLoadingTask(TaskId id, std::vector<ResourceLoadRequest> requests, bool use_pool, ResourcePool* target_pool)
        : m_id(id)
        , m_requests(std::move(requests))
        , m_use_resource_pool(use_pool)
        , m_target_pool(target_pool)
    {
        m_results.resize(m_requests.size());
    }
    
    std::vector<ResourceLoadResult> ResourceLoadingTask::GetResults()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_results;
    }
    
    void ResourceLoadingTask::SetResult(size_t index, ResourceLoadResult result)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (index < m_results.size())
        {
            m_results[index] = std::move(result);
        }
    }
    
    // AsyncResourceLoader 实现
    
    size_t AsyncResourceLoader::GetOptimalThreadCount() noexcept
    {
        // 获取 CPU 核心数
        unsigned int hardware_threads = std::thread::hardware_concurrency();
        
        if (hardware_threads == 0)
        {
            // 无法检测，使用默认值
            spdlog::warn("[luastg] AsyncResourceLoader: Unable to detect the number of CPU cores; using the default value of 1");
            return 1;
        }
        
        // 策略：
        // - 单核/双核：使用 1 个线程
        // - 四核：使用 2 个线程
        // - 六核及以上：使用 (核心数 / 2) 个线程，最多 8 个
        size_t optimal_count;
        
        if (hardware_threads <= 2)
        {
            optimal_count = 1;
        }
        else if (hardware_threads <= 4)
        {
            optimal_count = 2;
        }
        else
        {
            optimal_count = hardware_threads / 2;
            if (optimal_count > 8)
            {
                optimal_count = 8;
            }
        }
        
        spdlog::info("[luastg] AsyncResourceLoader: Detected {} CPU cores, selecting {} worker threads", 
                     hardware_threads, optimal_count);
        
        return optimal_count;
    }
    
    AsyncResourceLoader::AsyncResourceLoader(size_t thread_count)
    {
        // 如果 thread_count 为 0，自动检测
        if (thread_count == 0)
        {
            thread_count = GetOptimalThreadCount();
        }
        
        // 限制线程数范围
        if (thread_count < MIN_THREAD_COUNT)
        {
            spdlog::warn("[luastg] AsyncResourceLoader: The number of threads {} is less than the minimum value {}; use the minimum value", 
                         thread_count, MIN_THREAD_COUNT);
            thread_count = MIN_THREAD_COUNT;
        }
        else if (thread_count > MAX_THREAD_COUNT)
        {
            spdlog::warn("[luastg] AsyncResourceLoader: The number of threads {} exceeds the maximum value {}; using the maximum value", 
                         thread_count, MAX_THREAD_COUNT);
            thread_count = MAX_THREAD_COUNT;
        }
        
        for (size_t i = 0; i < thread_count; ++i)
        {
            m_worker_threads.emplace_back(&AsyncResourceLoader::WorkerThread, this);
        }
        
        spdlog::info("[luastg] AsyncResourceLoader: Started {} worker threads", thread_count);
    }
    
    AsyncResourceLoader::~AsyncResourceLoader()
    {
        m_shutdown.store(true);
        m_queue_cv.notify_all();
        
        for (auto& thread : m_worker_threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(m_completion_mutex);
            while (!m_completion_queue.empty())
            {
                m_completion_queue.pop();
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            while (!m_task_queue.empty())
            {
                m_task_queue.pop();
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(m_tasks_mutex);
            m_active_tasks.clear();
        }
        
        spdlog::info("[luastg] AsyncResourceLoader: Closed");
    }
    
    std::shared_ptr<ResourceLoadingTask> AsyncResourceLoader::SubmitTask(
        std::vector<ResourceLoadRequest> requests,
        bool use_resource_pool,
        ResourcePool* target_pool)
    {
        if (requests.empty())
        {
            return nullptr;
        }
        
        if (use_resource_pool && !target_pool)
        {
            target_pool = LRES.GetActivedPool();
        }
        
        auto task_id = m_next_task_id.fetch_add(1);
        auto task = std::make_shared<ResourceLoadingTask>(task_id, std::move(requests), use_resource_pool, target_pool);
        
        {
            std::lock_guard<std::mutex> lock(m_tasks_mutex);
            m_active_tasks[task_id] = task;
        }
        
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            m_task_queue.push(task);
        }
        
        m_queue_cv.notify_one();
        
        spdlog::info("[luastg] AsyncResourceLoader: Submitted task {} ({} resources)", 
                     task_id, task->GetTotalCount());
        
        return task;
    }
    
    std::shared_ptr<ResourceLoadingTask> AsyncResourceLoader::GetTask(ResourceLoadingTask::TaskId id)
    {
        std::lock_guard<std::mutex> lock(m_tasks_mutex);
        auto it = m_active_tasks.find(id);
        return (it != m_active_tasks.end()) ? it->second : nullptr;
    }
    
    void AsyncResourceLoader::Update()
    {
        std::vector<CompletionItem> items;
        
        {
            std::lock_guard<std::mutex> lock(m_completion_mutex);
            
            size_t gpu_count = 0;
            
            while (!m_completion_queue.empty())
            {
                auto& front_item = m_completion_queue.front();
                
                if (front_item.result.requires_gpu && gpu_count >= m_max_gpu_items_per_frame)
                {
                    break;
                }
                
                items.push_back(std::move(front_item));
                m_completion_queue.pop();
                
                if (front_item.result.requires_gpu)
                {
                    ++gpu_count;
                }
            }
        }
        
        for (auto& item : items)
        {
            switch (item.result.type)
            {
            case ResourceType::Texture:
                CompleteTexture(item.task, item.request_index, item.result);
                break;
            case ResourceType::Sprite:
                CompleteSprite(item.task, item.request_index, item.result);
                break;
            case ResourceType::Animation:
                CompleteAnimation(item.task, item.request_index, item.result);
                break;
            case ResourceType::Music:
                CompleteMusic(item.task, item.request_index, item.result);
                break;
            case ResourceType::SoundEffect:
                CompleteSoundEffect(item.task, item.request_index, item.result);
                break;
            case ResourceType::SpriteFont:
                CompleteSpriteFont(item.task, item.request_index, item.result);
                break;
            case ResourceType::TrueTypeFont:
                CompleteTrueTypeFont(item.task, item.request_index, item.result);
                break;
            case ResourceType::FX:
                CompleteFX(item.task, item.request_index, item.result);
                break;
            case ResourceType::Model:
                CompleteModel(item.task, item.request_index, item.result);
                break;
            case ResourceType::Particle:
                CompleteParticle(item.task, item.request_index, item.result);
                break;
            default:
                break;
            }
            
            item.task->SetResult(item.request_index, std::move(item.result));
            item.task->IncrementCompleted();
            
            if (item.task->GetCompletedCount() >= item.task->GetTotalCount())
            {
                item.task->SetStatus(LoadingTaskStatus::Completed);
                spdlog::info("[luastg] AsyncResourceLoader: Task {} completed", item.task->GetId());
            }
        }
    }
    
    void AsyncResourceLoader::CancelTask(ResourceLoadingTask::TaskId id)
    {
        auto task = GetTask(id);
        if (task)
        {
            task->Cancel();
            spdlog::info("[luastg] AsyncResourceLoader: Task {} cancelled", id);
        }
    }
    
    void AsyncResourceLoader::WaitAll()
    {
        while (true)
        {
            bool has_pending = false;
            
            {
                std::lock_guard<std::mutex> lock(m_tasks_mutex);
                for (auto& [id, task] : m_active_tasks)
                {
                    if (!task->IsCompleted())
                    {
                        has_pending = true;
                        break;
                    }
                }
            }
            
            if (!has_pending)
            {
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void AsyncResourceLoader::ClearAllTasks()
    {
        {
            std::lock_guard<std::mutex> lock(m_tasks_mutex);
            for (auto& [id, task] : m_active_tasks)
            {
                task->Cancel();
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            while (!m_task_queue.empty())
            {
                m_task_queue.pop();
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        {
            std::lock_guard<std::mutex> lock(m_completion_mutex);
            while (!m_completion_queue.empty())
            {
                m_completion_queue.pop();
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(m_tasks_mutex);
            m_active_tasks.clear();
        }
        
        spdlog::info("[luastg] AsyncResourceLoader: Cleared all tasks");
    }
    
    void AsyncResourceLoader::ClearTasksForPool(ResourcePool* pool)
    {
        if (!pool)
        {
            return;
        }
        
        std::vector<ResourceLoadingTask::TaskId> tasks_to_cancel;
        
        {
            std::lock_guard<std::mutex> lock(m_tasks_mutex);
            for (auto& [id, task] : m_active_tasks)
            {
                if (task->UseResourcePool())
                {
                    tasks_to_cancel.push_back(id);
                }
            }
        }
        
        for (auto id : tasks_to_cancel)
        {
            CancelTask(id);
        }
        
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            std::queue<std::shared_ptr<ResourceLoadingTask>> new_queue;
            while (!m_task_queue.empty())
            {
                auto task = m_task_queue.front();
                m_task_queue.pop();
                if (!task->UseResourcePool())
                {
                    new_queue.push(task);
                }
            }
            m_task_queue = std::move(new_queue);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        {
            std::lock_guard<std::mutex> lock(m_completion_mutex);
            std::queue<CompletionItem> new_queue;
            while (!m_completion_queue.empty())
            {
                auto item = m_completion_queue.front();
                m_completion_queue.pop();
                if (!item.task->UseResourcePool())
                {
                    new_queue.push(std::move(item));
                }
            }
            m_completion_queue = std::move(new_queue);
        }
        
        {
            std::lock_guard<std::mutex> lock(m_tasks_mutex);
            for (auto it = m_active_tasks.begin(); it != m_active_tasks.end();)
            {
                if (it->second->UseResourcePool() && it->second->IsCancelled())
                {
                    it = m_active_tasks.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
        
        spdlog::info("[luastg] AsyncResourceLoader: Cleared resource pool related tasks");
    }
    
    void AsyncResourceLoader::WorkerThread()
    {
        while (!m_shutdown.load())
        {
            std::shared_ptr<ResourceLoadingTask> task;
            
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                m_queue_cv.wait(lock, [this] {
                    return m_shutdown.load() || !m_task_queue.empty();
                });
                
                if (m_shutdown.load())
                {
                    break;
                }
                
                if (!m_task_queue.empty())
                {
                    task = m_task_queue.front();
                    m_task_queue.pop();
                }
            }
            
            if (task)
            {
                task->SetStatus(LoadingTaskStatus::Loading);
                
                const auto& requests = task->GetRequests();
                for (size_t i = 0; i < requests.size(); ++i)
                {
                    if (task->IsCancelled())
                    {
                        task->SetStatus(LoadingTaskStatus::Cancelled);
                        break;
                    }
                    
                    ProcessRequest(task, i);
                }
            }
        }
    }
    
    void AsyncResourceLoader::ProcessRequest(
        std::shared_ptr<ResourceLoadingTask> task,
        size_t request_index)
    {
        const auto& request = task->GetRequests()[request_index];
        ResourceLoadResult result;
        result.name = request.name;
        result.type = request.type;
        result.requires_gpu = true;
        
        try
        {
            switch (request.type)
            {
            case ResourceType::Texture:
                result = LoadTextureWorker(request);
                break;
            case ResourceType::Sprite:
                result = LoadSpriteWorker(request);
                break;
            case ResourceType::Animation:
                result = LoadAnimationWorker(request);
                break;
            case ResourceType::Music:
                result = LoadMusicWorker(request);
                break;
            case ResourceType::SoundEffect:
                result = LoadSoundEffectWorker(request);
                break;
            case ResourceType::SpriteFont:
                result = LoadSpriteFontWorker(request);
                break;
            case ResourceType::TrueTypeFont:
                result = LoadTrueTypeFontWorker(request);
                break;
            case ResourceType::FX:
                result = LoadFXWorker(request);
                break;
            case ResourceType::Model:
                result = LoadModelWorker(request);
                break;
            case ResourceType::Particle:
                result = LoadParticleWorker(request);
                break;
            default:
                result.success = false;
                result.error_message = "Unsupported resource type";
                break;
            }
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.error_message = e.what();
        }
        
        // 将结果放入完成队列
        {
            std::lock_guard<std::mutex> lock(m_completion_mutex);
            m_completion_queue.push({task, request_index, std::move(result)});
        }
    }
    
    // Worker 函数（在工作线程中执行，只做 CPU 端工作）
    
    ResourceLoadResult AsyncResourceLoader::LoadTextureWorker(const ResourceLoadRequest& request)
    {
        ResourceLoadResult result;
        result.name = request.name;
        result.type = ResourceType::Texture;
        
        const auto& params = std::get<TextureLoadParams>(request.params);
        
        // 检查文件是否存在
        if (!params.path.empty() && !core::FileSystemManager::hasFile(params.path))
        {
            result.success = false;
            result.error_message = "File not found: " + params.path;
            return result;
        }
        
        result.success = true;
        return result;
    }
    
    ResourceLoadResult AsyncResourceLoader::LoadSpriteWorker(const ResourceLoadRequest& request)
    {
        ResourceLoadResult result;
        result.name = request.name;
        result.type = ResourceType::Sprite;
        result.requires_gpu = false;
        
        result.success = true;
        return result;
    }
    
    ResourceLoadResult AsyncResourceLoader::LoadAnimationWorker(const ResourceLoadRequest& request)
    {
        ResourceLoadResult result;
        result.name = request.name;
        result.type = ResourceType::Animation;
        result.requires_gpu = false;
        
        result.success = true;
        return result;
    }
    
    ResourceLoadResult AsyncResourceLoader::LoadMusicWorker(const ResourceLoadRequest& request)
    {
        ResourceLoadResult result;
        result.name = request.name;
        result.type = ResourceType::Music;
        result.requires_gpu = false;
        
        const auto& params = std::get<MusicLoadParams>(request.params);
        
        if (!core::FileSystemManager::hasFile(params.path))
        {
            result.success = false;
            result.error_message = "File not found: " + params.path;
            return result;
        }
        
        if (!core::IAudioDecoder::create(params.path.c_str(), result.audio_decoder.put()))
        {
            result.success = false;
            result.error_message = "Failed to create audio decoder";
            return result;
        }
        
        result.success = true;
        return result;
    }
    
    ResourceLoadResult AsyncResourceLoader::LoadSoundEffectWorker(const ResourceLoadRequest& request)
    {
        ResourceLoadResult result;
        result.name = request.name;
        result.type = ResourceType::SoundEffect;
        result.requires_gpu = false;
        
        const auto& params = std::get<SoundEffectLoadParams>(request.params);
        
        if (!core::FileSystemManager::hasFile(params.path))
        {
            result.success = false;
            result.error_message = "File not found: " + params.path;
            return result;
        }
        
        if (!core::IAudioDecoder::create(params.path.c_str(), result.audio_decoder.put()))
        {
            result.success = false;
            result.error_message = "Failed to create audio decoder";
            return result;
        }
        
        result.success = true;
        return result;
    }
    
    ResourceLoadResult AsyncResourceLoader::LoadSpriteFontWorker(const ResourceLoadRequest& request)
    {
        ResourceLoadResult result;
        result.name = request.name;
        result.type = request.type;
        
        const auto& params = std::get<SpriteFontLoadParams>(request.params);
        
        if (!core::FileSystemManager::hasFile(params.path))
        {
            result.success = false;
            result.error_message = "File not found: " + params.path;
            return result;
        }
        
        // 如果有 tex_path，也需要检查
        if (!params.font_tex_path.empty() && !core::FileSystemManager::hasFile(params.font_tex_path))
        {
            result.success = false;
            result.error_message = "Texture file not found: " + params.font_tex_path;
            return result;
        }
        
        result.success = true;
        return result;
    }
    
    ResourceLoadResult AsyncResourceLoader::LoadTrueTypeFontWorker(const ResourceLoadRequest& request)
    {
        ResourceLoadResult result;
        result.name = request.name;
        result.type = request.type;
        
        const auto& params = std::get<TrueTypeFontLoadParams>(request.params);
        
        if (!core::FileSystemManager::hasFile(params.path))
        {
            result.success = false;
            result.error_message = "File not found: " + params.path;
            return result;
        }
        
        if (params.font_width <= 0.0f || params.font_height <= 0.0f)
        {
            result.success = false;
            result.error_message = "Invalid font size: width and height must be positive";
            return result;
        }
        
        result.success = true;
        return result;
    }
    
    ResourceLoadResult AsyncResourceLoader::LoadFXWorker(const ResourceLoadRequest& request)
    {
        ResourceLoadResult result;
        result.name = request.name;
        result.type = request.type;
        
        const auto& params = std::get<FXLoadParams>(request.params);
        
        if (!core::FileSystemManager::hasFile(params.path))
        {
            result.success = false;
            result.error_message = "File not found: " + params.path;
            return result;
        }
        
        result.success = true;
        return result;
    }
    
    ResourceLoadResult AsyncResourceLoader::LoadModelWorker(const ResourceLoadRequest& request)
    {
        ResourceLoadResult result;
        result.name = request.name;
        result.type = request.type;
        
        const auto& params = std::get<ModelLoadParams>(request.params);
        
        if (!core::FileSystemManager::hasFile(params.path))
        {
            result.success = false;
            result.error_message = "File not found: " + params.path;
            return result;
        }
        
        result.success = true;
        return result;
    }
    
    ResourceLoadResult AsyncResourceLoader::LoadParticleWorker(const ResourceLoadRequest& request)
    {
        ResourceLoadResult result;
        result.name = request.name;
        result.type = ResourceType::Particle;
        result.requires_gpu = false;
        
        const auto& params = std::get<ParticleLoadParams>(request.params);
        
        if (!core::FileSystemManager::hasFile(params.path))
        {
            result.success = false;
            result.error_message = "File not found: " + params.path;
            return result;
        }
        
        result.success = true;
        return result;
    }
    
    // Complete 函数（在主线程中执行，创建 GPU 资源和注册到资源池）
    
    void AsyncResourceLoader::CompleteTexture(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result)
    {
        if (!result.success)
        {
            return;
        }
        
        const auto& request = task->GetRequests()[index];
        const auto& params = std::get<TextureLoadParams>(request.params);
        
        try
        {
            if (task->UseResourcePool())
            {
                // 使用资源池 API
                auto pool = request.target_pool ? request.target_pool : task->GetTargetPool();
                if (!pool)
                {
                    result.success = false;
                    result.error_message = "No active resource pool";
                    return;
                }
                
                // 调用资源池的加载函数
                if (!params.path.empty())
                {
                    result.success = pool->LoadTexture(request.name.c_str(), params.path.c_str(), params.mipmaps);
                }
                else if (params.width > 0 && params.height > 0)
                {
                    result.success = pool->CreateTexture(request.name.c_str(), params.width, params.height);
                }
                else
                {
                    result.success = false;
                    result.error_message = "Invalid texture parameters";
                }
                
                if (result.success)
                {
                    result.registered_to_pool = true;
                }
                else
                {
                    result.error_message = "Failed to load texture to pool";
                }
            }
            else
            {
                core::SmartReference<core::Graphics::ITexture2D> p_texture;
                if (!LAPP.GetAppModel()->getDevice()->createTextureFromFile(params.path.c_str(), params.mipmaps, p_texture.put()))
                {
                    result.success = false;
                    result.error_message = "Failed to create texture from file";
                    return;
                }
                
                result.texture = p_texture;
                result.success = true;
            }
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.error_message = e.what();
        }
    }
    
    void AsyncResourceLoader::CompleteSprite(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result)
    {
        if (!result.success)
        {
            return;
        }
        
        const auto& request = task->GetRequests()[index];
        const auto& params = std::get<SpriteLoadParams>(request.params);
        
        try
        {
            if (task->UseResourcePool())
            {
                auto pool = request.target_pool ? request.target_pool : task->GetTargetPool();
                if (!pool)
                {
                    result.success = false;
                    result.error_message = "No active resource pool";
                    return;
                }
                
                result.success = pool->CreateSprite(
                    request.name.c_str(),
                    params.texture_name.c_str(),
                    params.x, params.y, params.w, params.h,
                    params.anchor_x, params.anchor_y,
                    params.is_rect
                );
                
                if (result.success)
                {
                    result.registered_to_pool = true;
                }
                else
                {
                    result.error_message = "Failed to create sprite";
                }
            }
            else
            {
                if (!params.texture_object)
                {
                    result.success = false;
                    result.error_message = "No texture object provided for modern API sprite";
                    return;
                }
                
                // 创建精灵对象
                core::SmartReference<core::Graphics::ISprite> p_sprite;
                if (!core::Graphics::ISprite::create(LAPP.GetAppModel()->getRenderer(), params.texture_object, p_sprite.put()))
                {
                    result.success = false;
                    result.error_message = "Failed to create sprite from texture object";
                    return;
                }
                
                // 设置纹理矩形
                p_sprite->setTextureRect(core::RectF(
                    static_cast<float>(params.x),
                    static_cast<float>(params.y),
                    static_cast<float>(params.w),
                    static_cast<float>(params.h)
                ));
                
                // 设置锚点
                p_sprite->setTextureCenter(core::Vector2F(
                    static_cast<float>(params.anchor_x),
                    static_cast<float>(params.anchor_y)
                ));
                
                result.sprite = p_sprite;
                result.success = true;
            }
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.error_message = e.what();
        }
    }
    
    void AsyncResourceLoader::CompleteAnimation(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result)
    {
        if (!result.success)
        {
            return;
        }
        
        const auto& request = task->GetRequests()[index];
        const auto& params = std::get<AnimationLoadParams>(request.params);
        
        try
        {
            if (task->UseResourcePool())
            {
                auto pool = request.target_pool ? request.target_pool : task->GetTargetPool();
                if (!pool)
                {
                    result.success = false;
                    result.error_message = "No active resource pool";
                    return;
                }
                
                if (params.sprite_names.empty())
                {
                    // 从纹理创建
                    result.success = pool->CreateAnimation(
                        request.name.c_str(),
                        params.texture_name.c_str(),
                        params.x, params.y, params.w, params.h,
                        params.n, params.m, params.interval,
                        params.anchor_x, params.anchor_y,
                        params.is_rect
                    );
                }
                else
                {
                    // 从精灵列表创建
                    std::vector<core::SmartReference<IResourceSprite>> sprites;
                    for (const auto& sprite_name : params.sprite_names)
                    {
                        auto sprite = LRES.FindSprite(sprite_name.c_str());
                        if (!sprite)
                        {
                            result.success = false;
                            result.error_message = "Sprite not found: " + sprite_name;
                            return;
                        }
                        sprites.push_back(sprite);
                    }
                    
                    result.success = pool->CreateAnimation(
                        request.name.c_str(),
                        sprites,
                        params.interval,
                        params.anchor_x, params.anchor_y,
                        params.is_rect
                    );
                }
                
                if (result.success)
                {
                    result.registered_to_pool = true;
                }
                else
                {
                    result.error_message = "Failed to create animation";
                }
            }
            else
            {
                result.success = false;
                result.error_message = "Modern API animation creation not implemented in async loader";
            }
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.error_message = e.what();
        }
    }
    
    void AsyncResourceLoader::CompleteMusic(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result)
    {
        if (!result.success)
        {
            return;
        }
        
        const auto& request = task->GetRequests()[index];
        const auto& params = std::get<MusicLoadParams>(request.params);
        
        try
        {
            if (task->UseResourcePool())
            {
                auto pool = request.target_pool ? request.target_pool : task->GetTargetPool();
                if (!pool)
                {
                    result.success = false;
                    result.error_message = "No active resource pool";
                    return;
                }
                
                if (!result.audio_decoder)
                {
                    result.success = false;
                    result.error_message = "No audio decoder from worker thread";
                    return;
                }
                
                if (pool->CheckResourceExists(ResourceType::Music, request.name))
                {
                    result.success = true;
                    result.registered_to_pool = true;
                    return;
                }
                
                auto to_sample = [&result](double t) -> uint32_t {
                    return (uint32_t)(t * (double)result.audio_decoder->getSampleRate());
                };
                
                double start = params.loop_start;
                double end = params.loop_end;
                
                if (0 == to_sample(start) && to_sample(start) == to_sample(end))
                {
                    end = (double)result.audio_decoder->getFrameCount() / (double)result.audio_decoder->getSampleRate();
                }
                
                if (to_sample(start) >= to_sample(end))
                {
                    result.success = false;
                    result.error_message = "Invalid loop range";
                    return;
                }
                
                core::SmartReference<core::IAudioPlayer> p_player;
                if (!params.once_decode)
                {
                    if (!LAPP.getAudioEngine()->createStreamAudioPlayer(result.audio_decoder.get(), core::AudioMixingChannel::music, p_player.put()))
                    {
                        result.success = false;
                        result.error_message = "Failed to create stream audio player";
                        return;
                    }
                }
                else
                {
                    if (!LAPP.getAudioEngine()->createAudioPlayer(result.audio_decoder.get(), core::AudioMixingChannel::music, p_player.put()))
                    {
                        result.success = false;
                        result.error_message = "Failed to create audio player";
                        return;
                    }
                }
                p_player->setLoop(true, start, end - start);
                
                core::SmartReference<IResourceMusic> tRes;
                tRes.attach(new ResourceMusicImpl(request.name.c_str(), result.audio_decoder.get(), p_player.get()));
                
                pool->m_MusicPool.emplace(request.name.c_str(), tRes);
                
                result.success = true;
                result.registered_to_pool = true;
            }
            else
            {
                result.success = false;
                result.error_message = "Modern API music loading not implemented in async loader";
            }
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.error_message = e.what();
        }
    }
    
    void AsyncResourceLoader::CompleteSoundEffect(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result)
    {
        if (!result.success)
        {
            return;
        }
        
        const auto& request = task->GetRequests()[index];
        const auto& params = std::get<SoundEffectLoadParams>(request.params);
        
        try
        {
            if (task->UseResourcePool())
            {
                auto pool = request.target_pool ? request.target_pool : task->GetTargetPool();
                if (!pool)
                {
                    result.success = false;
                    result.error_message = "No active resource pool";
                    return;
                }
                
                if (!result.audio_decoder)
                {
                    result.success = false;
                    result.error_message = "No audio decoder from worker thread";
                    return;
                }
                
                if (pool->CheckResourceExists(ResourceType::SoundEffect, request.name))
                {
                    result.success = true;
                    result.registered_to_pool = true;
                    return;
                }
                
                core::SmartReference<core::IAudioPlayer> p_player;
                if (!LAPP.getAudioEngine()->createAudioPlayer(result.audio_decoder.get(), core::AudioMixingChannel::sound_effect, p_player.put()))
                {
                    result.success = false;
                    result.error_message = "Failed to create audio player";
                    return;
                }
                
                core::SmartReference<IResourceSoundEffect> tRes;
                tRes.attach(new ResourceSoundEffectImpl(request.name.c_str(), p_player.get()));
                
                pool->m_SoundSpritePool.emplace(request.name.c_str(), tRes);
                
                result.success = true;
                result.registered_to_pool = true;
            }
            else
            {
                result.success = false;
                result.error_message = "Modern API sound effect loading not implemented in async loader";
            }
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.error_message = e.what();
        }
    }
    
    void AsyncResourceLoader::CompleteSpriteFont(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result)
    {
        if (!result.success)
        {
            return;
        }
        
        const auto& request = task->GetRequests()[index];
        const auto& params = std::get<SpriteFontLoadParams>(request.params);
        
        try
        {
            if (task->UseResourcePool())
            {
                auto pool = request.target_pool ? request.target_pool : task->GetTargetPool();
                if (!pool)
                {
                    result.success = false;
                    result.error_message = "No active resource pool";
                    return;
                }
                
                if (params.font_tex_path.empty())
                {
                    result.success = pool->LoadSpriteFont(
                        request.name.c_str(),
                        params.path.c_str(),
                        params.mipmaps
                    );
                }
                else
                {
                    result.success = pool->LoadSpriteFont(
                        request.name.c_str(),
                        params.path.c_str(),
                        params.font_tex_path.c_str(),
                        params.mipmaps
                    );
                }
                
                if (result.success)
                {
                    result.registered_to_pool = true;
                }
                else
                {
                    result.error_message = "Failed to load sprite font";
                }
            }
            else
            {
                result.success = false;
                result.error_message = "Modern API sprite font loading not implemented in async loader";
            }
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.error_message = e.what();
        }
    }
    
    void AsyncResourceLoader::CompleteTrueTypeFont(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result)
    {
        if (!result.success)
        {
            return;
        }
        
        const auto& request = task->GetRequests()[index];
        const auto& params = std::get<TrueTypeFontLoadParams>(request.params);
        
        try
        {
            if (task->UseResourcePool())
            {
                auto pool = request.target_pool ? request.target_pool : task->GetTargetPool();
                if (!pool)
                {
                    result.success = false;
                    result.error_message = "No active resource pool";
                    return;
                }
                
                result.success = pool->LoadTTFFont(
                    request.name.c_str(),
                    params.path.c_str(),
                    params.font_width,
                    params.font_height
                );
                
                if (result.success)
                {
                    result.registered_to_pool = true;
                }
                else
                {
                    result.error_message = "Failed to load TrueType font";
                }
            }
            else
            {
                result.success = false;
                result.error_message = "Modern API TrueType font loading not implemented in async loader";
            }
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.error_message = e.what();
        }
    }
    
    void AsyncResourceLoader::CompleteFX(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result)
    {
        if (!result.success)
        {
            return;
        }
        
        const auto& request = task->GetRequests()[index];
        const auto& params = std::get<FXLoadParams>(request.params);
        
        try
        {
            if (task->UseResourcePool())
            {
                auto pool = request.target_pool ? request.target_pool : task->GetTargetPool();
                if (!pool)
                {
                    result.success = false;
                    result.error_message = "No active resource pool";
                    return;
                }
                
                result.success = pool->LoadFX(
                    request.name.c_str(),
                    params.path.c_str()
                );
                
                if (result.success)
                {
                    result.registered_to_pool = true;
                }
                else
                {
                    result.error_message = "Failed to load FX";
                }
            }
            else
            {
                result.success = false;
                result.error_message = "Modern API FX loading not implemented in async loader";
            }
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.error_message = e.what();
        }
    }
    
    void AsyncResourceLoader::CompleteModel(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result)
    {
        if (!result.success)
        {
            return;
        }
        
        const auto& request = task->GetRequests()[index];
        const auto& params = std::get<ModelLoadParams>(request.params);
        
        try
        {
            if (task->UseResourcePool())
            {
                auto pool = request.target_pool ? request.target_pool : task->GetTargetPool();
                if (!pool)
                {
                    result.success = false;
                    result.error_message = "No active resource pool";
                    return;
                }
                
                result.success = pool->LoadModel(
                    request.name.c_str(),
                    params.path.c_str()
                );
                
                if (result.success)
                {
                    result.registered_to_pool = true;
                }
                else
                {
                    result.error_message = "Failed to load model";
                }
            }
            else
            {
                result.success = false;
                result.error_message = "Modern API model loading not implemented in async loader";
            }
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.error_message = e.what();
        }
    }
    
    void AsyncResourceLoader::CompleteParticle(std::shared_ptr<ResourceLoadingTask> task, size_t index, ResourceLoadResult& result)
    {
        if (!result.success)
        {
            return;
        }
        
        const auto& request = task->GetRequests()[index];
        const auto& params = std::get<ParticleLoadParams>(request.params);
        
        try
        {
            if (task->UseResourcePool())
            {
                auto pool = request.target_pool ? request.target_pool : task->GetTargetPool();
                if (!pool)
                {
                    result.success = false;
                    result.error_message = "No active resource pool";
                    return;
                }
                
                result.success = pool->LoadParticle(
                    request.name.c_str(),
                    params.path.c_str(),
                    params.particle_img_name.c_str(),
                    params.anchor_x,
                    params.anchor_y,
                    params.is_rect
                );
                
                if (result.success)
                {
                    result.registered_to_pool = true;
                }
                else
                {
                    result.error_message = "Failed to load particle";
                }
            }
            else
            {
                result.success = false;
                result.error_message = "Modern API particle loading not implemented in async loader";
            }
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.error_message = e.what();
        }
    }
}