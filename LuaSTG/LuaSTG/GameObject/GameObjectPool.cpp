#include "GameObject/GameObjectPool.h"
#include "LuaBinding/LuaWrapper.hpp"
#include "LuaBinding/modern/GameObject.hpp"
#include "lua/plus.hpp"
#include "AppFrame.h"
#include "iostream"

using std::string_view_literals::operator ""sv;

namespace {
	constexpr auto queue_to_destroy_reason_out_of_world_bound{ "luastg:leave_world_border"sv };
}

namespace luastg
{
	// Manager for storage of named pools
	static std::unordered_map<std::string, std::unique_ptr<GameObjectPool>> g_gameobject_pools;
	static GameObjectPool* g_active_gameobject_pool = nullptr;
	static std::mutex g_gameobject_pools_mutex; // jic

	// --------------------------------------------------------------------------------

	GameObjectPool::GameObjectPool() {
		// initialize GameObject list

		m_render_list = decltype(m_render_list){ &m_memory_resource };
		m_callbacks = decltype(m_callbacks){&m_memory_resource};
		resetGameObjectLists();

		// ex+

#ifdef USING_MULTI_GAME_WORLD
		m_pCurrentObject = nullptr;
#endif // USING_MULTI_GAME_WORLD
		m_superpause = 0;
		m_nextsuperpause = 0;
	}
	GameObjectPool::~GameObjectPool()
	{
		ResetPool();
		// If this instance was the global pointer or active pools, clear this fucker
		{
			std::lock_guard<std::mutex> lk(g_gameobject_pools_mutex);
			if (g_active_gameobject_pool == this)
				g_active_gameobject_pool = nullptr;
			
			for (auto it = g_gameobject_pools.begin(); it != g_gameobject_pools.end(); ++it)
			{
				if (it->second.get() == this)
				{
					g_gameobject_pools.erase(it);
					break;
				}
			}
		}
	}

	void GameObjectPool::resetGameObjectLists() {
		m_update_list.clear();
		m_render_list.clear();
		for (auto &list : m_detect_lists) {
			list.clear();
		}
	}

	// --------------------------------------------------------------------------------

	void GameObjectPool::DebugNextFrame()
	{
		m_statistics_index = (m_statistics_index + 1) % std::size(m_statistics);
		m_statistics[m_statistics_index].object_alloc = 0;
		m_statistics[m_statistics_index].object_free = 0;
		m_statistics[m_statistics_index].object_alive = m_ObjectPool.size();
		m_statistics[m_statistics_index].object_colli_check = 0;
		m_statistics[m_statistics_index].object_colli_callback = 0;
	}
	GameObjectPool::FrameStatistics GameObjectPool::DebugGetFrameStatistics()
	{
		size_t const n = std::size(m_statistics);
		size_t const i = (m_statistics_index + n - 1) % n;
		return m_statistics[i];
	}

	void GameObjectPool::ResetPool() noexcept
	{
		// 回收已分配的对象和更新链表
		dispatchOnBeforeBatchDestroy();
		for (auto p = m_update_list.first(); p != nullptr;) {
			p = freeWithCallbacks(p);
		}
		dispatchOnAfterBatchDestroy();
		// 重置其他链表
		resetGameObjectLists();
		// 重置整个对象池，恢复为线性状态
		m_ObjectPool.clear();
		// 重置其他数据
#ifdef USING_MULTI_GAME_WORLD
		m_iWorld = 0x00000001;
		m_ActiveWorldMask = 0xFFFFFFFF;
		m_pCurrentObject = nullptr;
#endif // USING_MULTI_GAME_WORLD
		m_LockObjectA = nullptr;
		m_LockObjectB = nullptr;
		m_superpause = 0;
		m_nextsuperpause = 0;
		// 清理内存
		m_memory_resource.release();
	}
	void GameObjectPool::updateMovementsLegacy() {
		tracy_zone_scoped_with_name("LOBJMGR.ObjFrame");
		dispatchOnBeforeBatchUpdate();
		auto const super_pause_time = UpdateSuperPause(); // 更新超级暂停
		for (auto p = m_update_list.first(); p != nullptr; p = p->update_list_next) {
			if (super_pause_time > 0 && !p->ignore_super_pause) {
				continue;
			}
			if (p->features.has_callback_update) {
			#ifdef USING_MULTI_GAME_WORLD
				m_pCurrentObject = p;
			#endif // USING_MULTI_GAME_WORLD
				p->dispatchOnUpdate();
			#ifdef USING_MULTI_GAME_WORLD
				m_pCurrentObject = nullptr;
			#endif // USING_MULTI_GAME_WORLD
			}
			p->Update();
		}
		dispatchOnAfterBatchUpdate();
	}
	void GameObjectPool::updateMovements() {
		tracy_zone_scoped_with_name("LOBJMGR.ObjFrame(New)");

		dispatchOnBeforeBatchUpdate();

		auto const super_pause_time = GetSuperPauseTime();
		for (auto p = m_update_list.first(); p != nullptr; p = p->update_list_next) {
			if (super_pause_time > 0 && !p->ignore_super_pause) {
				continue;
			}
			if (p->features.has_callback_update) {
			#ifdef USING_MULTI_GAME_WORLD
				m_pCurrentObject = p;
			#endif // USING_MULTI_GAME_WORLD
				p->dispatchOnUpdate();
			#ifdef USING_MULTI_GAME_WORLD
				m_pCurrentObject = nullptr;
			#endif // USING_MULTI_GAME_WORLD
			}
		}

		dispatchOnAfterBatchUpdate();

		for (auto p = m_update_list.first(); p != nullptr; p = p->update_list_next) {
			if (super_pause_time > 0 && !p->ignore_super_pause) {
				continue;
			}
			p->UpdateV2();
		}
	}
	void GameObjectPool::render() {
		m_is_rendering = true;
		dispatchOnBeforeBatchRender();
#ifdef USING_MULTI_GAME_WORLD
		m_pCurrentObject = nullptr;
		auto const world = GetWorldFlag();
#endif // USING_MULTI_GAME_WORLD

		for (auto& p : m_render_list) {
#ifdef USING_MULTI_GAME_WORLD
			if (!p->hide && CheckWorlds(p->world, world)) { // 只渲染可见对象
				m_pCurrentObject = p;
#else // USING_MULTI_GAME_WORLD
			if (!p->hide) {  // 只渲染可见对象
#endif // USING_MULTI_GAME_WORLD
				if (p->features.has_callback_render) {
					p->dispatchOnRender();
				}
				else {
					p->Render();
				}
			}
		}

#ifdef USING_MULTI_GAME_WORLD
		m_pCurrentObject = nullptr;
#endif // USING_MULTI_GAME_WORLD
		dispatchOnAfterBatchRender();
		m_is_rendering = false;
	}
	void GameObjectPool::UpdateXY() noexcept {
		tracy_zone_scoped_with_name("LOBJMGR.UpdateXY");
		auto const super_pause_time = GetSuperPauseTime();
		for (auto p = m_update_list.first(); p != nullptr; p = p->update_list_next) {
			if (super_pause_time > 0 && !p->ignore_super_pause) {
				continue;
			}
			p->UpdateLast();
		}
	}
	void GameObjectPool::updateNextLegacy() {
		tracy_zone_scoped_with_name("LOBJMGR.AfterFrame");
		dispatchOnBeforeBatchDestroy();
		auto const super_pause_time = GetSuperPauseTime();
		for (auto p = m_update_list.first(); p != nullptr;) {
			if (super_pause_time > 0 && !p->ignore_super_pause) {
				p = p->update_list_next;
				continue;
			}
			if (p->status != GameObjectStatus::Active) {
				p = freeWithCallbacks(p);
				continue;
			}
			p->UpdateTimer();
			p = p->update_list_next;
		}
		dispatchOnAfterBatchDestroy();
	}
	void GameObjectPool::updateNext() {
		tracy_zone_scoped_with_name("LOBJMGR.AfterFrame(New)");
		dispatchOnBeforeBatchDestroy();
		auto const super_pause_time = UpdateSuperPause(); // 更新超级暂停
		for (auto p = m_update_list.first(); p != nullptr;) {
			if (super_pause_time > 0 && !p->ignore_super_pause) {
				p = p->update_list_next;
				continue;
			}
			if (p->status != GameObjectStatus::Active) {
				p = freeWithCallbacks(p);
				continue;
			}
			p->UpdateLastV2();
			p = p->update_list_next;
		}
		dispatchOnAfterBatchDestroy();
	}
	void GameObjectPool::detectOutOfWorldBoundLegacy() {
		tracy_zone_scoped_with_name("LOBJMGR.BoundCheck");

		dispatchOnBeforeBatchOutOfWorldBoundCheck();
#ifdef USING_MULTI_GAME_WORLD
		auto const world = GetWorldFlag();
#endif // USING_MULTI_GAME_WORLD

		for (auto p = m_update_list.first(); p != nullptr; p = p->update_list_next) {
#ifdef USING_MULTI_GAME_WORLD
			if (!CheckWorlds(p->world, world)) {
				continue;
			}
#endif // USING_MULTI_GAME_WORLD
			if (_ObjectBoundCheck(p)) {
				continue;
			}
			p->status = GameObjectStatus::Dead; // 产生副作用
			// 调用 del 回调
			if (p->features.has_callback_destroy) {
#ifdef USING_MULTI_GAME_WORLD
				m_pCurrentObject = p;
#endif // USING_MULTI_GAME_WORLD
				p->dispatchOnQueueToDestroy(queue_to_destroy_reason_out_of_world_bound);
#ifdef USING_MULTI_GAME_WORLD
				m_pCurrentObject = nullptr;
#endif // USING_MULTI_GAME_WORLD
			}
		}

		dispatchOnAfterBatchOutOfWorldBoundCheck();
	}
	void GameObjectPool::detectOutOfWorldBound() {
		tracy_zone_scoped_with_name("LOBJMGR.BoundCheck(New)");

		dispatchOnBeforeBatchOutOfWorldBoundCheck();

		struct OutOfWorldBoundDetectionResult {
			uint64_t uid{};
			GameObject* game_object{};
		};

		std::pmr::deque<OutOfWorldBoundDetectionResult> cache{ &m_memory_resource };

#ifdef USING_MULTI_GAME_WORLD
		auto const world = GetWorldFlag();
#endif // USING_MULTI_GAME_WORLD

		for (auto p = m_update_list.first(); p != nullptr; p = p->update_list_next) {
#ifdef USING_MULTI_GAME_WORLD
			if (!CheckWorlds(p->world, world)) {
				continue;
			}
#endif // USING_MULTI_GAME_WORLD
			if (_ObjectBoundCheck(p)) {
				continue;
			}
			p->status = GameObjectStatus::Dead; // 产生副作用
			// 需要调用 del 回调
			if (p->features.has_callback_destroy) {
				cache.push_back(OutOfWorldBoundDetectionResult{
					.uid = p->unique_id,
					.game_object = p,
				});
			}
		}

		for (auto const& [uid, game_object] : cache) {
			if (game_object->unique_id != uid) {
				assert(false); continue; // 理论上不太可能发生
			}
			
#ifdef USING_MULTI_GAME_WORLD
			m_pCurrentObject = game_object;
#endif // USING_MULTI_GAME_WORLD
			game_object->dispatchOnQueueToDestroy(queue_to_destroy_reason_out_of_world_bound);
#ifdef USING_MULTI_GAME_WORLD
			m_pCurrentObject = nullptr;
#endif // USING_MULTI_GAME_WORLD
		}

		dispatchOnAfterBatchOutOfWorldBoundCheck();
	}
	void GameObjectPool::detectIntersectionLegacy(uint32_t const group1, uint32_t const group2) {
		tracy_zone_scoped_with_name("LOBJMGR.CollisionCheck");
		m_is_detecting_intersect = true;
		dispatchOnBeforeBatchIntersectDetect();
		auto& debug_data = m_statistics[m_statistics_index];
		for (auto ptrA = m_detect_lists[group1].first(); ptrA != nullptr;) {
			GameObject* pA = ptrA;
			ptrA = ptrA->detect_list_next;
			for (auto ptrB = m_detect_lists[group2].first(); ptrB != nullptr;) {
				GameObject* pB = ptrB;
				ptrB = ptrB->detect_list_next;
				if (!pA->features.has_callback_trigger) {
					continue;
				}
#ifdef USING_MULTI_GAME_WORLD
				if (!CheckWorlds(pA->world, pB->world)) {
					continue;
				}
#endif // USING_MULTI_GAME_WORLD
				debug_data.object_colli_check += 1;
				if (!GameObject::isIntersect(pA, pB)) {
					continue;
				}
				debug_data.object_colli_callback += 1;
#ifdef USING_MULTI_GAME_WORLD
				m_pCurrentObject = pA;
#endif // USING_MULTI_GAME_WORLD
				m_LockObjectA = pA;
				m_LockObjectB = pB;
				pA->dispatchOnTrigger(pB);
#ifdef USING_MULTI_GAME_WORLD
				m_pCurrentObject = nullptr;
#endif // USING_MULTI_GAME_WORLD
				m_LockObjectA = nullptr;
				m_LockObjectB = nullptr;
			}
		}
		dispatchOnAfterBatchIntersectDetect();
		m_is_detecting_intersect = false;
	}
	void GameObjectPool::detectIntersection(std::pmr::vector<IntersectionDetectionGroupPair> const& group_pairs) {
		tracy_zone_scoped_with_name("LOBJMGR.CollisionCheck(New)");
		m_is_detecting_intersect = true;
		dispatchOnBeforeBatchIntersectDetect();
		auto& debug_data = m_statistics[m_statistics_index];
		std::pmr::deque<IntersectionDetectionResult> cache{ &m_memory_resource };
		for (const auto& [group1, group2] : group_pairs) {
			for (auto object1 = m_detect_lists[group1].first(); object1 != nullptr; object1 = object1->detect_list_next) {
				for (auto object2 = m_detect_lists[group2].first(); object2 != nullptr; object2 = object2->detect_list_next) {
					if (!object1->features.has_callback_trigger) {
						continue;
					}
#ifdef USING_MULTI_GAME_WORLD
					if (!CheckWorlds(object1->world, object2->world)) {
						continue;
					}
#endif // USING_MULTI_GAME_WORLD
					debug_data.object_colli_check += 1;
					if (!GameObject::isIntersect(object1, object2)) {
						continue;
					}
					cache.push_back(IntersectionDetectionResult{
						.uid1 = object1->unique_id,
						.uid2 = object2->unique_id,
						.object1 = object1,
						.object2 = object2,
					});
				}
			}
		}
		for (auto const& [uid1, uid2, object1, object2] : cache) {
			if (object1->unique_id != uid1 || object2->unique_id != uid2) {
				assert(false); continue; // 理论上不太可能发生
			}
			debug_data.object_colli_callback += 1;
#ifdef USING_MULTI_GAME_WORLD
			m_pCurrentObject = object1;
#endif // USING_MULTI_GAME_WORLD
			m_LockObjectA = object1;
			m_LockObjectB = object2;
			object1->dispatchOnTrigger(object2);
#ifdef USING_MULTI_GAME_WORLD
			m_pCurrentObject = nullptr;
#endif // USING_MULTI_GAME_WORLD
			m_LockObjectA = nullptr;
			m_LockObjectB = nullptr;
		}
		dispatchOnAfterBatchIntersectDetect();
		m_is_detecting_intersect = false;
	}
	void GameObjectPool::DirtResetObject(GameObject* p) noexcept
	{
		// 分配新的 UUID 并重新插入更新链表末尾
		m_update_list.remove(p);
		m_render_list.erase(p);
		assert(p != m_LockObjectA && p != m_LockObjectB);
		m_detect_lists[p->group].remove(p);
		p->unique_id = m_iUid % GameObject::max_unique_id; // GameObject::max_unique_id is reserved
		++m_iUid;
		m_update_list.add(p);
		m_render_list.insert(p);
		m_detect_lists[p->group].add(p);
	}

	void GameObjectPool::setGroup(GameObject* const object, size_t const group) {
		assert(object != m_LockObjectA && object != m_LockObjectB);
		m_detect_lists[object->group].remove(object);
		object->group = static_cast<int32_t>(group);
		m_detect_lists[object->group].add(object);
	}
	void GameObjectPool::setLayer(GameObject* const object, double const layer) {
		assert(!m_is_rendering);
		m_render_list.erase(object);
		object->layer = layer;
		m_render_list.insert(object);
	}

	GameObject* GameObjectPool::allocateWithCallbacks(IGameObjectCallbacks* const callbacks) {
		size_t id = 0;
		if (!m_ObjectPool.alloc(id)) {
			return nullptr;
		}
		GameObject* p = m_ObjectPool.object(id);
		p->Reset();
		#ifdef USING_MULTI_GAME_WORLD
			p->world = m_iWorld;
		#endif
		p->status = GameObjectStatus::Active;
		p->id = id;
		p->unique_id = m_iUid % GameObject::max_unique_id; // GameObject::max_unique_id is reserved
		m_iUid++;
		m_update_list.add(p);
		m_render_list.insert(p);
		m_detect_lists[p->group].add(p);
		m_statistics[m_statistics_index].object_alloc += 1;
		if (callbacks != nullptr) {
			p->addCallbacks(callbacks);
		}
		dispatchOnCreate(p);
		return p;
	}
	GameObject* GameObjectPool::freeWithCallbacks(GameObject* const object) {
		dispatchOnDestroy(object);
		object->removeAllCallbacks();
		object->ReleaseResource();
		m_statistics[m_statistics_index].object_free += 1;
		auto const next = m_update_list.remove(object);
		m_render_list.erase(object);
		m_detect_lists[object->group].remove(object);
	#ifdef USING_MULTI_GAME_WORLD
		if (m_pCurrentObject == object) {
			m_pCurrentObject = nullptr;
		}
	#endif // USING_MULTI_GAME_WORLD
		object->status = GameObjectStatus::Free;
		m_ObjectPool.free(object->id);
		return next;
	}
	bool GameObjectPool::queueToFree(GameObject* const object, bool const legacy_kill_mode) {
		if (object->status != GameObjectStatus::Active) {
			return false;
		}
		object->status = legacy_kill_mode ? GameObjectStatus::Killed : GameObjectStatus::Dead;
		auto const has_callback_destroy = !legacy_kill_mode && object->features.has_callback_destroy;
		auto const has_callback_legacy_kill = legacy_kill_mode && object->features.has_callback_legacy_kill;
		return has_callback_destroy || has_callback_legacy_kill;
	}

	void GameObjectPool::DrawCollider()
	{
#if (defined LDEVVERSION)
		struct ColliderDisplayConfig
		{
			int group;
			core::Color4B color;
			ColliderDisplayConfig() { group = 0; }
			ColliderDisplayConfig(int g, core::Color4B c) { group = g; color = c; }
		};
		static std::vector<ColliderDisplayConfig> m_collidercfg = {
			ColliderDisplayConfig(1, core::Color4B(163, 73, 164, 150)), // GROUP_ENEMY_
			ColliderDisplayConfig(2, core::Color4B(163, 73, 164, 150)), // GROUP_ENEMY
			ColliderDisplayConfig(5, core::Color4B(163, 73,  20, 150)), // GROUP_INDES
			ColliderDisplayConfig(4, core::Color4B(175, 15,  20, 150)), // GROUP_PLAYER
		};
		static bool f8 = false;
		static bool kf8 = false;

		if (!kf8 && LAPP.GetKeyState(/* VK_F8 */ 0x77)) { kf8 = true; f8 = !f8; }
		else if (kf8 && !LAPP.GetKeyState(/* VK_F8 */ 0x77)) { kf8 = false; }

		if (f8)
		{
			LAPP.DebugSetGeometryRenderState();
			for (ColliderDisplayConfig cfg : m_collidercfg)
			{
				DrawGroupCollider(cfg.group, cfg.color);
			}
		}
#endif
	}
	void GameObjectPool::DrawGroupCollider(int groupId, core::Color4B fillColor)
	{
#ifdef USING_MULTI_GAME_WORLD
		auto const world = GetWorldFlag();
#endif // USING_MULTI_GAME_WORLD
		for (auto p = m_detect_lists[groupId].first(); p != nullptr; p = p->update_list_next) {
#ifdef USING_MULTI_GAME_WORLD
			if (p->colli && CheckWorlds(p->world, world))
#else // !USING_MULTI_GAME_WORLD
			if (p->colli)
#endif // USING_MULTI_GAME_WORLD
			{
				if (p->rect)
				{
					LAPP.DebugDrawRect((float)p->x, (float)p->y, (float)p->a, (float)p->b, (float)p->rot, fillColor);
				}
				else if (!p->rect && p->a == p->b)
				{
					LAPP.DebugDrawCircle((float)p->x, (float)p->y, (float)p->a, fillColor);
				}
				else if (!p->rect && p->a != p->b)
				{
					LAPP.DebugDrawEllipse((float)p->x, (float)p->y, (float)p->a, (float)p->b, (float)p->rot, fillColor);
				}
			}
		}
	}
	void GameObjectPool::DrawGroupCollider2(int groupId, core::Color4B fillColor)
	{
		LAPP.DebugSetGeometryRenderState();
		DrawGroupCollider(groupId, fillColor);
	}

	bool CreateGameObjectPool(std::string_view name)
	{
		if (name.empty())
			return false;
		
		std::string key(name);
		std::lock_guard<std::mutex> lk(g_gameobject_pools_mutex);
		if (g_gameobject_pools.find(key) != g_gameobject_pools.end())
			return false;
		
		g_gameobject_pools.emplace(key, std::make_unique<GameObjectPool>());
		spdlog::info("[luastg] GameObjectPool '{}' created", name);
		return true;
	}

	bool RemoveGameObjectPool(std::string_view name)
	{
		if (name.empty())
			return false;
		
		std::string key(name);
		std::lock_guard<std::mutex> lk(g_gameobject_pools_mutex);
		
		auto it = g_gameobject_pools.find(key);
		if (it == g_gameobject_pools.end())
			return false;
		
		GameObjectPool* p = it->second.get();
		if (g_active_gameobject_pool == p)
		{
			g_active_gameobject_pool = nullptr;
		}
		g_gameobject_pools.erase(it);
		spdlog::info("[luastg] GameObjectPool '{}' deleted", name);
		return true;
	}

	GameObjectPool* GetGameObjectPool(std::string_view name) noexcept
	{
		if (name.empty())
			return nullptr;
		
		std::string key(name);
		std::lock_guard<std::mutex> lk(g_gameobject_pools_mutex);

		auto it = g_gameobject_pools.find(key);
		return it != g_gameobject_pools.end() ? it->second.get() : nullptr;
	}

	bool SetActiveGameObjectPoolByName(std::string_view name) noexcept
	{
		if (name.empty())
		{
			std::lock_guard<std::mutex> lk(g_gameobject_pools_mutex);
			g_active_gameobject_pool = nullptr;
			return true;
		}

		std::string key(name);
		std::lock_guard<std::mutex> lk(g_gameobject_pools_mutex);

		auto it = g_gameobject_pools.find(key);
		if (it == g_gameobject_pools.end())
			return false;
		
		g_active_gameobject_pool = it->second.get();
		return true;
	}

	GameObjectPool* GetActiveGameObjectPool() noexcept
	{
		std::lock_guard<std::mutex> lk(g_gameobject_pools_mutex);
		return g_active_gameobject_pool;
	}

	std::string GetActiveGameObjectPoolName() noexcept
	{
		std::lock_guard<std::mutex> lk(g_gameobject_pools_mutex);
		for (auto const& kv : g_gameobject_pools)
		{
			if (kv.second.get() == g_active_gameobject_pool)
				return kv.first;
		}
		return std::string{};
	}

	std::vector<std::string> EnumGameObjectPools() noexcept
	{
		std::lock_guard<std::mutex> lk(g_gameobject_pools_mutex);
		std::vector<std::string> ret;
		ret.reserve(g_gameobject_pools.size());
		for (auto const& kv : g_gameobject_pools)
			ret.push_back(kv.first);
		
		return ret;
	}

	void ClearAllGameObjectPools()
	{
		std::lock_guard<std::mutex> lk(g_gameobject_pools_mutex);
		g_active_gameobject_pool = nullptr;
	}
}
