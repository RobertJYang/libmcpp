/*
 * Copyright (c) 2026 MCLang Project
 * Licensed under Mulan PSL v2
 */

/**
 * @file gc_head.h
 * @brief GC 对象标记，嵌入到 shared_counter 中
 *
 * GCHead 仅包含 gc_index 字段（4 字节），
 * 用于在 GCTracker 中央追踪数组中索引对象的 GC 元数据。
 * 所有 GC 状态（gc_refs, gc_bits, type_tag）
 * 存储在 GCTracker::Entry 而非对象头中。
 */

#ifndef MC_GC_GC_HEAD_H
#define MC_GC_GC_HEAD_H

#include <cstdint>
#include <atomic>
#include <mc/common.h>

namespace mc::gc {

struct GCHead;

namespace detail {

template <typename T, typename = void>
struct gc_head_object_cast {
    static T* cast(GCHead* head) {
        return static_cast<T*>(head);
    }
};

} // namespace detail

/// GC 对象标记，嵌入到 shared_counter 中（4 字节）
/// gc_index 使用 atomic：untrack_locked 的 swap-with-last 会写其他对象的 gc_index，
/// 而那个对象可能正被另一个线程析构/访问。
struct GCHead {
    std::atomic<uint32_t> gc_index;
    std::atomic<uint8_t>  gc_flags;

    GCHead() noexcept : gc_index{GC_INDEX_UNTRACKED}, gc_flags{0} {}

    static constexpr uint32_t GC_INDEX_UNTRACKED = UINT32_MAX;
    static constexpr uint8_t GC_FLAG_PENDING_FINAL_RELEASE = 0x01;
    static constexpr uint8_t GC_FLAG_EXTERNAL_OWNED        = 0x02;

    bool is_tracked() const {
        return gc_index.load(std::memory_order_relaxed) != GC_INDEX_UNTRACKED;
    }
    uint32_t get_gc_index() const {
        return gc_index.load(std::memory_order_relaxed);
    }
    void set_gc_index(uint32_t idx) {
        gc_index.store(idx, std::memory_order_relaxed);
    }

    bool is_pending_final_release() const {
        return (gc_flags.load(std::memory_order_relaxed) & GC_FLAG_PENDING_FINAL_RELEASE) != 0;
    }

    void set_pending_final_release(bool pending) {
        uint8_t flags = gc_flags.load(std::memory_order_relaxed);
        if (pending) {
            flags |= GC_FLAG_PENDING_FINAL_RELEASE;
        } else {
            flags &= static_cast<uint8_t>(~GC_FLAG_PENDING_FINAL_RELEASE);
        }
        gc_flags.store(flags, std::memory_order_relaxed);
    }

    bool is_external_owned() const {
        return (gc_flags.load(std::memory_order_relaxed) & GC_FLAG_EXTERNAL_OWNED) != 0;
    }

    void set_external_owned(bool external_owned) {
        uint8_t flags = gc_flags.load(std::memory_order_relaxed);
        if (external_owned) {
            flags |= GC_FLAG_EXTERNAL_OWNED;
        } else {
            flags &= static_cast<uint8_t>(~GC_FLAG_EXTERNAL_OWNED);
        }
        gc_flags.store(flags, std::memory_order_relaxed);
    }

    void gc_init() {
        gc_index.store(GC_INDEX_UNTRACKED, std::memory_order_relaxed);
        gc_flags.store(0, std::memory_order_relaxed);
    }

    template<typename T>
    static T* get_object(GCHead* head) {
        return detail::gc_head_object_cast<T>::cast(head);
    }
};

/// 自动 untrack 回调类型（由 mclruntime 设置，mcbase 不依赖 mclruntime）
using gc_untrack_hook_t = void(*)(GCHead*);

/// 全局 untrack 回调指针（定义在 common.cpp，跨共享库唯一）
MC_API gc_untrack_hook_t& gc_untrack_hook();

/// 自动 track 回调：(GCHead*, type_tag)
using gc_track_hook_t = void(*)(GCHead*, uint8_t);

MC_API gc_track_hook_t& gc_track_hook();

using gc_final_release_context_t = void*;
using gc_execute_final_release_t = void(*)(gc_final_release_context_t);
using gc_final_release_hook_t =
    bool(*)(GCHead*, gc_final_release_context_t, gc_execute_final_release_t);

MC_API gc_final_release_hook_t& gc_final_release_hook();
MC_API void gc_pre_destroy_untrack(GCHead* head);
MC_API bool gc_try_dispatch_final_release(GCHead* head,
                                          gc_final_release_context_t ctx,
                                          gc_execute_final_release_t exec);

} // namespace mc::gc

#endif // MC_GC_GC_HEAD_H
