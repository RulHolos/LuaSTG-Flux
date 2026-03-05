#pragma once

#include <tracy/Tracy.hpp>
#include <lua.hpp>
#include <cstdlib>
#include <new>
#include <unordered_set>

#ifdef LSTG_TRACY_MEMORY_IMPL

void* operator new(std::size_t count) {
    auto ptr = malloc(count);
    if (!ptr) throw std::bad_alloc();
    TracyAlloc(ptr, count);
    return ptr;
}

void* operator new(std::size_t count, const std::nothrow_t&) noexcept {
    auto ptr = malloc(count);
    if (ptr) TracyAlloc(ptr, count);
    return ptr;
}

void* operator new[](std::size_t count) {
    auto ptr = malloc(count);
    if (!ptr) throw std::bad_alloc();
    TracyAlloc(ptr, count);
    return ptr;
}

void* operator new[](std::size_t count, const std::nothrow_t&) noexcept {
    auto ptr = malloc(count);
    if (ptr) TracyAlloc(ptr, count);
    return ptr;
}

void operator delete(void* ptr) noexcept {
    TracyFree(ptr);
    free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    TracyFree(ptr);
    free(ptr);
}

void operator delete[](void* ptr) noexcept {
    TracyFree(ptr);
    free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    TracyFree(ptr);
    free(ptr);
}

#endif

namespace LuaMemoryTracker {

    static constexpr const char* POOL_NAME = "Lua heap";

    struct AllocatorContext {
        lua_Alloc original_alloc;
        void* original_ud;
        std::unordered_set<void*> tracked;
    };

    static AllocatorContext alloc_ctx{};

    static void* lua_tracy_allocator(void* ud, void* optr, std::size_t osize, std::size_t nsize) {
        auto* ctx = static_cast<AllocatorContext*>(ud);

        if (nsize == 0) // free
        {
            if (optr && ctx->tracked.erase(optr)) {
                TracyFreeN(optr, POOL_NAME);
            }
            return ctx->original_alloc(ctx->original_ud, optr, osize, nsize);
        }

        if (optr == nullptr) // alloc
        {
            void* ptr = ctx->original_alloc(ctx->original_ud, optr, osize, nsize);
            if (ptr) {
                TracyAllocN(ptr, nsize, POOL_NAME);
                ctx->tracked.insert(ptr);
            }
            return ptr;
        }

        // realloc
        bool const was_tracked = ctx->tracked.erase(optr);
        if (was_tracked) TracyFreeN(optr, POOL_NAME);
        void* ptr = ctx->original_alloc(ctx->original_ud, optr, osize, nsize);
        if (ptr) {
            TracyAllocN(ptr, nsize, POOL_NAME);
            ctx->tracked.insert(ptr);
        } else if (was_tracked) {
            TracyAllocN(optr, osize, POOL_NAME);
            ctx->tracked.insert(optr);
        }
        return ptr;
    }

    inline void install(lua_State* L) {
        alloc_ctx.tracked.reserve(4096);
        alloc_ctx.original_alloc = lua_getallocf(L, &alloc_ctx.original_ud);
        lua_setallocf(L, lua_tracy_allocator, &alloc_ctx);
    }
}