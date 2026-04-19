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

#include <mc/exception.h>
#include <mc/futures/any_promise.h>
#include <mc/futures/state.h>

namespace mc::futures {

namespace detail {

bool handle_result_state_common(any_promise& promise, state_base& state)
{
    if (state.is_cancelled()) {
        promise.cancel();
        return true;
    }

    if (state.is_rejected()) {
        state.copy_exception_to(promise);
        return true;
    }

    return false;
}

} // namespace detail

namespace {

void* payload_ptr_erased_impl(unsigned char* slot, detail::storage_kind storage_kind,
                              detail::payload_layout layout) noexcept
{
    switch (storage_kind) {
        case detail::storage_kind::inline_storage:
            return layout == detail::payload_layout::inline_object ? static_cast<void*>(slot) : nullptr;
        case detail::storage_kind::external_storage:
            if (layout != detail::payload_layout::external_pointer) {
                return nullptr;
            }
            return *std::launder(reinterpret_cast<void**>(slot));
        case detail::storage_kind::empty:
        default:
            return nullptr;
    }
}

const void* payload_ptr_erased_impl(const unsigned char* slot, detail::storage_kind storage_kind,
                                    detail::payload_layout layout) noexcept
{
    switch (storage_kind) {
        case detail::storage_kind::inline_storage:
            return layout == detail::payload_layout::inline_object ? static_cast<const void*>(slot) : nullptr;
        case detail::storage_kind::external_storage:
            if (layout != detail::payload_layout::external_pointer) {
                return nullptr;
            }
            return *std::launder(reinterpret_cast<void* const*>(slot));
        case detail::storage_kind::empty:
        default:
            return nullptr;
    }
}

} // namespace

state_base::state_base(executor_type executor) noexcept : m_executor(std::move(executor))
{}

state_base::~state_base()
{
    destroy_deferred_task_locked();
    destroy_slot_locked();
}

bool state_base::try_mark_cancelled() noexcept
{
    bool expected = false;
    if (!m_cancel_requested.compare_exchange_strong(expected, true, std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
        return false;
    }

    std::lock_guard lock(m_mutex);
    if (get_completion_state() != completion_state::pending) {
        m_cancel_requested.store(get_completion_state() == completion_state::cancelled, std::memory_order_release);
        return false;
    }
    return true;
}

void state_base::mark_bound() noexcept
{
    set_flag(s_bound_flag, std::memory_order_release);
}

std::exception_ptr state_base::get_exception_locked() const noexcept
{
    return m_exception;
}

void state_base::validate_get_value_locked() const
{
    if (is_cancelled() || is_rejected()) {
        std::rethrow_exception(get_exception_locked());
    }
    MC_ASSERT_THROW(is_ready(), mc::runtime_exception, "Future 未就绪");
}

void state_base::set_executor(executor_type executor) noexcept
{
    m_executor = std::move(executor);
}

void state_base::set_policy(launch policy) noexcept
{
    auto flags = m_state_flags.load(std::memory_order_acquire);
    while (true) {
        const auto desired = (flags & ~s_policy_mask) | encode_policy_bits(policy);
        if (m_state_flags.compare_exchange_weak(flags, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return;
        }
    }
}

void state_base::set_ready()
{
    if (get_completion_state() == completion_state::cancelled) {
        return;
    }
    set_completion_state(m_exception == nullptr ? completion_state::resolved : completion_state::rejected);
}

void state_base::set_completion_state(completion_state state) noexcept
{
    auto flags = m_state_flags.load(std::memory_order_acquire);
    while (true) {
        const auto desired = (flags & ~s_completion_state_mask) | encode_completion_state_bits(state);
        if (m_state_flags.compare_exchange_weak(flags, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return;
        }
    }
}

void state_base::set_storage_kind(detail::storage_kind kind) noexcept
{
    auto flags = m_state_flags.load(std::memory_order_acquire);
    while (true) {
        const auto desired = (flags & ~s_storage_kind_mask) | encode_storage_kind_bits(kind);
        if (m_state_flags.compare_exchange_weak(flags, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return;
        }
    }
}

std::exception_ptr state_base::get_exception() const noexcept
{
    const auto state = get_completion_state();
    if (state != completion_state::rejected && state != completion_state::cancelled) {
        return nullptr;
    }

    if (m_exception == nullptr) {
        return nullptr;
    }

    std::lock_guard lock(m_mutex);
    return get_exception_locked();
}

void state_base::set_exception(std::exception_ptr exception) noexcept
{
    if (exception == nullptr) {
        m_exception = nullptr;
        return;
    }

    m_exception = std::move(exception);
}

void* state_base::payload_ptr_erased(detail::payload_layout layout) noexcept
{
    return payload_ptr_erased_impl(m_slot, get_storage_kind(), layout);
}

const void* state_base::payload_ptr_erased(detail::payload_layout layout) const noexcept
{
    return payload_ptr_erased_impl(m_slot, get_storage_kind(), layout);
}

void state_base::destroy_slot_locked() noexcept
{
    if (!is_payload_constructed()) {
        set_storage_kind(detail::storage_kind::empty);
        set_payload_constructed(false);
        return;
    }

    if (m_storage_ops != nullptr) {
        m_storage_ops->destroy_slot(*this);
        return;
    }

    set_storage_kind(detail::storage_kind::empty);
    set_payload_constructed(false);
}

std::shared_ptr<mc::exception> state_base::get_exception_object() const noexcept
{
    auto exception = get_exception();
    if (!exception) {
        return {};
    }

    std::shared_ptr<mc::exception> result;
    try {
        std::rethrow_exception(exception);
    } catch (const mc::exception& mc_ex) {
        result = mc_ex.dynamic_copy_exception();
    } catch (const std::exception& std_ex) {
        auto wrapped = mc::std_exception_wrapper::from_current_exception(std_ex);
        result       = wrapped.dynamic_copy_exception();
    } catch (...) {
        auto unknown = MC_MAKE_EXCEPTION(mc::exception, "Unknown Future Exception");
        result       = unknown.dynamic_copy_exception();
    }
    return result;
}

void state_base::rethrow_exception() const
{
    std::rethrow_exception(get_exception());
}

void state_base::copy_exception_to(any_promise& promise, bool strict_once) const
{
    auto exception = get_exception();
    if (!exception) {
        return;
    }

    try {
        std::rethrow_exception(exception);
    } catch (const mc::exception& mc_ex) {
        promise.set_exception(mc_ex, strict_once);
    } catch (...) {
        promise.set_current_exception(strict_once);
    }
}

void state_base::reset()
{
    destroy_deferred_task_locked();
    destroy_slot_locked();
    m_state_flags.store(encode_completion_state_bits(completion_state::pending) | encode_policy_bits(launch::async),
                        std::memory_order_release);
    m_cancel_requested.store(false, std::memory_order_release);
    m_executor = executor_type{};
    m_continuations.clear();
    m_cancel_callbacks.clear();
    m_exception   = nullptr;
    m_storage_ops = nullptr;
}

void state_base::mark_ready()
{
    // launch::deferred：把发布动作推迟到 executor 下一 tick；executor 不可用时降级同步发布。
    const auto policy = decode_policy_bits(m_state_flags.load(std::memory_order_acquire));
    if (policy == launch::deferred && m_executor.valid()) {
        state_base_ptr self{this};
        m_executor.post([self = std::move(self)]() mutable noexcept {
            self->mark_ready_immediate();
        });
        return;
    }
    mark_ready_immediate();
}

void state_base::mark_ready_immediate() noexcept
{
    callback_list callbacks;
    callback_list cancel_callbacks;
    {
        std::lock_guard lock(m_mutex);
        set_ready();
        m_continuations.swap(callbacks);
        m_cancel_callbacks.swap(cancel_callbacks);
    }
    m_cv.notify_all();
    callbacks.execute_and_clear();

    // 直接清空 cancel_callbacks 因为不再需要执行了
    cancel_callbacks.clear();
}

void state_base::install_deferred_task(deferred_task_fn task_fn, void* context, deferred_task_dtor ctx_dtor)
{
    MC_ASSERT(task_fn != nullptr, "deferred task_fn 不可为 nullptr");
    MC_ASSERT(m_deferred_task_fn == nullptr, "同一 state 上不可重复装载 deferred task");
    MC_ASSERT(!is_deferred_task_started(), "deferred task 已启动后不可再次装载");
    MC_ASSERT(!is_ready(), "已就绪 state 不可装载 deferred task");

    m_deferred_task_fn   = task_fn;
    m_deferred_task_ctx  = context;
    m_deferred_task_dtor = ctx_dtor;
}

bool state_base::try_start_deferred_task() noexcept
{
    if (m_deferred_task_fn == nullptr) {
        return false;
    }

    const auto flags_snapshot = m_state_flags.load(std::memory_order_acquire);
    if (decode_policy_bits(flags_snapshot) != launch::deferred) {
        return false;
    }

    // 已是终态（cancel/exception/resolved）：不再启动任务体，避免无效调度。
    if (decode_completion_state_bits(flags_snapshot) != completion_state::pending) {
        return false;
    }

    auto flags = flags_snapshot;
    while (true) {
        if ((flags & s_deferred_task_started_flag) != 0) {
            return false;
        }
        const auto desired = flags | s_deferred_task_started_flag;
        if (m_state_flags.compare_exchange_weak(flags, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
            break;
        }
    }

    auto fn  = m_deferred_task_fn;
    auto ctx = m_deferred_task_ctx;

    if (!m_executor.valid()) {
        fn(ctx);
        return true;
    }

    // self 引用保活 state 与 ctx 直到任务体执行完。
    state_base_ptr self{this};
    m_executor.post([self = std::move(self), fn, ctx]() mutable noexcept {
        fn(ctx);
    });
    return true;
}

void state_base::destroy_deferred_task_locked() noexcept
{
    if (m_deferred_task_fn == nullptr) {
        return;
    }
    auto dtor            = m_deferred_task_dtor;
    auto ctx             = m_deferred_task_ctx;
    m_deferred_task_fn   = nullptr;
    m_deferred_task_ctx  = nullptr;
    m_deferred_task_dtor = nullptr;
    if (dtor != nullptr) {
        dtor(ctx);
    }
}

void state_base::cancel()
{
    // 先检查是否已经 ready，如果已经完成则忽略取消操作
    if (is_ready()) {
        return;
    }

    if (!try_mark_cancelled()) {
        return;
    }

    // 移动到临时变量执行，避免回调过程中修改链表或死锁
    callback_list callbacks;
    {
        std::lock_guard lock(m_mutex);
        m_cancel_callbacks.swap(callbacks);
    }
    callbacks.execute_and_clear();

    {
        std::lock_guard lock(m_mutex);
        if (get_completion_state() == completion_state::pending) {
            set_exception(std::make_exception_ptr(mc::canceled_exception()));
            set_completion_state(completion_state::cancelled);
        }
    }

    mark_ready();
}

void state_base::add_cancel_callback(callback_type callback)
{
    if (!is_ready() && !is_cancel_requested()) {
        std::lock_guard lock(m_mutex);
        if (!is_ready() && !is_cancel_requested()) {
            m_cancel_callbacks.push_back(std::move(callback));
            return;
        }
    }

    if (is_cancel_requested()) {
        safe_invoke(std::move(callback));
    }
}

void state_base::destory()
{
    reset();
}

} // namespace mc::futures

namespace mc::memory::detail {

void shared_release_ops_for<mc::futures::state_base>::deallocate(shared_base* base) noexcept
{
    auto* state = static_cast<mc::futures::state_base*>(base);
    if (!state) {
        return;
    }
    if (!mc::futures::detail::pooled_state_try_release(state)) {
        state->~state_base();
        free(state);
    }
}

} // namespace mc::memory::detail
