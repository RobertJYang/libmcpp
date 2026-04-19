/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/shm/shm_runtime.h>

#include <cstring>
#include <mutex>

#include <mc/shm/detail/clock.h>
#include <mc/shm/detail/process_identity.h>
#include <mc/shm/ipc_mutex.h>
#include <mc/shm/ipc_shared_mutex.h>
#include <mc/shm/message_queue/mq_queue.h>

#include "message_queue/mq_notifier.h"

namespace mc::shm {
namespace {

constexpr const char  runtime_control_root_name[] = "__runtime_control";
constexpr std::size_t max_endpoint_name_size      = 64;
constexpr std::size_t max_queue_name_size         = 64;
constexpr std::size_t max_notifier_name_size      = 128;

template <std::size_t N>
mc::string read_c_string(const char (&value)[N])
{
    return mc::string(mc::string_view(value, ::strnlen(value, N)));
}

template <std::size_t N>
void write_c_string(char (&target)[N], mc::string_view value)
{
    std::memset(target, 0, sizeof(target));
    if (!value.empty()) {
        const auto count = std::min<std::size_t>(N - 1, value.size());
        std::memcpy(target, value.data(), count);
    }
}

struct endpoint_record {
    std::atomic<std::uint32_t> in_use{0};
    std::uint16_t              endpoint_id{0};
    std::uint16_t              reserved0{0};
    std::atomic<std::uint32_t> instance_id{0};
    std::atomic<std::uint32_t> state{0};
    std::atomic<std::int64_t>  owner_pid{0};
    std::atomic<std::uint64_t> heartbeat_deadline_ns{0};
    std::uint32_t              slot_count{0};
    char                       endpoint_name[max_endpoint_name_size]{};
    char                       queue_name[max_queue_name_size]{};
    char                       notifier_name[max_notifier_name_size]{};
};

enum class named_lock_kind : std::uint32_t {
    none         = 0,
    mutex        = 1,
    shared_mutex = 2,
};

struct named_lock_record {
    std::atomic<std::uint32_t> kind{0};
    std::uint32_t              reserved0{0};
    std::atomic<std::uint64_t> offset{0};
    char                       name[shm_runtime::max_named_lock_name]{};
};

struct runtime_control_block {
    ipc_mutex         lock{};
    std::uint64_t     heartbeat_timeout_ns{0};
    endpoint_record   endpoints[shm_runtime::max_endpoints]{};
    named_lock_record named_locks[shm_runtime::max_named_locks]{};
};

mc::string make_queue_segment_name(mc::string_view endpoint_name)
{
    return "mc_shm_queue_" + endpoint_name;
}

std::optional<std::size_t> find_endpoint_slot(runtime_control_block* control, mc::string_view endpoint_name)
{
    for (std::size_t i = 0; i < shm_runtime::max_endpoints; ++i) {
        if (control->endpoints[i].in_use.load(std::memory_order_acquire) == 0) {
            continue;
        }

        if (read_c_string(control->endpoints[i].endpoint_name) == endpoint_name) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> find_free_slot(runtime_control_block* control)
{
    for (std::size_t i = 0; i < shm_runtime::max_endpoints; ++i) {
        if (control->endpoints[i].in_use.load(std::memory_order_acquire) == 0) {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace

struct shm_runtime::impl {
    runtime_options        options;
    shm_region             region;
    runtime_control_block* control = nullptr;
    shm_allocator          arena;
    // 同进程内多线程串行化命名容器 / 命名锁工厂调用；
    // 跨进程互斥由 control->lock 提供。两道锁顺序固定：
    //   factory_mutex（本地）先，control->lock（跨进程）后。
    std::mutex             factory_mutex;
};

shm_runtime::shm_runtime(const runtime_options& options) : m_impl(std::make_shared<impl>())
{
    m_impl->options = options;

    shm_region_options region_options;
    region_options.segment_name  = options.region_name;
    region_options.total_size    = options.region_size;
    region_options.root_capacity = options.root_capacity;
    region_options.open_mode     = detail::open_mode::create_or_open;

    m_impl->region = shm_region(region_options);
    if (!m_impl->region.is_valid()) {
        m_impl.reset();
        return;
    }

    auto allocator = m_impl->region.user_arena();
    if (!allocator.is_valid()) {
        m_impl.reset();
        return;
    }

    auto control_root = m_impl->region.find_root(runtime_control_root_name);
    if (!control_root.has_value()) {
        auto* control = allocator.construct<runtime_control_block>();
        if (control == nullptr) {
            m_impl.reset();
            return;
        }

        control->heartbeat_timeout_ns =
            static_cast<std::uint64_t>(options.heartbeat_timeout.count()) * 1000ULL * 1000ULL;
        for (std::size_t i = 0; i < max_endpoints; ++i) {
            control->endpoints[i].endpoint_id = static_cast<std::uint16_t>(i + 1);
            control->endpoints[i].slot_count  = 256;
        }

        if (!m_impl->region.upsert_root(runtime_control_root_name, m_impl->region.offset_of(control), sizeof(*control),
                                        root_kind::runtime_control, 1)) {
            allocator.destroy(control);
            m_impl.reset();
            return;
        }
        control_root = m_impl->region.find_root(runtime_control_root_name);
    }

    if (!control_root.has_value()) {
        m_impl.reset();
        return;
    }

    m_impl->control = m_impl->region.ptr_from_offset<runtime_control_block>(control_root->offset);
    if (m_impl->control == nullptr) {
        m_impl.reset();
        return;
    }
    m_impl->arena = allocator;
}

shm_runtime::~shm_runtime() = default;

shm_runtime::shm_runtime(shm_runtime&& other) noexcept : m_impl(std::move(other.m_impl))
{}

shm_runtime& shm_runtime::operator=(shm_runtime&& other) noexcept
{
    if (this != &other) {
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

bool shm_runtime::is_valid() const noexcept
{
    return m_impl != nullptr;
}

bool shm_runtime::is_manager_process() const noexcept
{
    return is_valid() && m_impl->options.manager_process;
}

shm_region& shm_runtime::region() noexcept
{
    return m_impl->region;
}

const shm_region& shm_runtime::region() const noexcept
{
    return m_impl->region;
}

shm_allocator shm_runtime::user_arena() const noexcept
{
    return is_valid() ? m_impl->region.user_arena() : shm_allocator{};
}

std::optional<endpoint> shm_runtime::register_endpoint(mc::string_view endpoint_name, std::size_t slot_count)
{
    if (!is_valid() || endpoint_name.empty() || endpoint_name.size() >= max_endpoint_name_size) {
        return std::nullopt;
    }

    ipc_mutex_guard guard(m_impl->control->lock);

    auto slot = find_endpoint_slot(m_impl->control, endpoint_name);
    if (!slot.has_value()) {
        slot = find_free_slot(m_impl->control);
    }
    if (!slot.has_value()) {
        return std::nullopt;
    }

    auto& entry = m_impl->control->endpoints[*slot];
    entry.in_use.store(1, std::memory_order_release);
    const auto next_instance_id = std::max<std::uint32_t>(1, entry.instance_id.load(std::memory_order_acquire) + 1);
    entry.instance_id.store(next_instance_id, std::memory_order_release);
    entry.state.store(static_cast<std::uint32_t>(endpoint_state::starting), std::memory_order_release);
    entry.owner_pid.store(static_cast<std::int64_t>(detail::current_process_pid()), std::memory_order_release);
    entry.heartbeat_deadline_ns.store(detail::monotonic_now_ns() + m_impl->control->heartbeat_timeout_ns,
                                      std::memory_order_release);
    entry.slot_count = static_cast<std::uint32_t>(std::max<std::size_t>(1, slot_count));

    write_c_string(entry.endpoint_name, endpoint_name);
    if (entry.queue_name[0] == '\0') {
        const auto queue_name = make_queue_segment_name(endpoint_name);
        write_c_string(entry.queue_name, queue_name);
    }
    if (entry.notifier_name[0] == '\0') {
        const auto notifier_name = detail::mq_notifier::make_default_name(entry.queue_name);
        write_c_string(entry.notifier_name, notifier_name);
    }

    endpoint result;
    result.endpoint_id   = entry.endpoint_id;
    result.instance_id   = next_instance_id;
    result.state         = endpoint_state::starting;
    result.slot_count    = entry.slot_count;
    result.endpoint_name = read_c_string(entry.endpoint_name);
    result.queue_name    = read_c_string(entry.queue_name);
    result.notifier_name = read_c_string(entry.notifier_name);
    return result;
}

bool shm_runtime::mark_endpoint_running(const endpoint& endpoint)
{
    if (!is_valid() || !endpoint.is_valid()) {
        return false;
    }

    ipc_mutex_guard guard(m_impl->control->lock);
    auto&           entry = m_impl->control->endpoints[endpoint.endpoint_id - 1];
    if (entry.instance_id.load(std::memory_order_acquire) != endpoint.instance_id) {
        return false;
    }

    entry.state.store(static_cast<std::uint32_t>(endpoint_state::running), std::memory_order_release);
    entry.heartbeat_deadline_ns.store(detail::monotonic_now_ns() + m_impl->control->heartbeat_timeout_ns,
                                      std::memory_order_release);
    return true;
}

bool shm_runtime::heartbeat(const endpoint& endpoint)
{
    if (!is_valid() || !endpoint.is_valid()) {
        return false;
    }

    ipc_mutex_guard guard(m_impl->control->lock);
    auto&           entry = m_impl->control->endpoints[endpoint.endpoint_id - 1];
    if (entry.instance_id.load(std::memory_order_acquire) != endpoint.instance_id) {
        return false;
    }

    entry.heartbeat_deadline_ns.store(detail::monotonic_now_ns() + m_impl->control->heartbeat_timeout_ns,
                                      std::memory_order_release);
    return true;
}

bool shm_runtime::abort_endpoint(const endpoint& endpoint)
{
    if (!is_valid() || !endpoint.is_valid()) {
        return false;
    }

    ipc_mutex_guard guard(m_impl->control->lock);
    auto&           entry = m_impl->control->endpoints[endpoint.endpoint_id - 1];
    if (entry.instance_id.load(std::memory_order_acquire) != endpoint.instance_id) {
        return false;
    }

    entry.state.store(static_cast<std::uint32_t>(endpoint_state::aborting), std::memory_order_release);
    return true;
}

std::size_t shm_runtime::recover_stale_endpoints()
{
    if (!is_valid()) {
        return 0;
    }

    std::size_t     recovered = 0;
    const auto      now       = detail::monotonic_now_ns();
    ipc_mutex_guard guard(m_impl->control->lock);
    for (auto& entry : m_impl->control->endpoints) {
        if (entry.in_use.load(std::memory_order_acquire) == 0) {
            continue;
        }

        const auto state = static_cast<endpoint_state>(entry.state.load(std::memory_order_acquire));
        if (state != endpoint_state::starting && state != endpoint_state::running) {
            continue;
        }

        const auto deadline  = entry.heartbeat_deadline_ns.load(std::memory_order_acquire);
        const auto owner_pid = static_cast<std::uint32_t>(entry.owner_pid.load(std::memory_order_acquire));
        if (deadline < now || !ipc_mutex::is_process_alive(owner_pid)) {
            entry.state.store(static_cast<std::uint32_t>(endpoint_state::aborting), std::memory_order_release);
            ++recovered;
        }
    }
    return recovered;
}

std::optional<endpoint_info> shm_runtime::get_endpoint(std::uint16_t endpoint_id) const
{
    if (!is_valid() || endpoint_id == 0 || endpoint_id > max_endpoints) {
        return std::nullopt;
    }

    ipc_mutex_guard guard(m_impl->control->lock);
    const auto&     entry = m_impl->control->endpoints[endpoint_id - 1];
    if (entry.in_use.load(std::memory_order_acquire) == 0) {
        return std::nullopt;
    }

    endpoint_info snapshot;
    snapshot.endpoint_id           = entry.endpoint_id;
    snapshot.instance_id           = entry.instance_id.load(std::memory_order_acquire);
    snapshot.state                 = static_cast<endpoint_state>(entry.state.load(std::memory_order_acquire));
    snapshot.slot_count            = entry.slot_count;
    snapshot.endpoint_name         = read_c_string(entry.endpoint_name);
    snapshot.queue_name            = read_c_string(entry.queue_name);
    snapshot.notifier_name         = read_c_string(entry.notifier_name);
    snapshot.owner_pid             = entry.owner_pid.load(std::memory_order_acquire);
    snapshot.heartbeat_deadline_ns = entry.heartbeat_deadline_ns.load(std::memory_order_acquire);
    return snapshot;
}

bool shm_runtime::writer_instance_is_current(std::uint16_t endpoint_id, std::uint64_t instance_id) const
{
    const auto snapshot = get_endpoint(endpoint_id);
    if (!snapshot.has_value()) {
        return false;
    }

    return snapshot->instance_id == instance_id &&
           (snapshot->state == endpoint_state::starting || snapshot->state == endpoint_state::running);
}

mq_queue shm_runtime::open_queue(const endpoint& endpoint) const
{
    mq_queue_options options;
    options.shared_memory_name = endpoint.queue_name;
    options.slot_count         = endpoint.slot_count == 0 ? 256 : endpoint.slot_count;
    return mq_queue(options);
}

namespace {

bool named_lock_name_is_valid(mc::string_view name) noexcept
{
    return !name.empty() && name.size() < shm_runtime::max_named_lock_name;
}

std::pair<std::size_t, named_lock_kind> find_named_lock_slot(const runtime_control_block* control,
                                                             mc::string_view              name)
{
    for (std::size_t i = 0; i < shm_runtime::max_named_locks; ++i) {
        const auto kind =
            static_cast<named_lock_kind>(control->named_locks[i].kind.load(std::memory_order_acquire));
        if (kind == named_lock_kind::none) {
            continue;
        }
        if (read_c_string(control->named_locks[i].name) == name) {
            return {i, kind};
        }
    }
    return {shm_runtime::max_named_locks, named_lock_kind::none};
}

std::size_t find_free_named_lock_slot(const runtime_control_block* control)
{
    for (std::size_t i = 0; i < shm_runtime::max_named_locks; ++i) {
        if (control->named_locks[i].kind.load(std::memory_order_acquire) ==
            static_cast<std::uint32_t>(named_lock_kind::none)) {
            return i;
        }
    }
    return shm_runtime::max_named_locks;
}

template <typename LockType>
LockType* resolve_named_lock(shm_region& region, runtime_control_block* control, mc::string_view name,
                             named_lock_kind expected_kind)
{
    if (!named_lock_name_is_valid(name)) {
        return nullptr;
    }

    ipc_mutex_guard guard(control->lock);

    const auto [slot, existing_kind] = find_named_lock_slot(control, name);
    if (slot != shm_runtime::max_named_locks) {
        if (existing_kind != expected_kind) {
            return nullptr;
        }
        return region.template ptr_from_offset<LockType>(
            control->named_locks[slot].offset.load(std::memory_order_acquire));
    }

    const auto free_slot = find_free_named_lock_slot(control);
    if (free_slot == shm_runtime::max_named_locks) {
        return nullptr;
    }

    auto  allocator = region.user_arena();
    auto* lock      = allocator.template construct<LockType>();
    if (lock == nullptr) {
        return nullptr;
    }

    auto& record = control->named_locks[free_slot];
    write_c_string(record.name, name);
    record.offset.store(region.offset_of(lock), std::memory_order_release);
    record.kind.store(static_cast<std::uint32_t>(expected_kind), std::memory_order_release);
    return lock;
}

} // namespace

ipc_mutex* shm_runtime::get_or_create_mutex(mc::string_view name)
{
    if (!is_valid()) {
        return nullptr;
    }
    return resolve_named_lock<ipc_mutex>(m_impl->region, m_impl->control, name, named_lock_kind::mutex);
}

ipc_shared_mutex* shm_runtime::get_or_create_shared_mutex(mc::string_view name)
{
    if (!is_valid()) {
        return nullptr;
    }
    return resolve_named_lock<ipc_shared_mutex>(m_impl->region, m_impl->control, name, named_lock_kind::shared_mutex);
}

// ==========================================================================
// 命名容器工厂
// ==========================================================================
namespace {

// root_table 中命名容器的名字前缀（8 字节，固定宽度）
// 形式："__mcC_" + kind_code + '_'，共 8 字节
constexpr std::size_t container_root_prefix_len = 8;

// 构造 root_table 使用的完整名字：prefix + user_name
// 失败返回空 string_view（长度溢出或 user_name 非法）
struct full_name_buf {
    char        storage[shm_region::max_root_name_size]{};
    std::size_t length = 0;
    bool        valid  = false;
};

full_name_buf make_full_name(char kind_code, mc::string_view user_name) noexcept
{
    full_name_buf buf;
    if (user_name.empty()) {
        return buf;
    }
    if (user_name.size() + container_root_prefix_len >= shm_region::max_root_name_size) {
        return buf;
    }

    buf.storage[0] = '_';
    buf.storage[1] = '_';
    buf.storage[2] = 'm';
    buf.storage[3] = 'c';
    buf.storage[4] = 'C';
    buf.storage[5] = '_';
    buf.storage[6] = kind_code;
    buf.storage[7] = '_';
    std::memcpy(buf.storage + container_root_prefix_len, user_name.data(), user_name.size());
    buf.length = container_root_prefix_len + user_name.size();
    buf.valid  = true;
    return buf;
}

} // namespace

std::uint64_t shm_runtime::make_container_signature(char kind_code, std::uint32_t key_size,
                                                    std::uint32_t value_size,
                                                    std::uint32_t key_align,
                                                    std::uint32_t value_align) noexcept
{
    // 64 位布局：
    //   [63:56] kind_code（ASCII，'L'/'S'/'M'）
    //   [55:48] key_align（字节）
    //   [47:40] value_align（字节；list 为 0）
    //   [39:20] key_size   20bit
    //   [19:0]  value_size 20bit（list 为 0）
    const std::uint64_t k  = static_cast<std::uint64_t>(kind_code) & 0xFFu;
    const std::uint64_t ka = static_cast<std::uint64_t>(key_align) & 0xFFu;
    const std::uint64_t va = static_cast<std::uint64_t>(value_align) & 0xFFu;
    const std::uint64_t ks = static_cast<std::uint64_t>(key_size) & 0xFFFFFu;
    const std::uint64_t vs = static_cast<std::uint64_t>(value_size) & 0xFFFFFu;
    return (k << 56) | (ka << 48) | (va << 40) | (ks << 20) | vs;
}

shm_runtime::named_container_handle
shm_runtime::ensure_named_container(const named_container_spec& spec) noexcept
{
    named_container_handle empty;
    if (!is_valid() || spec.init_fn == nullptr || spec.ctrl_size == 0 || spec.ctrl_align == 0) {
        return empty;
    }

    const auto full = make_full_name(spec.kind_code, spec.name);
    if (!full.valid) {
        return empty;
    }
    const mc::string_view full_name(full.storage, full.length);

    // 先本地锁（同进程线程互斥），再跨进程锁。
    std::lock_guard<std::mutex> local_guard(m_impl->factory_mutex);
    ipc_mutex_guard             cross_guard(m_impl->control->lock);

    if (auto existing = m_impl->region.find_root(full_name); existing.has_value()) {
        if (existing->size != spec.ctrl_size || existing->generation != spec.signature) {
            return empty;  // 同名不同类型 → 类型冲突
        }
        named_container_handle handle;
        handle.control   = m_impl->region.address_from_offset(existing->offset);
        handle.allocator = handle.control != nullptr ? &m_impl->arena : nullptr;
        if (handle.allocator == nullptr) {
            return empty;
        }
        return handle;
    }

    void* mem = m_impl->arena.allocate(spec.ctrl_size, spec.ctrl_align);
    if (mem == nullptr) {
        return empty;
    }
    std::memset(mem, 0, spec.ctrl_size);

    const std::uint64_t ctrl_offset = m_impl->region.offset_of(mem);
    const auto          self_tag    = static_cast<std::uint32_t>(ctrl_offset);
    spec.init_fn(mem, self_tag);

    if (!m_impl->region.upsert_root(full_name, ctrl_offset, spec.ctrl_size, root_kind::opaque,
                                    spec.signature)) {
        // 登记失败（root_table 满）；释放控制块避免泄漏
        m_impl->arena.deallocate(mem);
        return empty;
    }

    named_container_handle handle;
    handle.control   = mem;
    handle.allocator = &m_impl->arena;
    return handle;
}

bool shm_runtime::drop_named_container_impl(char kind_code, mc::string_view name) noexcept
{
    if (!is_valid()) {
        return false;
    }

    const auto full = make_full_name(kind_code, name);
    if (!full.valid) {
        return false;
    }
    const mc::string_view full_name(full.storage, full.length);

    std::lock_guard<std::mutex> local_guard(m_impl->factory_mutex);
    ipc_mutex_guard             cross_guard(m_impl->control->lock);

    auto existing = m_impl->region.find_root(full_name);
    if (!existing.has_value()) {
        return false;
    }

    void* ctrl = m_impl->region.address_from_offset(existing->offset);
    if (ctrl == nullptr) {
        return false;
    }

    // drop 要求容器已清空（无法在不知类型的情况下安全析构元素）。
    // 按 kind 校验 size == 0 + head 为 0；否则拒绝 drop，避免泄漏二级分配。
    bool empty = false;
    switch (kind_code) {
    case 'L': {
        auto* lc = static_cast<container::list_control*>(ctrl);
        empty    = lc->size.load(std::memory_order_acquire) == 0 && lc->head_offset == 0;
        break;
    }
    case 'S': {
        auto* sc = static_cast<container::set_control*>(ctrl);
        empty    = sc->size.load(std::memory_order_acquire) == 0 && sc->head_forward[0] == 0;
        break;
    }
    case 'M': {
        auto* mc_ctrl = static_cast<container::map_control*>(ctrl);
        empty = mc_ctrl->size.load(std::memory_order_acquire) == 0 && mc_ctrl->head_forward[0] == 0;
        break;
    }
    default:
        return false;
    }

    if (!empty) {
        return false;
    }

    m_impl->arena.deallocate(ctrl);
    return m_impl->region.erase_root(full_name);
}

bool shm_runtime::drop_named_container(mc::string_view name) noexcept
{
    if (!is_valid() || name.empty()) {
        return false;
    }
    // 调用方不提供 kind_code，依次按 list/set/map 查找；找到即 drop。
    // 同名不同 kind 的容器逻辑上属于不同命名空间，只会命中一个。
    for (char kind : {'L', 'S', 'M'}) {
        const auto full = make_full_name(kind, name);
        if (!full.valid) continue;
        const mc::string_view full_name(full.storage, full.length);

        if (m_impl->region.find_root(full_name).has_value()) {
            return drop_named_container_impl(kind, name);
        }
    }
    return false;
}

} // namespace mc::shm
