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

#ifndef MC_RUNTIME_EXECUTOR_H
#define MC_RUNTIME_EXECUTOR_H

#include <mc/common.h>
#include <mc/exception.h>

#include <boost/asio.hpp>

#include <atomic>
#include <memory>
#include <type_traits>
#include <typeinfo>

namespace mc::runtime {
using execution_context = boost::asio::execution_context;

/**
 * @brief 执行器包装器，支持包装任意 boost::asio 执行器
 */
class MC_API executor {
public:
    /**
     * @brief 默认构造函数
     */
    executor() = default;

    /**
     * @brief 从任意执行器构造
     * @tparam Executor 执行器类型
     * @param exec 执行器对象
     */
    template <typename Executor, typename Allocator = std::allocator<void>,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<Executor>, executor>>>
    explicit executor(Executor&& exec, const Allocator& allocator = Allocator());

    /**
     * @brief 拷贝构造函数
     */
    executor(const executor& other) noexcept;

    /**
     * @brief 移动构造函数
     */
    executor(executor&& other) noexcept;

    /**
     * @brief 拷贝赋值运算符
     */
    executor& operator=(const executor& other) noexcept;

    /**
     * @brief 移动赋值运算符
     */
    executor& operator=(executor&& other) noexcept;

    /**
     * @brief 析构函数
     */
    ~executor();

    /**
     * @brief 提交任务到队列末尾执行
     */
    template <typename Function, typename Allocator = std::allocator<void>>
    auto post(Function&& token, const Allocator& a = Allocator()) const;

    /**
     * @brief 延迟执行任务
     */
    template <typename Function, typename Allocator = std::allocator<void>>
    auto defer(Function&& token, const Allocator& a = Allocator()) const;

    /**
     * @brief 分发任务（可能立即执行）
     */
    template <typename Function, typename Allocator = std::allocator<void>>
    auto dispatch(Function&& token, const Allocator& a = Allocator()) const;

    /**
     * @brief 检查执行器是否有效
     */
    bool valid() const noexcept;
    /**
     * @brief 比较两个执行器是否相等
     */
    bool operator==(const executor& other) const noexcept;

    /**
     * @brief 比较两个执行器是否不等
     */
    bool operator!=(const executor& other) const noexcept;

    void               on_work_started() const noexcept;
    void               on_work_finished() const noexcept;
    execution_context& context() const;

private:
    using function = boost::asio::detail::executor_function;
    class impl_base {
    public:
        impl_base() : m_ref_count(1) {
        }

        virtual ~impl_base() = default;

        virtual void                  post(function&&) const              = 0;
        virtual void                  defer(function&&) const             = 0;
        virtual void                  dispatch(function&&) const          = 0;
        virtual bool                  equal(const impl_base& other) const = 0;
        virtual std::type_info const& target_type() const                 = 0;
        virtual void                  on_work_started() const noexcept    = 0;
        virtual void                  on_work_finished() const noexcept   = 0;
        virtual execution_context&    context() const                     = 0;

        // 引用计数管理
        void add_ref() const noexcept {
            m_ref_count.fetch_add(1, std::memory_order_relaxed);
        }

        bool release() const noexcept {
            return m_ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1;
        }

        std::size_t use_count() const noexcept {
            return m_ref_count.load(std::memory_order_acquire);
        }

    private:
        mutable std::atomic<std::size_t> m_ref_count;
    };

    template <typename Executor, typename Allocator>
    class impl : public impl_base {
    public:
        explicit impl(Executor executor, const Allocator& allocator)
            : m_executor(std::move(executor)), m_allocator(allocator) {
        }

        void post(function&& f) const override {
            m_executor.post(std::forward<function&&>(f), m_allocator);
        }

        void defer(function&& f) const override {
            m_executor.defer(std::forward<function&&>(f), m_allocator);
        }

        void dispatch(function&& f) const override {
            m_executor.dispatch(std::forward<function&&>(f), m_allocator);
        }

        bool equal(const impl_base& other) const override {
            if (target_type() != other.target_type()) {
                return false;
            }
            const auto* other_impl = static_cast<const impl<Executor, Allocator>*>(&other);
            return m_executor == other_impl->m_executor;
        }

        std::type_info const& target_type() const override {
            return typeid(Executor);
        }

        const Executor& get_executor() const {
            return m_executor;
        }

        void on_work_started() const noexcept override {
            m_executor.on_work_started();
        }

        void on_work_finished() const noexcept override {
            m_executor.on_work_finished();
        }

        execution_context& context() const override {
            return m_executor.context();
        }

    private:
        Executor  m_executor;
        Allocator m_allocator;
    };

    impl_base* m_impl{nullptr};
};

template <typename Executor, typename Allocator, typename>
executor::executor(Executor&& exec, const Allocator& allocator)
    : m_impl(new impl<std::decay_t<Executor>, std::decay_t<Allocator>>(
          std::forward<Executor>(exec), allocator)) {
}

template <typename Function, typename Allocator>
auto executor::post(Function&& f, const Allocator& a) const {
    MC_ASSERT_THROW(m_impl, mc::invalid_op_exception, "post on invalid executor");
    m_impl->post(function(std::forward<Function&&>(f), a));
}

template <typename Function, typename Allocator>
auto executor::defer(Function&& f, const Allocator& a) const {
    MC_ASSERT_THROW(m_impl, mc::invalid_op_exception, "defer on invalid executor");
    m_impl->defer(function(std::forward<Function&&>(f), a));
}

template <typename Function, typename Allocator>
auto executor::dispatch(Function&& f, const Allocator& a) const {
    MC_ASSERT_THROW(m_impl, mc::invalid_op_exception, "dispatch on invalid executor");
    m_impl->dispatch(function(std::forward<Function&&>(f), a));
}

} // namespace mc::runtime

#endif // MC_RUNTIME_EXECUTOR_H