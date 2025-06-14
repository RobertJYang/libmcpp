/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_REF_BASE_H
#define MC_REF_BASE_H

#include <atomic>
#include <cstddef>
#include <mc/atomic_ref.h>

namespace mc {

template <typename T, typename PointerType = T*>
class ref_ptr;

template <typename T, typename PointerType = T*>
class weak_ptr;

/**
 * 引用计数基类，提供引用计数管理能力
 * 支持强引用和弱引用，适用于共享内存场景
 */
template <typename T, typename PointerType = T*>
class ref_base {
public:
    using element_type = T;
    using pointer_type = PointerType;
    using ref_ptr      = mc::ref_ptr<element_type, pointer_type>;
    using weak_ptr     = mc::weak_ptr<element_type, pointer_type>;

    ref_base() = default;

    virtual ~ref_base() {
    }

    // ref_base 的生命周期由 ref_ptr 管理，拷贝构造函数也是初始引用计数为0
    ref_base(const ref_base&) {
        // 新对象的引用计数为0
        m_ref_count  = 0;
        m_weak_count = 0;
    }

    ref_base& operator=(const ref_base&) {
        // 赋值操作不改变引用计数
        return *this;
    }

    ref_base(ref_base&&) noexcept {
        // 新对象的引用计数为0
        m_ref_count  = 0;
        m_weak_count = 0;
    }

    ref_base& operator=(ref_base&&) noexcept {
        // 赋值操作不改变引用计数
        return *this;
    }

    // 增加强引用计数
    void add_ref() const {
        atomic_size_ref(m_ref_count).fetch_add(1, std::memory_order_relaxed);
    }

    // 减少强引用计数，如果引用计数为0则返回true
    bool release_ref() const {
        return atomic_size_ref(m_ref_count).fetch_sub(1, std::memory_order_acq_rel) == 1;
    }

    // 增加弱引用计数
    void add_weak_ref() const {
        atomic_size_ref(m_weak_count).fetch_add(1, std::memory_order_relaxed);
    }

    // 减少弱引用计数，如果弱引用计数为0且对象已销毁则返回true
    bool release_weak_ref() const {
        if (atomic_size_ref(m_weak_count).fetch_sub(1, std::memory_order_acq_rel) == 1) {
            // 弱引用计数为0且对象已销毁时，可以释放内存
            return atomic_size_ref(m_ref_count).load(std::memory_order_acquire) == 0;
        }
        return false;
    }

    // 获取当前强引用计数
    size_t ref_count() const {
        return atomic_size_ref(m_ref_count).load(std::memory_order_relaxed);
    }

    // 获取当前弱引用计数
    size_t weak_count() const {
        return atomic_size_ref(m_weak_count).load(std::memory_order_relaxed);
    }

    // 检查对象是否已销毁
    bool is_destroyed() const {
        return atomic_size_ref(m_ref_count).load(std::memory_order_acquire) == 0;
    }

    // 尝试从弱引用升级为强引用
    bool try_add_ref() const {
        atomic_size_ref ref_atomic(m_ref_count);
        size_t          current = ref_atomic.load(std::memory_order_acquire);
        do {
            if (current == 0) {
                return false; // 对象已销毁，无法升级
            }
        } while (!ref_atomic.compare_exchange_weak(current, current + 1,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire));
        return true;
    }

    ref_ptr shared_from_this() const {
        return ref_ptr(static_cast<const element_type*>(this), false);
    }

    ref_ptr shared_from_this(bool add_ref = true) {
        return ref_ptr(static_cast<element_type*>(this), add_ref);
    }

    weak_ptr weak_from_this() const {
        return weak_ptr(static_cast<const element_type*>(this));
    }

    weak_ptr weak_from_this() {
        return weak_ptr(static_cast<element_type*>(this));
    }

    static ref_ptr from_raw(void* ptr, bool add_ref = false) {
        return ref_ptr(static_cast<element_type*>(ptr), add_ref);
    }

private:
    mutable size_t m_ref_count{0};  // 强引用计数（非原子，通过atomic_ref操作）
    mutable size_t m_weak_count{0}; // 弱引用计数（非原子，通过atomic_ref操作）
};

} // namespace mc

#endif // MC_REF_BASE_H