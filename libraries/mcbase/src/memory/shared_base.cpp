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
#include <mc/memory/shared_base.h>

namespace {

struct final_release_context_storage {
    mc::memory::shared_base* counter;
};

void execute_final_release_impl(mc::gc::gc_final_release_context_t opaque)
{
    auto* ctx = static_cast<final_release_context_storage*>(opaque);
    auto* counter = ctx->counter;
    mc::memory::detail::destroy_managed_object(counter);
    if (counter->release_weak_ref()) {
        mc::memory::detail::deallocate_managed_object(counter);
    }
    delete ctx;
}

} // namespace

namespace mc::memory {

shared_counter::shared_counter()
    : m_ref_count(INVALID),
      m_weak_count(1),
      m_alloc_base(nullptr),
      m_release_ops(nullptr)
{
    gc_init();
}

shared_counter::~shared_counter()
{
    if (gc_index != gc::GCHead::GC_INDEX_UNTRACKED) {
        ::mc::gc::gc_untrack_hook_t fn = ::mc::gc::gc_untrack_hook();
        if (fn) {
            fn(static_cast<::mc::gc::GCHead*>(this));
        }
    }
}

shared_counter::shared_counter(const shared_counter&)
    : m_ref_count(INVALID),
      m_weak_count(1),
      m_alloc_base(nullptr),
      m_release_ops(nullptr)
{
    gc_init();
}

shared_counter& shared_counter::operator=(const shared_counter&)
{
    return *this;
}

shared_counter::shared_counter(shared_counter&&) noexcept
    : m_ref_count(INVALID),
      m_weak_count(1),
      m_alloc_base(nullptr),
      m_release_ops(nullptr)
{
    gc_init();
}

shared_counter& shared_counter::operator=(shared_counter&&) noexcept
{
    return *this;
}

void shared_counter::add_ref() const
{
    if (is_external_owned()) {
        return;
    }

    atomic_counter_ref ref(m_ref_count);
    counter_type       current = ref.load(std::memory_order_acquire);

    while (current != DESTROYED && current != EXTERNAL) {
        counter_type next = current == INVALID ? 1 : current + 1;
        if (ref.compare_exchange_weak(current, next, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return;
        }
    }

    detail::throw_invalid_op_exception("attempt to add reference to a destroyed object");
}

bool shared_counter::release_ref() const
{
    if (is_external_owned()) {
        return false;
    }

    atomic_counter_ref ref(m_ref_count);
    counter_type       current = ref.load(std::memory_order_acquire);

    while (current != DESTROYED && current != INVALID && current != EXTERNAL) {
        if (ref.compare_exchange_weak(current, current - 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return current == 1;
        }
    }

    detail::throw_invalid_op_exception(
        "attempt to release reference to a destroyed or not managed object");
}

void shared_counter::add_weak_ref() const
{
    if (is_external_owned()) {
        return;
    }
    atomic_counter_ref(m_weak_count).fetch_add(1, std::memory_order_relaxed);
}

bool shared_counter::release_weak_ref() const
{
    if (is_external_owned()) {
        return false;
    }
    if (atomic_counter_ref(m_weak_count).fetch_sub(1, std::memory_order_acq_rel) == 1) {
        return is_destroyed();
    }

    return false;
}

shared_counter::counter_type shared_counter::ref_count() const
{
    if (is_external_owned()) {
        return 0;
    }
    return atomic_counter_ref(m_ref_count).load(std::memory_order_relaxed);
}

shared_counter::counter_type shared_counter::weak_count() const
{
    return atomic_counter_ref(m_weak_count).load(std::memory_order_relaxed);
}

bool shared_counter::try_add_ref() const
{
    if (is_external_owned()) {
        return false;
    }

    atomic_counter_ref ref(m_ref_count);
    counter_type       current = ref.load(std::memory_order_acquire);

    while (current != DESTROYED && current != INVALID && current != EXTERNAL) {
        if (ref.compare_exchange_weak(current, current + 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }
    }

    return false;
}

bool shared_counter::is_managed() const
{
    counter_type current = atomic_counter_ref(m_ref_count).load(std::memory_order_acquire);
    return current != INVALID;
}

bool shared_counter::is_destroyed() const
{
    counter_type current = atomic_counter_ref(m_ref_count).load(std::memory_order_acquire);
    return current == DESTROYED;
}

bool shared_counter::is_externally_owned() const
{
    return is_external_owned();
}

bool shared_counter::externalize_ownership()
{
    atomic_counter_ref ref(m_ref_count);
    counter_type       current = ref.load(std::memory_order_acquire);

    if (current == INVALID || current == DESTROYED) {
        return false;
    }
    if (is_external_owned()) {
        return true;
    }
    if (weak_count() != 1) {
        return false;
    }

    set_external_owned(true);
    ref.store(EXTERNAL, std::memory_order_release);
    return true;
}

void shared_counter::set_shared_release_protocol(const shared_release_ops* ops,
                                                 void* alloc_base) noexcept
{
    m_release_ops = ops;
    m_alloc_base  = alloc_base ? alloc_base : const_cast<shared_counter*>(this);
}

void shared_counter::ensure_shared_release_protocol(const shared_release_ops* ops,
                                                    void* alloc_base) noexcept
{
    if (!m_release_ops) {
        set_shared_release_protocol(ops, alloc_base);
    }
}

const shared_release_ops* shared_counter::shared_release_protocol() const noexcept
{
    return m_release_ops;
}

void* shared_counter::shared_alloc_base() const noexcept
{
    return m_alloc_base ? m_alloc_base : const_cast<shared_counter*>(this);
}

} // namespace mc::memory

namespace mc::memory::detail {

void destroy_managed_object(shared_base* counter)
{
    auto* ops = counter ? counter->shared_release_protocol() : nullptr;
    if (!ops || !ops->destroy) {
        throw_invalid_op_exception("shared object missing destroy release protocol");
    }
    ops->destroy(counter);
}

void deallocate_managed_object(shared_base* counter)
{
    auto* ops = counter ? counter->shared_release_protocol() : nullptr;
    if (!ops || !ops->deallocate) {
        throw_invalid_op_exception("shared object missing deallocate release protocol");
    }
    ops->deallocate(counter);
}

bool try_dispatch_managed_final_release(gc::GCHead* head, shared_base* counter)
{
    if (!head || !counter) {
        return false;
    }

    auto* ctx = new final_release_context_storage{counter};
    if (::mc::gc::gc_try_dispatch_final_release(head, ctx, execute_final_release_impl)) {
        return true;
    }

    delete ctx;
    return false;
}

void throw_invalid_op_exception(mc::string_view msg)
{
    MC_THROW(mc::invalid_op_exception, "invalid operation: {msg}", ("msg", msg));
}

} // namespace mc::memory::detail
