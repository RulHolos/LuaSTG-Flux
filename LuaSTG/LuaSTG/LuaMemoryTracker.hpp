#pragma once

#include <tracy/Tracy.hpp>
#include <lua.hpp>
#include <cstdlib>
#include <new>
#include <unordered_set>

#define LSTG_TRACY_CONNECTED (tracy::ProfilerAvailable())

#ifdef LSTG_TRACY_MEMORY_IMPL

void* operator new(std::size_t count) {
    auto ptr = malloc(count);
    if (!ptr) throw std::bad_alloc();
    if (LSTG_TRACY_CONNECTED) TracyAlloc(ptr, count);
    return ptr;
}

void* operator new(std::size_t count, const std::nothrow_t&) noexcept {
    auto ptr = malloc(count);
    if (ptr && LSTG_TRACY_CONNECTED) TracyAlloc(ptr, count);
    return ptr;
}

void* operator new[](std::size_t count) {
    auto ptr = malloc(count);
    if (!ptr) throw std::bad_alloc();
    if (LSTG_TRACY_CONNECTED) TracyAlloc(ptr, count);
    return ptr;
}

void* operator new[](std::size_t count, const std::nothrow_t&) noexcept {
    auto ptr = malloc(count);
    if (ptr && LSTG_TRACY_CONNECTED) TracyAlloc(ptr, count);
    return ptr;
}

void operator delete(void* ptr) noexcept {
    if (LSTG_TRACY_CONNECTED) TracyFree(ptr);
    free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    if (LSTG_TRACY_CONNECTED) TracyFree(ptr);
    free(ptr);
}

void operator delete[](void* ptr) noexcept {
    if (LSTG_TRACY_CONNECTED) TracyFree(ptr);
    free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    if (LSTG_TRACY_CONNECTED) TracyFree(ptr);
    free(ptr);
}

#endif

namespace LuaMemoryTracker {

    static constexpr const char* POOL_NAME = "Lua heap";

    static void* lua_tracy_allocator(void* /*ud*/, void* optr, std::size_t osize, std::size_t nsize)
    {
        bool const connected = LSTG_TRACY_CONNECTED;

        if (nsize == 0) {
            if (optr) {
                if (connected) TracyFreeN(optr, POOL_NAME);
                free(optr);
            }
            return nullptr;
        }

        if (optr == nullptr) {
            void* ptr = malloc(nsize);
            if (ptr && connected) TracyAllocN(ptr, nsize, POOL_NAME);
            return ptr;
        }

        if (connected) TracyFreeN(optr, POOL_NAME);
        void* ptr = realloc(optr, nsize);
        if (ptr) {
            if (connected) TracyAllocN(ptr, nsize, POOL_NAME);
        } else {
            if (connected) TracyAllocN(optr, osize, POOL_NAME);
        }
        return ptr;
    }

    inline void install(lua_State* L) {
        lua_setallocf(L, lua_tracy_allocator, nullptr);
    }
}