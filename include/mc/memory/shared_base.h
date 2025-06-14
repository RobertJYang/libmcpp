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

/**
 * @file shared_base.h
 * @brief MC智能指针系统基础设施
 *
 * 本文件定义了MC库的智能指针基础类shared_base，配合shared_ptr和weak_ptr
 * 提供完整的引用计数内存管理解决方案。
 *
 * ## 为什么重新实现智能指针？
 *
 * 虽然标准库的std::shared_ptr功能完善，但在以下场景下我们需要自定义实现：
 *
 * ### 1. 内存布局紧凑性
 * - std::shared_ptr使用独立的控制块，导致对象和控制块分离
 * - 我们的实现将引用计数直接嵌入对象中，避免额外内存分配
 * - 减少内存碎片，提高缓存局部性，特别适合嵌入式和高性能场景
 *
 * ### 2. 共享内存支持
 * - 标准库智能指针无法直接用于进程间共享内存
 * - 通过模板参数PointerType，支持offset_ptr等共享内存指针类型
 * - 为分布式系统和进程间通信提供基础设施
 *
 * ### 3. 定制化内存管理
 * - 支持自定义分配器，适应特殊内存需求
 * - 与MC库的其他组件（如variant、dict）无缝集成
 * - 提供统一的内存管理语义
 *
 * ## 使用方式
 *
 * ```cpp
 * // 定义业务类
 * class MyClass : public mc::shared_base<MyClass> {
 *     // 业务逻辑...
 * };
 *
 * // 创建智能指针
 * auto ptr = mc::make_shared<MyClass>();
 * auto weak = ptr->weak_from_this();
 *
 * // 共享内存场景（offset_ptr为示例）
 * using shared_memory_ptr = mc::shared_ptr<MyClass, offset_ptr<MyClass>>;
 * ```
 *
 * @see shared_ptr, weak_ptr
 */

namespace mc::memory {

template <typename T, typename PointerType = T*>
class shared_ptr;

template <typename T, typename PointerType = T*>
class weak_ptr;

/**
 * 引用计数基类，提供引用计数管理能力
 * 支持强引用和弱引用，适用于共享内存场景
 */
template <typename T, typename PointerType = T*>
class shared_base {
public:
    using element_type = T;
    using pointer_type = PointerType;
    using shared_ptr   = mc::memory::shared_ptr<element_type, pointer_type>;
    using weak_ptr     = mc::memory::weak_ptr<element_type, pointer_type>;

    shared_base() = default;

    virtual ~shared_base() {
    }

    // shared_base 的生命周期由 shared_ptr 管理，拷贝构造函数也是初始引用计数为0
    shared_base(const shared_base&) {
        m_ref_count  = 0;
        m_weak_count = 0;
    }

    shared_base& operator=(const shared_base&) {
        // 赋值操作不改变引用计数
        return *this;
    }

    shared_base(shared_base&&) noexcept {
        // 新对象的引用计数为0
        m_ref_count  = 0;
        m_weak_count = 0;
    }

    shared_base& operator=(shared_base&&) noexcept {
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

    shared_ptr shared_from_this() const {
        return shared_ptr(static_cast<const element_type*>(this), false);
    }

    shared_ptr shared_from_this(bool add_ref = true) {
        return shared_ptr(static_cast<element_type*>(this), add_ref);
    }

    weak_ptr weak_from_this() const {
        return weak_ptr(static_cast<const element_type*>(this));
    }

    weak_ptr weak_from_this() {
        return weak_ptr(static_cast<element_type*>(this));
    }

    static shared_ptr from_raw(void* ptr, bool add_ref = false) {
        return shared_ptr(static_cast<element_type*>(ptr), add_ref);
    }

private:
    mutable size_t m_ref_count{0};  // 强引用计数
    mutable size_t m_weak_count{0}; // 弱引用计数
};

} // namespace mc::memory

#endif // MC_REF_BASE_H