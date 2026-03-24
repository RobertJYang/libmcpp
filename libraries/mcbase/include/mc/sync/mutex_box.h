/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_SYNC_MUTEX_BOX_H
#define MC_SYNC_MUTEX_BOX_H

#include <mc/exception.h>
#include <mc/sync/shared_mutex.h>
#include <mc/time.h>
#include <mc/traits.h>

#include <cstddef>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <type_traits>

/**
 * @file mutex_box.h
 * @brief 线程安全的数据容器 mutex_box
 *
 * mutex_box 是一个线程安全的数据容器，它将数据和保护该数据的互斥量封装在一起，
 * 提供 RAII 风格的线程安全访问机制。
 *
 * ## 主要特性
 *
 * 1. **RAII 安全**: 自动管理锁的生命周期，防止忘记解锁
 * 2. **类型安全**: 编译时确保正确的锁类型匹配
 * 3. **多种锁支持**: 支持独占锁(std::mutex)、读写锁(std::shared_mutex)、超时锁等
 * 4. **死锁避免**: 提供多对象锁定功能，自动避免死锁
 * 5. **高性能**: 最小化锁定开销，支持读共享访问
 *
 * ## 基本用法
 *
 * ```cpp
 * // 创建线程安全的数据容器
 * mutex_box<int> counter(0);
 * mutex_box<mc::string> message("hello");
 *
 * // 独占访问 (写锁)
 * {
 *     auto locked = counter.wlock();
 *     *locked += 1;  // 安全地修改数据
 * } // 自动解锁
 *
 * // 共享访问 (读锁) - 适用于 std::shared_mutex
 * {
 *     auto locked = counter.rlock();
 *     int value = *locked;  // 安全地读取数据，支持并发读取
 * }
 *
 * // 通用锁定 (根据互斥量类型自动选择)
 * {
 *     auto locked = counter.lock();
 *     *locked = 100;
 * }
 *
 * // 超时锁定 - 避免无限等待
 * {
 *     auto locked = counter.try_rlock_for(mc::milliseconds(100));
 *     if (locked) {
 *         int value = *locked;  // 在100ms内成功获取读锁
 *     } else {
 *         // 超时，未能获取锁
 *     }
 * }
 * ```
 *
 * ## 多对象锁定
 *
 * ```cpp
 * mutex_box<int> account1(1000);
 * mutex_box<int> account2(500);
 *
 * // 安全地同时锁定多个对象，自动避免死锁
 * auto [locked1, locked2] = lock_pair(account1, account2);
 * *locked1 -= 100;  // 转账操作
 * *locked2 += 100;
 *
 * // 支持任意数量的对象同时锁定
 * auto [a, b, c] = lock(box1, box2, box3);
 * ```
 *
 * ## 支持的互斥量类型
 *
 * - `std::mutex`: 独占锁，只支持 wlock() 和 lock()
 * - `std::shared_mutex`: 读写锁，支持 rlock()、wlock() 和 lock()
 * - `std::timed_mutex`: 超时独占锁，支持超时操作
 * - `std::shared_timed_mutex`: 超时读写锁，支持所有操作
 * - 或任何符合标准互斥量概念的自定义类型，如 mc::sync::shared_mutex
 *
 * ## 线程安全保证
 *
 * - 多个线程可以安全地并发访问同一个 mutex_box
 * - 读操作(rlock)之间可以并发执行（仅限支持共享锁的互斥量）
 * - 写操作(wlock)与任何其他操作互斥
 * - 所有访问都通过 RAII 锁定指针进行，确保数据访问期间锁不会被意外释放
 *
 * @see lock_pair 用于安全地同时锁定两个对象
 * @see lock 用于安全地同时锁定多个对象
 */

namespace mc::sync {

namespace detail {

// 判断互斥锁是否支持独占锁
template <typename Mutex, typename = void>
inline constexpr bool has_lock = false;
template <typename Mutex>
inline constexpr bool has_lock<Mutex, decltype(void(std::declval<Mutex&>().lock()))> = true;

// 判断互斥锁是否支持共享锁
template <typename Mutex, typename = void>
inline constexpr bool has_shared = false;
template <typename Mutex>
inline constexpr bool has_shared<Mutex, decltype(void(std::declval<Mutex&>().lock_shared()))> = true;

// 判断互斥锁是否支持超时独占锁
template <typename Mutex, typename = void>
inline constexpr bool has_try_lock_for = false;
template <typename Mutex>
inline constexpr bool
    has_try_lock_for<Mutex, decltype(void(std::declval<Mutex&>().try_lock_for(std::chrono::milliseconds(1))))> = true;

// 判断互斥锁是否支持超时共享锁
template <typename Mutex, typename = void>
inline constexpr bool has_try_lock_shared_for = false;
template <typename Mutex>
inline constexpr bool has_try_lock_shared_for<Mutex, decltype(void(std::declval<Mutex&>().try_lock_shared_for(
                                                         std::chrono::milliseconds(1))))> = true;

// 判断互斥锁是否支持升级锁
template <typename Mutex, typename = void>
inline constexpr bool has_upgrade = false;
template <typename Mutex>
inline constexpr bool has_upgrade<Mutex, decltype(void(std::declval<Mutex&>().lock_upgrade()))> = true;

// 判断互斥锁是否支持超时升级锁
template <typename Mutex, typename = void>
inline constexpr bool has_try_lock_upgrade_for = false;
template <typename Mutex>
inline constexpr bool has_try_lock_upgrade_for<Mutex, decltype(void(std::declval<Mutex&>().try_lock_upgrade_for(
                                                          std::chrono::milliseconds(1))))> = true;

/**
 * @brief 锁类型枚举
 */
enum class lock_type {
    unique,     // 独占锁
    shared,     // 共享锁
    upgrade,    // 升级锁
    try_unique, // 尝试独占锁
    try_shared, // 尝试共享锁
    try_upgrade // 尝试升级锁
};

/**
 * @brief 根据锁类型获取对应的 std::lock 类型
 */
template <lock_type LockType, typename Mutex>
struct lock_select {};

template <typename Mutex>
struct lock_select<lock_type::unique, Mutex> {
    using type = std::unique_lock<Mutex>;
};

template <typename Mutex>
struct lock_select<lock_type::shared, Mutex> {
    using type = std::shared_lock<Mutex>;
};

template <typename Mutex>
struct lock_select<lock_type::try_unique, Mutex> {
    using type = std::unique_lock<Mutex>;
};

template <typename Mutex>
struct lock_select<lock_type::try_shared, Mutex> {
    using type = std::shared_lock<Mutex>;
};

template <typename Mutex>
struct lock_select<lock_type::upgrade, Mutex> {
    static_assert(has_upgrade<Mutex>, "Mutex does not support upgrade lock");
    using type = std::unique_lock<Mutex>; // 升级锁使用 unique_lock 管理
};

template <typename Mutex>
struct lock_select<lock_type::try_upgrade, Mutex> {
    static_assert(has_upgrade<Mutex>, "Mutex does not support upgrade lock");
    using type = std::unique_lock<Mutex>;
};

template <lock_type LockType, typename Mutex>
using lock_type_t = typename lock_select<LockType, Mutex>::type;

template <class BoxType, bool IsSharedLock>
using box_data_type =
    std::conditional_t<std::is_const_v<BoxType> || IsSharedLock,
                       typename BoxType::data_type const, // 读锁和常量数据类型不允许修改
                       typename BoxType::data_type>; // 独占锁、写锁、升级锁以及非常量数据类型都允许修改

} // namespace detail

template <class BoxType, detail::lock_type LockType>
class scoped_unlocker;

/**
 * @brief 锁定指针，RAII方式管理锁
 */
template <typename BoxType, detail::lock_type LockType>
class locked_ptr {
public:
    using box_type   = std::remove_const_t<BoxType>;
    using data_type  = detail::box_data_type<BoxType, LockType == detail::lock_type::shared>;
    using mutex_type = typename box_type::mutex_type;
    using lock_type  = detail::lock_type_t<LockType, mutex_type>;

    /**
     * @brief 构造函数
     */
    explicit locked_ptr(BoxType* parent) : m_lock(!parent ? lock_type{} : do_lock(parent->m_mutex))
    {}

    explicit locked_ptr(lock_type lock) noexcept : m_lock(std::move(lock))
    {}

    locked_ptr(const locked_ptr&)            = delete;
    locked_ptr& operator=(const locked_ptr&) = delete;

    locked_ptr(locked_ptr&& rhs) noexcept : m_lock(std::move(rhs.m_lock))
    {}

    locked_ptr& operator=(locked_ptr&& rhs) noexcept
    {
        if (this != &rhs) {
            m_lock = std::move(rhs.m_lock);
        }
        return *this;
    }

    /**
     * @brief 析构函数
     */
    ~locked_ptr()
    {
        // 升级锁需要特殊的解锁方式
        if constexpr (LockType == detail::lock_type::upgrade || LockType == detail::lock_type::try_upgrade) {
            if (m_lock && m_lock.owns_lock()) {
                // 直接调用 unlock() 让 unique_lock 管理，但先手动解锁升级锁
                auto* mutex_ptr = m_lock.mutex();
                m_lock.release(); // 释放 unique_lock 的所有权
                if (mutex_ptr) {
                    mutex_ptr->unlock_upgrade(); // 手动解锁升级锁
                }
            }
        }
        // 其他锁类型由 lock_type 的析构函数自动处理
    }

    /**
     * @brief 解引用操作符
     */
    data_type* operator->() const
    {
        return &get_box().m_data;
    }

    data_type& operator*() const
    {
        return get_box().m_data;
    }

    /**
     * @brief 获取底层锁对象
     */
    const lock_type& as_lock() const noexcept
    {
        return m_lock;
    }

    /**
     * @brief 检查锁是否为空
     */
    bool is_null() const
    {
        return !m_lock;
    }

    /**
     * @brief 转换为bool（检查是否持有锁）
     */
    explicit operator bool() const
    {
        return static_cast<bool>(m_lock);
    }

    /**
     * @brief 释放锁（类似std::unique_lock::unlock）
     */
    void unlock() noexcept
    {
        m_lock = {};
    }

    /**
     * @brief 创建临时解锁器（作用域解锁）
     */
    scoped_unlocker<box_type, LockType> scoped_unlock()
    {
        return scoped_unlocker<box_type, LockType>(this);
    }

    /**
     * @brief 获取所属的 box 对象
     */
    box_type& get_box() const
    {
        return *box_type::to_box_ptr(m_lock.mutex());
    }

    /**
     * @brief 升级为写锁 - 仅升级锁类型可用
     *
     * 将当前的升级锁升级为写锁。如果当前有其他读锁存在，会等待直到所有读锁释放。
     */
    template <detail::lock_type L = LockType>
    std::enable_if_t<L == detail::lock_type::upgrade, locked_ptr<BoxType, detail::lock_type::unique>>
    upgrade_to_wlock() &&
    {
        if (m_lock && m_lock.owns_lock()) {
            auto* mutex_ptr = m_lock.mutex();
            if (mutex_ptr) {
                mutex_ptr->unlock_upgrade_and_lock();
                m_lock.release(); // 释放 unique_lock 的所有权
                auto new_lock = std::unique_lock<mutex_type>(*mutex_ptr, std::adopt_lock);
                return locked_ptr<BoxType, detail::lock_type::unique>(std::move(new_lock));
            }
        }
        return locked_ptr<BoxType, detail::lock_type::unique>(std::unique_lock<mutex_type>{});
    }

    /**
     * @brief 尝试升级为写锁 - 仅升级锁类型可用
     *
     * 尝试将当前的升级锁升级为写锁。如果当前有读锁存在，立即返回失败。
     */
    template <detail::lock_type L = LockType>
    std::enable_if_t<L == detail::lock_type::upgrade, std::optional<locked_ptr<BoxType, detail::lock_type::unique>>>
    try_upgrade_to_wlock() &&
    {
        if (m_lock && m_lock.owns_lock()) {
            auto* mutex_ptr = m_lock.mutex();
            if (mutex_ptr && mutex_ptr->try_unlock_upgrade_and_lock()) {
                m_lock.release(); // 释放 unique_lock 的所有权，因为升级锁已经变成写锁
                auto new_lock = std::unique_lock<mutex_type>(*mutex_ptr, std::adopt_lock);
                return locked_ptr<BoxType, detail::lock_type::unique>(std::move(new_lock));
            }
        }
        return std::nullopt;
    }

    /**
     * @brief 超时升级为写锁 - 仅升级锁类型可用
     */
    template <typename Duration, detail::lock_type L = LockType>
    std::enable_if_t<L == detail::lock_type::upgrade, std::optional<locked_ptr<BoxType, detail::lock_type::unique>>>
    try_upgrade_to_wlock_for(const Duration& timeout) &&
    {
        if (m_lock && m_lock.owns_lock()) {
            auto* mutex_ptr = m_lock.mutex();
            if (mutex_ptr) {
                using duration_type = std::chrono::steady_clock::duration;
                if (mutex_ptr->try_unlock_upgrade_and_lock_for(static_cast<duration_type>(timeout))) {
                    m_lock.release(); // 释放 unique_lock 的所有权
                    auto new_lock = std::unique_lock<mutex_type>(*mutex_ptr, std::adopt_lock);
                    return locked_ptr<BoxType, detail::lock_type::unique>(std::move(new_lock));
                }
            }
        }
        return std::nullopt;
    }

    /**
     * @brief 降级为读锁 - 仅升级锁类型可用
     */
    template <detail::lock_type L = LockType>
    std::enable_if_t<L == detail::lock_type::upgrade, locked_ptr<BoxType, detail::lock_type::shared>>
    downgrade_to_rlock() &&
    {
        if (m_lock) {
            m_lock.mutex()->unlock_upgrade_and_lock_shared();
            auto new_lock = std::shared_lock<mutex_type>(*m_lock.mutex(), std::adopt_lock);
            m_lock        = {}; // 释放当前锁
            return locked_ptr<BoxType, detail::lock_type::shared>(std::move(new_lock));
        }
        return locked_ptr<BoxType, detail::lock_type::shared>(std::shared_lock<mutex_type>{});
    }

    /**
     * @brief 降级为读锁 - 仅写锁类型可用
     */
    template <detail::lock_type L = LockType>
    std::enable_if_t<L == detail::lock_type::unique, locked_ptr<BoxType, detail::lock_type::shared>>
    downgrade_to_rlock() &&
    {
        if (m_lock) {
            m_lock.mutex()->unlock_and_lock_shared();
            auto new_lock = std::shared_lock<mutex_type>(*m_lock.mutex(), std::adopt_lock);
            m_lock        = {}; // 释放当前锁
            return locked_ptr<BoxType, detail::lock_type::shared>(std::move(new_lock));
        }
        return locked_ptr<BoxType, detail::lock_type::shared>(std::shared_lock<mutex_type>{});
    }

    /**
     * @brief 降级为升级锁 - 仅写锁类型可用
     */
    template <detail::lock_type L = LockType>
    std::enable_if_t<L == detail::lock_type::unique, locked_ptr<BoxType, detail::lock_type::upgrade>>
    downgrade_to_ulock() &&
    {
        if (m_lock) {
            m_lock.mutex()->unlock_and_lock_upgrade();
            auto new_lock = std::unique_lock<mutex_type>(*m_lock.mutex(), std::adopt_lock);
            m_lock        = {}; // 释放当前锁
            return locked_ptr<BoxType, detail::lock_type::upgrade>(std::move(new_lock));
        }
        return locked_ptr<BoxType, detail::lock_type::upgrade>(std::unique_lock<mutex_type>{});
    }

private:
    friend class scoped_unlocker<box_type, LockType>;

    template <typename MT>
    static lock_type do_lock(MT& mutex)
    {
        if constexpr (LockType == detail::lock_type::try_unique) {
            return lock_type{mutex, std::try_to_lock};
        } else if constexpr (LockType == detail::lock_type::try_shared) {
            return lock_type{mutex, std::try_to_lock};
        } else if constexpr (LockType == detail::lock_type::try_upgrade) {
            if (mutex.try_lock_upgrade()) {
                return lock_type{mutex, std::adopt_lock};
            }
            return lock_type{};
        } else if constexpr (LockType == detail::lock_type::unique) {
            return lock_type{mutex};
        } else if constexpr (LockType == detail::lock_type::shared) {
            return lock_type{mutex};
        } else if constexpr (LockType == detail::lock_type::upgrade) {
            mutex.lock_upgrade();
            return lock_type{mutex, std::adopt_lock};
        }
    }

    void release_lock() noexcept
    {
        m_lock = {};
    }

    void reacquire_lock(box_type& box)
    {
        m_lock = do_lock(box.m_mutex);
    }

    lock_type m_lock;
};

/**
 * @brief 临时解锁器
 */
template <class BoxType, detail::lock_type LockType>
class scoped_unlocker {
public:
    explicit scoped_unlocker(locked_ptr<BoxType, LockType>* p) noexcept
        : m_locked_ptr(p), m_box(p ? &p->get_box() : nullptr)
    {
        if (m_locked_ptr) {
            m_locked_ptr->release_lock();
        }
    }

    scoped_unlocker(const scoped_unlocker&)                    = delete;
    scoped_unlocker& operator=(const scoped_unlocker&)         = delete;
    scoped_unlocker(scoped_unlocker&& rhs) noexcept            = default;
    scoped_unlocker& operator=(scoped_unlocker&& rhs) noexcept = default;

    ~scoped_unlocker()
    {
        if (m_locked_ptr && m_box) {
            m_locked_ptr->reacquire_lock(*m_box);
        }
    }

private:
    locked_ptr<BoxType, LockType>* m_locked_ptr;
    std::remove_const_t<BoxType>*  m_box;
};

/**
 * @brief 互斥锁数据容器
 *
 * 将数据与互斥锁配对，数据只能通过locked_ptr访问
 *
 * @tparam T 要保护的数据类型
 * @tparam Mutex 互斥锁类型，默认为std::shared_mutex
 */
template <class T, class Mutex = std::shared_mutex>
struct mutex_box {
private:
    static constexpr bool has_copy_ctor = std::is_nothrow_copy_constructible_v<T>;
    static constexpr bool has_move_ctor = std::is_nothrow_move_constructible_v<T>;

    // 用于禁用拷贝构造和赋值的辅助类型
    class non_implemented_type;

    using box_param_type = std::conditional_t<std::is_copy_constructible_v<T>, const mutex_box&, non_implemented_type>;

    using data_traits      = mc::traits::property_traits<T>;
    using data_param_type  = typename data_traits::param_type;
    using data_rvalue_type = typename data_traits::rvalue_type;

public:
    using data_type  = T;
    using mutex_type = Mutex;

    // 锁定指针类型别名
    using w_locked_ptr           = locked_ptr<mutex_box, detail::lock_type::unique>;
    using const_w_locked_ptr     = locked_ptr<const mutex_box, detail::lock_type::unique>;
    using try_w_locked_ptr       = locked_ptr<mutex_box, detail::lock_type::try_unique>;
    using const_try_w_locked_ptr = locked_ptr<const mutex_box, detail::lock_type::try_unique>;

    // 升级锁类型别名（仅支持升级锁的互斥量）
    using u_locked_ptr           = locked_ptr<mutex_box, detail::lock_type::upgrade>;
    using const_u_locked_ptr     = locked_ptr<const mutex_box, detail::lock_type::upgrade>;
    using try_u_locked_ptr       = locked_ptr<mutex_box, detail::lock_type::try_upgrade>;
    using const_try_u_locked_ptr = locked_ptr<const mutex_box, detail::lock_type::try_upgrade>;

    // 读锁类型别名（仅支持读锁的互斥量）
    using r_locked_ptr           = locked_ptr<mutex_box, detail::lock_type::shared>;
    using const_r_locked_ptr     = locked_ptr<const mutex_box, detail::lock_type::shared>;
    using try_r_locked_ptr       = locked_ptr<mutex_box, detail::lock_type::try_shared>;
    using const_try_r_locked_ptr = locked_ptr<const mutex_box, detail::lock_type::try_shared>;

    /**
     * @brief 默认构造函数
     */
    constexpr mutex_box() = default;

    /**
     * @brief 拷贝构造函数（仅当T可拷贝构造时启用）
     */
    /* implicit */ mutex_box(box_param_type rhs) /* may throw */
        : mutex_box(rhs.copy())
    {}

    /**
     * @brief 移动构造函数
     */
    mutex_box(mutex_box&& rhs) noexcept(has_move_ctor) : mutex_box(std::move(rhs.m_data))
    {}

    /**
     * @brief 从数据拷贝构造
     */
    explicit mutex_box(const T& rhs) noexcept(has_copy_ctor) : m_data(rhs)
    {}

    /**
     * @brief 从数据移动构造
     */
    explicit mutex_box(T&& rhs) noexcept(has_move_ctor) : m_data(std::move(rhs))
    {}

    /**
     * @brief 原地构造
     */
    template <typename... Args>
    explicit constexpr mutex_box(std::in_place_t, Args&&... args) : m_data(std::forward<Args>(args)...)
    {}

    /**
     * @brief 分别构造数据和互斥锁
     */
    template <typename... DataArgs, typename... MutexArgs>
    mutex_box(std::piecewise_construct_t, std::tuple<DataArgs...> data_args, std::tuple<MutexArgs...> mutex_args)
        : mutex_box{std::piecewise_construct, std::move(data_args), std::move(mutex_args),
                    std::make_index_sequence<sizeof...(DataArgs)>{}, std::make_index_sequence<sizeof...(MutexArgs)>{}}
    {}

    /**
     * @brief 拷贝赋值运算符
     */
    mutex_box& operator=(box_param_type rhs)
    {
        return *this = rhs.copy();
    }

    /**
     * @brief 移动赋值运算符
     */
    mutex_box& operator=(mutex_box&& rhs)
    {
        return *this = std::move(rhs.m_data);
    }

    /**
     * @brief 从数据赋值
     */
    mutex_box& operator=(data_param_type rhs)
    {
        if (&m_data != &rhs) {
            auto guard = this->lock();
            m_data     = rhs;
        }
        return *this;
    }

    /**
     * @brief 从数据移动赋值
     */
    mutex_box& operator=(data_rvalue_type rhs)
    {
        if (&m_data != &rhs) {
            auto guard = this->lock();
            m_data     = std::move(rhs);
        }
        return *this;
    }

    /**
     * @brief 获取写锁（独占锁）
     */
    template <typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto wlock()
    {
        return w_locked_ptr(this);
    }
    template <typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto wlock() const
    {
        return const_w_locked_ptr(this);
    }

    /**
     * @brief 尝试获取写锁
     */
    template <typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto try_wlock()
    {
        return try_w_locked_ptr{this};
    }
    template <typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto try_wlock() const
    {
        return const_try_w_locked_ptr{this};
    }

    /**
     * @brief 通用锁定方法（根据互斥锁能力选择写锁）
     */
    template <typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto lock()
    {
        return wlock<M>();
    }
    template <typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto lock() const
    {
        return wlock<M>();
    }

    /**
     * @brief 通用尝试锁定方法
     */
    template <typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto try_lock()
    {
        return try_wlock<M>();
    }
    template <typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto try_lock() const
    {
        return try_wlock<M>();
    }

    /**
     * @brief 在写锁保护下执行函数
     */
    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto with_wlock(Function&& function)
    {
        return function(*wlock<M>());
    }
    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto with_wlock(Function&& function) const
    {
        return function(*wlock<M>());
    }

    /**
     * @brief 在写锁保护下执行函数（传递locked_ptr）
     */
    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto with_wlock_ptr(Function&& function)
    {
        return function(wlock<M>());
    }
    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto with_wlock_ptr(Function&& function) const
    {
        return function(wlock<M>());
    }

    /**
     * @brief 在锁保护下执行函数
     */
    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto with_lock(Function&& function)
    {
        return function(*lock<M>());
    }
    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto with_lock(Function&& function) const
    {
        return function(*lock<M>());
    }

    /**
     * @brief 在锁保护下执行函数（传递locked_ptr）
     */
    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto with_lock_ptr(Function&& function)
    {
        return function(lock<M>());
    }
    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_lock<M>, int> = 0>
    auto with_lock_ptr(Function&& function) const
    {
        return function(lock<M>());
    }

    /**
     * @brief 获取读锁（共享锁）- 仅当互斥锁支持时可用
     */
    template <typename M = Mutex, std::enable_if_t<detail::has_shared<M>, int> = 0>
    auto rlock()
    {
        return r_locked_ptr(this);
    }

    template <typename M = Mutex, std::enable_if_t<detail::has_shared<M>, int> = 0>
    auto rlock() const
    {
        return const_r_locked_ptr(this);
    }

    /**
     * @brief 尝试获取读锁 - 仅当互斥锁支持时可用
     */
    template <typename M = Mutex, std::enable_if_t<detail::has_shared<M>, int> = 0>
    auto try_rlock()
    {
        return try_r_locked_ptr(this);
    }

    template <typename M = Mutex, std::enable_if_t<detail::has_shared<M>, int> = 0>
    auto try_rlock() const
    {
        return const_try_r_locked_ptr(this);
    }

    /**
     * @brief 超时获取读锁 - 仅当互斥锁支持时可用
     */
    template <typename Duration, typename M = Mutex,
              std::enable_if_t<detail::has_shared<M> && detail::has_try_lock_shared_for<M>, int> = 0>
    auto try_rlock_for(const Duration& timeout)
    {
        using duration_type = std::chrono::steady_clock::duration;

        std::shared_lock<M> lock(m_mutex, std::defer_lock);
        if (lock.try_lock_for(static_cast<duration_type>(timeout))) {
            return r_locked_ptr(std::move(lock));
        }
        return r_locked_ptr(std::shared_lock<M>{}); // 空锁
    }

    template <typename Duration, typename M = Mutex,
              std::enable_if_t<detail::has_shared<M> && detail::has_try_lock_shared_for<M>, int> = 0>
    auto try_rlock_for(const Duration& timeout) const
    {
        using duration_type = std::chrono::steady_clock::duration;

        std::shared_lock<M> lock(m_mutex, std::defer_lock);
        if (lock.try_lock_for(static_cast<duration_type>(timeout))) {
            return const_r_locked_ptr(std::move(lock));
        }
        return const_r_locked_ptr(std::shared_lock<M>{}); // 空锁
    }

    /**
     * @brief 超时获取写锁 - 仅当互斥锁支持时可用
     */
    template <typename Duration, typename M = Mutex,
              std::enable_if_t<detail::has_lock<M> && detail::has_try_lock_for<M>, int> = 0>
    auto try_wlock_for(const Duration& timeout)
    {
        using duration_type = std::chrono::steady_clock::duration;

        std::unique_lock<M> lock(m_mutex, std::defer_lock);
        if (lock.try_lock_for(static_cast<duration_type>(timeout))) {
            return w_locked_ptr(std::move(lock));
        }
        return w_locked_ptr(std::unique_lock<M>{}); // 空锁
    }

    template <typename Duration, typename M = Mutex,
              std::enable_if_t<detail::has_lock<M> && detail::has_try_lock_for<M>, int> = 0>
    auto try_wlock_for(const Duration& timeout) const
    {
        using duration_type = std::chrono::steady_clock::duration;

        std::unique_lock<M> lock(m_mutex, std::defer_lock);
        if (lock.try_lock_for(static_cast<duration_type>(timeout))) {
            return const_w_locked_ptr(std::move(lock));
        }
        return const_w_locked_ptr(std::unique_lock<M>{}); // 空锁
    }

    /**
     * @brief 超时获取通用锁
     */
    template <typename Duration, typename M = Mutex,
              std::enable_if_t<detail::has_lock<M> && detail::has_try_lock_for<M>, int> = 0>
    auto try_lock_for(const Duration& timeout)
    {
        return try_wlock_for<Duration, M>(timeout);
    }

    template <typename Duration, typename M = Mutex,
              std::enable_if_t<detail::has_lock<M> && detail::has_try_lock_for<M>, int> = 0>
    auto try_lock_for(const Duration& timeout) const
    {
        return try_wlock_for<Duration, M>(timeout);
    }

    /**
     * @brief 在读锁保护下执行函数 - 仅当互斥锁支持时可用
     */
    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_shared<M>, int> = 0>
    auto with_rlock(Function&& function) const
    {
        return function(*rlock<M>());
    }

    /**
     * @brief 在读锁保护下执行函数（传递locked_ptr）- 仅当互斥锁支持时可用
     */
    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_shared<M>, int> = 0>
    auto with_rlock_ptr(Function&& function)
    {
        return function(rlock<M>());
    }

    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_shared<M>, int> = 0>
    auto with_rlock_ptr(Function&& function) const
    {
        return function(rlock<M>());
    }

    /**
     * @brief 获取升级锁 - 仅当互斥锁支持时可用
     *
     * 升级锁允许持有者稍后升级为写锁，避免了多个读锁同时升级的死锁问题。
     * 同一时间只能有一个线程持有升级锁，但可以与读锁共存。
     */
    template <typename M = Mutex, std::enable_if_t<detail::has_upgrade<M>, int> = 0>
    auto ulock()
    {
        return u_locked_ptr(this);
    }

    template <typename M = Mutex, std::enable_if_t<detail::has_upgrade<M>, int> = 0>
    auto ulock() const
    {
        return const_u_locked_ptr(this);
    }

    /**
     * @brief 尝试获取升级锁 - 仅当互斥锁支持时可用
     */
    template <typename M = Mutex, std::enable_if_t<detail::has_upgrade<M>, int> = 0>
    auto try_ulock()
    {
        return try_u_locked_ptr(this);
    }

    template <typename M = Mutex, std::enable_if_t<detail::has_upgrade<M>, int> = 0>
    auto try_ulock() const
    {
        return const_try_u_locked_ptr(this);
    }

    /**
     * @brief 超时获取升级锁 - 仅当互斥锁支持时可用
     */
    template <typename Duration, typename M = Mutex,
              std::enable_if_t<detail::has_upgrade<M> && detail::has_try_lock_upgrade_for<M>, int> = 0>
    auto try_ulock_for(const Duration& timeout)
    {
        using duration_type = std::chrono::steady_clock::duration;

        std::unique_lock<M> lock(m_mutex, std::defer_lock);
        if (m_mutex.try_lock_upgrade_for(static_cast<duration_type>(timeout))) {
            lock = std::unique_lock<M>(m_mutex, std::adopt_lock);
            return ::mc::sync::locked_ptr<mutex_box, detail::lock_type::upgrade>(std::move(lock));
        }
        return ::mc::sync::locked_ptr<mutex_box, detail::lock_type::upgrade>(std::unique_lock<M>{}); // 空锁
    }

    template <typename Duration, typename M = Mutex,
              std::enable_if_t<detail::has_upgrade<M> && detail::has_try_lock_upgrade_for<M>, int> = 0>
    auto try_ulock_for(const Duration& timeout) const
    {
        using duration_type = std::chrono::steady_clock::duration;

        std::unique_lock<M> lock(m_mutex, std::defer_lock);
        if (m_mutex.try_lock_upgrade_for(static_cast<duration_type>(timeout))) {
            lock = std::unique_lock<M>(m_mutex, std::adopt_lock);
            return ::mc::sync::locked_ptr<const mutex_box, detail::lock_type::upgrade>(std::move(lock));
        }
        return ::mc::sync::locked_ptr<const mutex_box, detail::lock_type::upgrade>(std::unique_lock<M>{}); // 空锁
    }

    /**
     * @brief 在升级锁保护下执行函数 - 仅当互斥锁支持时可用
     */
    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_upgrade<M>, int> = 0>
    auto with_ulock(Function&& function)
    {
        return function(*ulock<M>());
    }

    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_upgrade<M>, int> = 0>
    auto with_ulock(Function&& function) const
    {
        return function(*ulock<M>());
    }

    /**
     * @brief 在升级锁保护下执行函数（传递locked_ptr）- 仅当互斥锁支持时可用
     */
    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_upgrade<M>, int> = 0>
    auto with_ulock_ptr(Function&& function)
    {
        return function(ulock<M>());
    }

    template <class Function, typename M = Mutex, std::enable_if_t<detail::has_upgrade<M>, int> = 0>
    auto with_ulock_ptr(Function&& function) const
    {
        return function(ulock<M>());
    }

    /**
     * @brief 交换数据
     */
    void swap(mutex_box& rhs)
    {
        if (this == &rhs) {
            return;
        }

        // 使用 std::lock 避免死锁
        std::unique_lock<Mutex> lock1(this->m_mutex, std::defer_lock);
        std::unique_lock<Mutex> lock2(rhs.m_mutex, std::defer_lock);
        std::lock(lock1, lock2);

        std::swap(m_data, rhs.m_data);
    }

    /**
     * @brief 与外部数据交换
     */
    void swap(T& rhs)
    {
        auto guard = this->lock();
        std::swap(m_data, rhs);
    }

    /**
     * @brief 交换并返回原值
     */
    T exchange(T&& rhs)
    {
        swap(rhs);
        return std::move(rhs);
    }

    /**
     * @brief 拷贝数据到目标
     */
    void copy(T& target) const
    {
        if constexpr (detail::has_shared<Mutex>) {
            auto guard = this->rlock();
            target     = m_data;
        } else {
            auto guard = this->lock();
            target     = m_data;
        }
    }

    /**
     * @brief 返回数据的拷贝
     */
    T copy() const
    {
        if constexpr (detail::has_shared<Mutex>) {
            auto guard = this->rlock();
            return m_data;
        } else {
            auto guard = this->lock();
            return m_data;
        }
    }

    /**
     * @brief 不安全地获取未锁定的数据引用
     *
     * 仅在确保线程安全的情况下使用
     */
    T& unsafe_get_data()
    {
        return m_data;
    }
    const T& unsafe_get_data() const
    {
        return m_data;
    }

    /**
     * @brief 不安全地直接访问互斥锁
     */
    Mutex& unsafe_get_mutex() const
    {
        return m_mutex;
    }

private:
    template <typename BoxType, detail::lock_type LockType>
    friend class ::mc::sync::locked_ptr;

    template <typename... DataArgs, typename... MutexArgs, std::size_t... IndicesOne, std::size_t... IndicesTwo>
    mutex_box(std::piecewise_construct_t, std::tuple<DataArgs...> data_args, std::tuple<MutexArgs...> mutex_args,
              std::index_sequence<IndicesOne...>, std::index_sequence<IndicesTwo...>)
        : m_data{std::get<IndicesOne>(std::move(data_args))...}, m_mutex{std::get<IndicesTwo>(std::move(mutex_args))...}
    {}

    // 模拟成员布局用于标准布局的 offsetof 计算
    struct simul_t {
        using data_storage_t  = std::aligned_storage_t<sizeof(T), alignof(T)>;
        using mutex_storage_t = std::aligned_storage_t<sizeof(Mutex), alignof(Mutex)>;
        data_storage_t  m_data;
        mutex_storage_t m_mutex;
    };

    // 数据成员
    T             m_data{};
    mutable Mutex m_mutex;

    static mutex_box* to_box_ptr(void* mutex)
    {
        static_assert(sizeof(simul_t) == sizeof(mutex_box) && alignof(simul_t) == alignof(mutex_box), "mismatch");
        auto       off = offsetof(simul_t, m_mutex);
        const auto raw = reinterpret_cast<char*>(mutex);
        return reinterpret_cast<mutex_box*>(raw - (raw ? off : 0));
    }
};

/**
 * @brief 安全地同时锁定两个 mutex_box 对象
 *
 * 使用 std::lock 避免死锁，返回锁定的指针对
 *
 * @param box1 第一个要锁定的 mutex_box 对象
 * @param box2 第二个要锁定的 mutex_box 对象
 * @return 锁定指针的 pair
 *
 * 使用示例：
 * auto [locked1, locked2] = lock_pair(box1, box2);
 * *locked1 = 100;
 * *locked2 = 200;
 */
template <typename Box1, typename Box2>
auto lock_pair(Box1& box1, Box2& box2)
{
    using mutex1_type = typename Box1::mutex_type;
    using mutex2_type = typename Box2::mutex_type;

    std::unique_lock<mutex1_type> lock1(box1.unsafe_get_mutex(), std::defer_lock);
    std::unique_lock<mutex2_type> lock2(box2.unsafe_get_mutex(), std::defer_lock);

    std::lock(lock1, lock2);

    return std::make_pair(typename Box1::w_locked_ptr(std::move(lock1)), typename Box2::w_locked_ptr(std::move(lock2)));
}

/**
 * @brief 安全地同时锁定多个 mutex_box 对象
 *
 * 使用 std::lock 避免死锁，返回锁定的指针元组
 *
 * @param boxes 要锁定的 mutex_box 对象
 * @return 锁定指针的元组
 *
 * 使用示例：
 * auto [locked1, locked2, locked3] = lock(box1, box2, box3);
 * *locked1 = 100;
 * *locked2 = 200;
 * *locked3 = 300;
 */
template <typename... Boxes>
auto lock(Boxes&... boxes)
{
    static_assert(sizeof...(Boxes) > 0, "至少需要一个 mutex_box");

    auto locks =
        std::make_tuple(std::unique_lock<typename Boxes::mutex_type>(boxes.unsafe_get_mutex(), std::defer_lock)...);

    std::apply([](auto&... lock_refs) {
        std::lock(lock_refs...);
    }, locks);

    return std::apply([](auto&&... lock_refs) {
        return std::make_tuple(typename Boxes::w_locked_ptr(std::move(lock_refs))...);
    }, std::move(locks));
}
} // namespace mc::sync

namespace mc {

// 导出到 mc 命名空间
using sync::lock;
using sync::lock_pair;
using sync::mutex_box;

} // namespace mc

#endif // MC_SYNC_MUTEX_BOX_H
