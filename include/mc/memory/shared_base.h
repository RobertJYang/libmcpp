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
#include <mc/atomic_ref.h>
#include <mc/memory/allocator.h>

/**
 * @file shared_counter.h
 * @brief MC 智能指针基础设施
 *
 * 本文件定义了 MC 库的智能指针基础类 shared_base，配合 shared_ptr 和 weak_ptr
 * 提供完整的引用计数内存管理解决方案。
 *
 * ## 为什么重新实现智能指针？
 *
 * 虽然标准库的 std::shared_ptr 功能完善，但在以下场景下我们需要自定义实现：
 *
 * ### 1. 内存布局紧凑性
 * - std::shared_ptr 使用独立的控制块，导致对象和控制块分离
 * - 我们的实现将引用计数直接嵌入对象中，避免额外内存分配
 * - 减少内存碎片，提高缓存局部性，特别适合嵌入式和高性能场景
 *
 * ### 2. 共享内存支持
 * - 标准库智能指针无法直接用于进程间共享内存
 * - 通过模板参数 PointerType，支持 offset_ptr 等共享内存指针类型
 * - 为分布式系统和进程间通信提供基础设施
 *
 * ### 3. 定制化内存管理
 * - 支持自定义分配器，适应特殊内存需求
 * - 与MC库的其他组件（如 variant、dict）无缝集成
 * - 提供统一的内存管理语义
 *
 * enable_shared_from_this 实现的功能与 std::enable_shared_from_this 类似，但做了一些修改：
 * 1. 初始化引用计数为 INVALID 表示对象未被管理。
 * 2. 初始化过程中允许创建 weak_ptr，并确保 weak_ptr 不会误销毁未管理的对象。
 * 3. 延迟 shared_ptr 构造，可以从未管理对象的指针直接构造 shared_ptr。
 *
 * 这些机制对 object 体系的实现至关重要，根据我们 object 体系设计，父对象使用 mc::shared_ptr 管理子对象生命周期，
 * 并且我们支持子对象在构造过程中就将自己加入到父对象的管理列表中，但这个时候外部没有机会构造子对象的 shared_ptr 指针，
 * 因为子对象的构造函数还未返回。
 *
 * 举例：
 * ```cpp
 * class ChildObject : public mc::object {
 *     ChildObject(mc::object *parent): mc::object(parent) {
 *         // 如果指定了 parent，在这里 this 就已经被加入到 parent 的管理列表中
 *     }
 * };
 * ```
 *
 * ## 使用方式
 *
 * ```cpp
 * // 定义业务类
 * class MyClass : public mc::enable_shared_from_this<MyClass> {
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

template <typename T, typename Deleter = mc::default_deleter<T>, typename PointerType = T*>
class shared_ptr;

template <typename T, typename Deleter = mc::default_deleter<T>, typename PointerType = T*>
class weak_ptr;

namespace detail {
[[noreturn]] void throw_invalid_op_exception(const char* msg);
} // namespace detail

/**
 * 共享计数器，提供引用计数管理能力
 * 支持强引用和弱引用，适用于共享内存场景
 * 作为智能指针的控制块，管理对象的生命周期
 */
template <typename CounterType = uint32_t>
class shared_counter {
public:
    using counter_type       = CounterType;
    using atomic_counter_ref = mc::atomic_ref<counter_type>;

    static constexpr counter_type INVALID   = std::numeric_limits<counter_type>::max();
    static constexpr counter_type DESTROYED = 0;

    shared_counter() : m_ref_count(INVALID), m_weak_count(0) {
    }

    virtual ~shared_counter() {
    }

    shared_counter(const shared_counter&) : m_ref_count(INVALID), m_weak_count(0) {
    }

    shared_counter& operator=(const shared_counter&) {
        // 赋值操作不改变引用计数和管理状态
        return *this;
    }

    shared_counter(shared_counter&&) noexcept : m_ref_count(INVALID), m_weak_count(0) {
    }

    shared_counter& operator=(shared_counter&&) noexcept {
        // 赋值操作不改变引用计数和管理状态
        return *this;
    }

    // 增加强引用计数
    void add_ref() const {
        atomic_counter_ref ref(m_ref_count);
        counter_type       current = ref.load(std::memory_order_acquire);

        while (current != DESTROYED) {
            // 如果引用计数为初始值，由第一个 shared_ptr 将其引用计数设置为 1，否则增加引用计数
            counter_type next = current == INVALID ? 1 : current + 1;
            if (ref.compare_exchange_weak(
                    current, next, std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return;
            }
        }

        detail::throw_invalid_op_exception("attempt to add reference to a destroyed object");
    }

    // 减少强引用计数，如果引用计数为 0 则返回 true
    bool release_ref() const {
        atomic_counter_ref ref(m_ref_count);
        counter_type       current = ref.load(std::memory_order_acquire);

        while (current != DESTROYED && current != INVALID) {
            if (ref.compare_exchange_weak(
                    current, current - 1, std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return current == 1; // 前一个引用计数为 1，对象可以销毁了
            }
        }

        detail::throw_invalid_op_exception(
            "attempt to release reference to a destroyed or not managed object");
    }

    // 增加弱引用计数
    void add_weak_ref() const {
        atomic_counter_ref(m_weak_count).fetch_add(1, std::memory_order_relaxed);
    }

    // 减少弱引用计数，如果弱引用计数为0且对象已销毁则返回true
    bool release_weak_ref() const {
        if (atomic_counter_ref(m_weak_count).fetch_sub(1, std::memory_order_acq_rel) == 1) {
            // 弱引用计数为0且对象已销毁时，可以释放内存
            return is_destroyed();
        }

        return false;
    }

    // 获取当前强引用计数
    counter_type ref_count() const {
        return atomic_counter_ref(m_ref_count).load(std::memory_order_relaxed);
    }

    // 获取当前弱引用计数
    counter_type weak_count() const {
        return atomic_counter_ref(m_weak_count).load(std::memory_order_relaxed);
    }

    // 尝试从弱引用升级为强引用
    bool try_add_ref() const {
        atomic_counter_ref ref(m_ref_count);
        counter_type       current = ref.load(std::memory_order_acquire);

        while (current != DESTROYED && current != INVALID) {
            if (ref.compare_exchange_weak(
                    current, current + 1, std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }
        }

        return false;
    }

    /**
     * @brief 检查对象是否已被智能指针管理
     */
    bool is_managed() const {
        counter_type current = atomic_counter_ref(m_ref_count).load(std::memory_order_acquire);
        return current != INVALID;
    }

    /**
     * @brief 检查对象是否已析构
     */
    bool is_destroyed() const {
        counter_type current = atomic_counter_ref(m_ref_count).load(std::memory_order_acquire);
        return current == DESTROYED;
    }

private:
    mutable counter_type m_ref_count;  // 强引用计数
    mutable counter_type m_weak_count; // 弱引用计数
};

/*
 * 共享计数器基类，提供引用计数管理能力
 * 作为智能指针的控制块，管理对象的生命周期
 */
using shared_base = shared_counter<>;

/*
 * 与 std::enable_shared_from_this 类似，提供 shared_from_this 方法
 */
template <typename T,
          typename Deleter     = mc::default_deleter<T>,
          typename PointerType = T*,
          typename CounterType = uint32_t>
class enable_shared_from_this : public shared_counter<CounterType> {
public:
    using element_type = T;
    using pointer_type = PointerType;

    using shared_ptr_type = shared_ptr<element_type, Deleter, PointerType>;
    using weak_ptr_type   = weak_ptr<element_type, Deleter, PointerType>;

    /**
     * @brief 安全地获取 shared_ptr，类似 std::enable_shared_from_this
     *
     * 由于对象创建时引用计数就是1，现在总是安全的
     */
    shared_ptr_type shared_from_this() const {
        return shared_ptr_type(static_cast<const element_type*>(this));
    }

    /**
     * @brief 安全地获取 shared_ptr，类似 std::enable_shared_from_this
     */
    shared_ptr_type shared_from_this() {
        return shared_ptr_type(static_cast<element_type*>(this));
    }

    weak_ptr_type weak_from_this() const {
        return weak_ptr_type(static_cast<const element_type*>(this));
    }

    weak_ptr_type weak_from_this() {
        return weak_ptr_type(static_cast<element_type*>(this));
    }

    static shared_ptr_type from_raw(void* ptr, bool add_ref = false) {
        if (!ptr) {
            return {};
        }

        auto* p_element = static_cast<element_type*>(ptr);
        if (add_ref) {
            return shared_ptr_type{p_element};
        } else {
            // 不需要增加引用计数，使用内部构造函数
            return shared_ptr_type{p_element, typename shared_ptr_type::already_referenced_tag{}};
        }
    }
};

} // namespace mc::memory

#endif // MC_REF_BASE_H