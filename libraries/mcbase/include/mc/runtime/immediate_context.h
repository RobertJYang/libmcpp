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

#ifndef MC_RUNTIME_IMMEDIATE_CONTEXT_H
#define MC_RUNTIME_IMMEDIATE_CONTEXT_H

#include <mc/common.h>

namespace mc::runtime {

class immediate_context;
class MC_API immediate_executor {
public:
    immediate_executor();
    ~immediate_executor();

    immediate_executor(const immediate_executor&);
    immediate_executor& operator=(const immediate_executor&);

    // 在当前线程立即执行工作
    template <typename F>
    void execute(F&& f) const
    {
        std::forward<F>(f)();
    }

    bool operator==(const immediate_executor&) const noexcept;
    bool operator!=(const immediate_executor&) const noexcept;

    // 在当前线程立即派发工作
    template <typename F, typename Allocator>
    void dispatch(F&& f, const Allocator& alloc = Allocator()) const
    {
        MC_UNUSED(alloc);
        std::forward<F>(f)();
    }

    // 在当前线程立即投递工作
    template <typename F, typename Allocator>
    void post(F&& f, const Allocator& alloc = Allocator()) const
    {
        MC_UNUSED(alloc);
        std::forward<F>(f)();
    }

    // 延迟执行（在这里等同于立即执行）
    template <typename F, typename Allocator>
    void defer(F&& f, const Allocator& alloc = Allocator()) const
    {
        MC_UNUSED(alloc);
        std::forward<F>(f)();
    }

    void on_work_started() const noexcept;

    void on_work_finished() const noexcept;

    /**
     * @brief 检查当前线程是否在此 executor 上执行
     * @note immediate_executor 总是在当前线程执行，所以始终返回 true
     */
    bool running_in_this_thread() const noexcept
    {
        return true;
    }
};

// immediate_context 用于立即执行回调的上下文
class MC_API immediate_context {
public:
    using executor_type = immediate_executor;

    immediate_context();
    ~immediate_context();

    executor_type get_executor() const noexcept;
};

} // namespace mc::runtime

#endif // MC_RUNTIME_IMMEDIATE_CONTEXT_H