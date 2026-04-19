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

#ifndef MC_DATABASE_SHM_STORAGE_ENGINE_H
#define MC_DATABASE_SHM_STORAGE_ENGINE_H

#include <mc/common.h>
#include <mc/db/common.h>
#include <mc/db/storage_engine.h>
#include <mc/exception.h>
#include <mc/intrusive/offset_ptr.h>
#include <mc/memory.h>
#include <mc/object_base.h>
#include <mc/shm/byte_string.h>
#include <mc/shm/container/map.h>
#include <mc/shm/shm_runtime.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace mc::db {

// ============================================================================
// shm_storage_engine（SHM 存储引擎实现）
//
// 每个 index 由一份 shm::map<byte_string, offset_ptr<ShmRecord>, byte_string_less>
// 支撑，把"持久化数据本体"（ShmRecord，例如 mc::engine::shm_object POD）放进 SHM；
// shm map value 是 offset_ptr<ShmRecord>，跨进程可读、crash-safe。
//
// heap pool 仍是进程内的对象生命周期持有者：
//   unordered_map<ShmRecord*, shared_ptr<Object>>
//   key 用 ShmRecord* 而不是 object_id：
//     - shm 中 ShmRecord 由 SHM allocator 持有，地址稳定；
//     - read 路径直接 map -> offset_ptr<ShmRecord> -> ShmRecord* -> heap_pool；
//     - 不需要再注入 id_extractor。
//
// 三个外部注入点（mcbase 不知道业务 POD 的字段语义）：
//   1) shm_handle_extractor_fn(const Object&) -> ShmRecord*
//      add() / insert() 时从 Object 中抽出已绑定的 ShmRecord*；
//      mcengine 实现：return obj.get_shm_handle()。未注入时 insert 抛异常。
//   2) reconstruct_fn(ShmRecord*) -> shared_ptr<Object>
//      recover() 在重启 / takeover 时用此回调重建 heap pool；
//      mcengine 实现：从 ShmRecord 读 class_name → class_factory → 构造 + 回填属性。
//      未注入时 recover() 退化为只清空 heap pool。
//   3) （ShmRecord 物理生命周期由 mcengine 管理，本引擎不 destroy ShmRecord；
//        mcengine 在 service 销毁路径上自行调用 shm_object_destroy）
//
// commit/rollback：shm::map 自带 container_journal 崩溃安全，每次
// try_emplace/erase 自身已是一次原子事务，引擎层 commit/rollback 是 no-op。
//
// 线程模型：
//   - shm::map 内部 container_guard 跨进程互斥
//   - heap pool 用 std::mutex 保护同进程多线程
// ============================================================================

// ----------------------------------------------------------------------------
// shm_engine_alloc：上下文载体型 allocator
//   - value_type=char、std::allocator-like
//   - 携带 shm_runtime* + base name；不承担实际堆分配（::operator new）
//   - 默认构造为 unbound（runtime=nullptr），首次 I/O 时抛 invalid_arg_exception
//   - 通过 derive_per_index_alloc 重载点为每个 index 派生 ".idx<N>" 后缀
// ----------------------------------------------------------------------------

struct shm_engine_alloc {
    using value_type = char;

    shm_engine_alloc() noexcept = default;

    shm_engine_alloc(mc::shm::shm_runtime& rt, std::string_view base) noexcept
        : runtime(&rt), name(base)
    {
    }

    template <typename U>
    shm_engine_alloc(const std::allocator<U>&) noexcept
    {
    }

    value_type* allocate(std::size_t n) { return static_cast<value_type*>(::operator new(n)); }

    void deallocate(value_type* p, std::size_t) noexcept { ::operator delete(p); }

    bool operator==(const shm_engine_alloc& o) const noexcept
    {
        return runtime == o.runtime && name == o.name;
    }
    bool operator!=(const shm_engine_alloc& o) const noexcept { return !(*this == o); }

    mc::shm::shm_runtime* runtime = nullptr;
    std::string           name;
};

// derive_per_index_alloc 重载：把 base name 追加 ".idx<N>"
inline shm_engine_alloc derive_per_index_alloc(const shm_engine_alloc& alloc, std::size_t index_id,
                                               mc::string_view table_name)
{
    if (alloc.runtime == nullptr) {
        return alloc;
    }
    shm_engine_alloc out;
    out.runtime = alloc.runtime;
    std::string base =
        !alloc.name.empty() ? alloc.name : std::string(table_name.data(), table_name.size());
    if (base.empty()) {
        base = "shm_engine";
    }
    out.name = base + ".idx" + std::to_string(index_id);
    return out;
}

// ============================================================================
// shm_storage_engine 主体
// ============================================================================

template <typename Object, typename ShmRecord, std::size_t IndexCount,
          typename Allocator = shm_engine_alloc>
class shm_storage_engine {
    static_assert(IndexCount >= 1U, "shm_storage_engine 至少需要 1 个 index（object_id）");
    static_assert(std::is_base_of_v<mc::object_base, Object>,
                  "shm_storage_engine<Object>: Object 必须继承 mc::object_base");

public:
    using object_type        = Object;
    using object_ptr_type    = mc::shared_ptr<object_type>;
    using leaf_type          = std::optional<object_ptr_type>;
    using allocator_type     = Allocator;
    using id_type            = mc::object_id_type;
    using shm_record_type    = ShmRecord;
    using shm_record_ptr     = mc::intrusive::offset_ptr<ShmRecord>;
    using map_type =
        mc::shm::container::map<mc::shm::byte_string, shm_record_ptr, mc::shm::byte_string_less>;
    using shm_handle_extractor_fn = std::function<ShmRecord*(const Object&)>;
    using reconstruct_fn          = std::function<object_ptr_type(ShmRecord*)>;

    class raw_iterator;

    static constexpr std::size_t index_count = IndexCount;

    explicit shm_storage_engine(const allocator_type& alloc      = allocator_type(),
                                mc::string_view       table_name = mc::string_view())
        : m_alloc(alloc), m_table_name(table_name.data(), table_name.size())
    {
        if (m_alloc.runtime != nullptr) {
            _bind_all();
        }
    }

    ~shm_storage_engine()                                    = default;
    shm_storage_engine(const shm_storage_engine&)            = delete;
    shm_storage_engine& operator=(const shm_storage_engine&) = delete;
    shm_storage_engine(shm_storage_engine&&)                 = delete;
    shm_storage_engine& operator=(shm_storage_engine&&)      = delete;

    // 注入点：从 Object 中抽出已绑定的 ShmRecord*
    void set_shm_handle_extractor(shm_handle_extractor_fn fn) noexcept
    {
        m_handle_extractor = std::move(fn);
    }
    // 注入点：recover 时从 ShmRecord 重建 shared_ptr<Object>
    void set_reconstruct(reconstruct_fn fn) noexcept { m_reconstruct = std::move(fn); }

    // ----- per-index 写 -----
    std::pair<leaf_type, bool> insert(std::size_t idx, mc::string_view key, object_ptr_type v)
    {
        auto& m = _at(idx);
        if (!v) {
            return {std::nullopt, false};
        }
        ShmRecord* sh = _extract_handle(*v);
        if (sh == nullptr) {
            MC_THROW(mc::invalid_arg_exception,
                     "shm_storage_engine.insert: object 未绑定 ShmRecord（m_shm_handle=null），"
                     "请先调用 shm_object_create 并 set_shm_handle 后再 add 进表");
        }

        if (auto existing = m.find(key); existing) {
            ShmRecord* old_sh = existing.value->get();
            leaf_type  old_leaf;
            {
                std::lock_guard lk(m_pool_mutex);
                if (auto pit = m_pool.find(old_sh); pit != m_pool.end()) {
                    old_leaf = pit->second;
                }
            }
            if (!m.erase(key)) {
                return {std::nullopt, false};
            }
            if (idx == 0U) {
                std::lock_guard lk(m_pool_mutex);
                m_pool.erase(old_sh);
            }
            auto& arena = *m.allocator();
            auto  bs    = mc::shm::byte_string::create(arena, key);
            if (bs.size() == 0 && !key.empty()) {
                return {old_leaf, true};
            }
            auto [mp, ok] = m.try_emplace(std::move(bs), shm_record_ptr(sh));
            if (ok && idx == 0U) {
                std::lock_guard lk(m_pool_mutex);
                m_pool[sh] = std::move(v);
            }
            return {old_leaf, true};
        }

        auto& arena = *m.allocator();
        auto  bs    = mc::shm::byte_string::create(arena, key);
        if (bs.size() == 0 && !key.empty()) {
            return {std::nullopt, false};
        }
        auto [mp, inserted] = m.try_emplace(std::move(bs), shm_record_ptr(sh));
        if (inserted && idx == 0U) {
            std::lock_guard lk(m_pool_mutex);
            m_pool[sh] = std::move(v);
        }
        return {std::nullopt, false};
    }

    leaf_type remove(std::size_t idx, mc::string_view key)
    {
        auto& m  = _at(idx);
        auto  mp = m.find(key);
        if (!mp) {
            return std::nullopt;
        }
        ShmRecord* sh = mp.value->get();
        if (!m.erase(key)) {
            return std::nullopt;
        }
        if (idx != 0U) {
            std::lock_guard lk(m_pool_mutex);
            if (auto pit = m_pool.find(sh); pit != m_pool.end()) {
                return pit->second;
            }
            return object_ptr_type{};
        }
        std::lock_guard lk(m_pool_mutex);
        auto            pit = m_pool.find(sh);
        if (pit == m_pool.end()) {
            return object_ptr_type{};
        }
        object_ptr_type old = std::move(pit->second);
        m_pool.erase(pit);
        return old;
    }

    void clear(std::size_t idx)
    {
        auto& m = _at(idx);
        m.clear();
        if (idx == 0U) {
            std::lock_guard lk(m_pool_mutex);
            m_pool.clear();
        }
    }

    // ----- per-index 读 -----
    leaf_type get(std::size_t idx, mc::string_view key) const
    {
        const auto& m  = _at(idx);
        auto        mp = m.find(key);
        if (!mp) {
            return std::nullopt;
        }
        ShmRecord*      sh = mp.value->get();
        std::lock_guard lk(m_pool_mutex);
        if (auto pit = m_pool.find(sh); pit != m_pool.end()) {
            return pit->second;
        }
        return object_ptr_type{};
    }

    raw_iterator find(std::size_t idx, mc::string_view key) const
    {
        const auto& m  = _at(idx);
        auto        mp = m.find(key);
        if (!mp) {
            return raw_iterator{};
        }
        return raw_iterator(this, idx, m.lower_bound(key));
    }

    raw_iterator lower_bound(std::size_t idx, mc::string_view key) const
    {
        return raw_iterator(this, idx, _at(idx).lower_bound(key));
    }

    raw_iterator upper_bound(std::size_t idx, mc::string_view key) const
    {
        return raw_iterator(this, idx, _at(idx).upper_bound(key));
    }

    raw_iterator begin(std::size_t idx) const { return raw_iterator(this, idx, _at(idx).cbegin()); }

    raw_iterator end(std::size_t /*idx*/) const { return raw_iterator{}; }

    bool empty(std::size_t idx) const { return _at(idx).empty(); }

    std::size_t size(std::size_t idx) const { return _at(idx).size(); }

    // ----- engine-wide -----
    void clear()
    {
        for (std::size_t i = 0; i < IndexCount; ++i) {
            m_maps[i]->clear();
        }
        std::lock_guard lk(m_pool_mutex);
        m_pool.clear();
    }

    // shm::map 自身已是 journal 安全的原子事务，引擎层事务 API 全部 no-op
    void commit() noexcept {}
    void rollback() noexcept {}
    int  save_point() noexcept { return 0; }
    int  current_save_point() const noexcept { return -1; }
    void rollback_to(int /*save_point_id*/) noexcept {}
    void lock_db() noexcept {}
    void unlock_db() noexcept {}

    // recover：遍历主索引（idx 0），通过 reconstruct_fn 把 ShmRecord 物化为 shared_ptr<Object>
    // 没注入 reconstruct_fn 时退化为清空 heap pool（调用方需对空 leaf 做兼容）
    void recover()
    {
        std::lock_guard lk(m_pool_mutex);
        m_pool.clear();
        if (!m_maps[0]) {
            return;
        }
        if (!m_reconstruct) {
            return;
        }
        for (auto it = m_maps[0]->cbegin(); it != m_maps[0]->cend(); ++it) {
            auto kv = *it;
            if (kv.value == nullptr) {
                continue;
            }
            ShmRecord* sh = kv.value->get();
            if (sh == nullptr) {
                continue;
            }
            object_ptr_type p = m_reconstruct(sh);
            if (p) {
                m_pool.emplace(sh, std::move(p));
            }
        }
    }

    // ----- 诊断 -----
    map_type&             index_map(std::size_t idx) { return _at(idx); }
    const map_type&       index_map(std::size_t idx) const { return _at(idx); }
    const allocator_type& get_allocator() const noexcept { return m_alloc; }
    mc::string_view       table_name() const noexcept { return m_table_name; }

    // 按 ShmRecord* 注入 heap pool（手动重建场景，例如 mcengine recover 钩子）
    void heap_pool_emplace(ShmRecord* sh, object_ptr_type obj)
    {
        if (sh == nullptr || !obj) {
            return;
        }
        std::lock_guard lk(m_pool_mutex);
        m_pool[sh] = std::move(obj);
    }

private:
    friend class raw_iterator;

    ShmRecord* _extract_handle(const Object& obj) const
    {
        if (m_handle_extractor) {
            return m_handle_extractor(obj);
        }
        return nullptr;
    }

    void _bind_all()
    {
        for (std::size_t i = 0; i < IndexCount; ++i) {
            shm_engine_alloc per = derive_per_index_alloc(m_alloc, i, m_table_name);
            auto             opt =
                per.runtime->get_or_create_map<mc::shm::byte_string, shm_record_ptr,
                                               mc::shm::byte_string_less>(per.name);
            if (!opt) {
                MC_THROW(mc::invalid_arg_exception,
                         "shm_storage_engine: get_or_create_map 失败 name=${n}", ("n", per.name));
            }
            m_maps[i] = std::make_unique<map_type>(std::move(*opt));
        }
    }

    map_type& _at(std::size_t idx) const
    {
        if (idx >= IndexCount) {
            MC_THROW(mc::invalid_arg_exception,
                     "shm_storage_engine: index_id 越界 idx=${idx}, IndexCount=${n}", ("idx", idx)(
                                                                                          "n",
                                                                                          IndexCount));
        }
        if (!m_maps[idx]) {
            MC_THROW(mc::invalid_arg_exception,
                     "shm_storage_engine: 未绑定 shm_runtime（默认构造 allocator）");
        }
        return *m_maps[idx];
    }

    allocator_type                                          m_alloc;
    std::string                                             m_table_name;
    std::array<std::unique_ptr<map_type>, IndexCount>       m_maps{};
    mutable std::mutex                                      m_pool_mutex;
    mutable std::unordered_map<ShmRecord*, object_ptr_type> m_pool;
    shm_handle_extractor_fn                                 m_handle_extractor;
    reconstruct_fn                                          m_reconstruct;
};

// ============================================================================
// shm_storage_engine::raw_iterator
//
// 适配 shm::map::const_iterator -> 满足 db::iterator 的 raw_iterator 契约：
//   - 默认构造 == end
//   - operator++ / == / !=
//   - key() -> string_view
//   - operator->()->{first: string_view, second: const shared_ptr<Object>&}
//   - to_next_prefix(prefix)：跳到第一个不以 prefix 为前缀的位置
// ============================================================================

template <typename Object, typename ShmRecord, std::size_t IndexCount, typename Allocator>
class shm_storage_engine<Object, ShmRecord, IndexCount, Allocator>::raw_iterator {
public:
    using engine_type = shm_storage_engine<Object, ShmRecord, IndexCount, Allocator>;
    using shm_cit     = typename engine_type::map_type::const_iterator;

    struct proxy {
        std::string_view       first;
        const object_ptr_type& second;
    };

    raw_iterator() noexcept = default;

    raw_iterator(const engine_type* eng, std::size_t idx, shm_cit it) noexcept
        : m_engine(eng), m_index(idx), m_it(std::move(it))
    {
        // isolated（reconstruct 失败）记录留在 SHM 索引中，但 heap pool
        // 里没有对应 shared_ptr。迭代器对外暴露的语义是"遍历活对象"，构造时立刻
        // 跳过 leading isolated 项，避免 caller 解引用空 shared_ptr 触发 SIGSEGV。
        _skip_isolated();
    }

    raw_iterator& operator++() noexcept
    {
        ++m_it;
        _skip_isolated();
        return *this;
    }

    raw_iterator operator++(int) noexcept
    {
        auto t = *this;
        ++*this;
        return t;
    }

    bool operator==(const raw_iterator& o) const noexcept { return m_it == o.m_it; }
    bool operator!=(const raw_iterator& o) const noexcept { return !(*this == o); }

    bool is_end() const noexcept { return m_it == shm_cit{}; }

    std::string_view key() const noexcept
    {
        if (is_end()) {
            return {};
        }
        auto mp = *m_it;
        return mp.key != nullptr ? mp.key->as_string_view() : std::string_view{};
    }

    proxy operator*() const noexcept { return _make_proxy(); }

    class arrow_holder {
    public:
        explicit arrow_holder(proxy p) : m_proxy(p) {}
        proxy* operator->() noexcept { return &m_proxy; }

    private:
        proxy m_proxy;
    };

    arrow_holder operator->() const noexcept { return arrow_holder(_make_proxy()); }

    // to_next_prefix：用 byte-wise binary increment + lower_bound 跳到下一个非前缀
    void to_next_prefix(std::string_view prefix) noexcept
    {
        if (is_end() || m_engine == nullptr) {
            return;
        }
        std::string_view cur = key();
        if (cur.size() < prefix.size() || std::string_view(cur.data(), prefix.size()) != prefix) {
            return;
        }
        std::string next(prefix);
        while (!next.empty() && static_cast<unsigned char>(next.back()) == 0xFFU) {
            next.pop_back();
        }
        if (next.empty()) {
            *this = raw_iterator{};
            return;
        }
        next.back() = static_cast<char>(static_cast<unsigned char>(next.back()) + 1U);
        *this       = m_engine->lower_bound(m_index, std::string_view(next));
    }

private:
    proxy _make_proxy() const noexcept
    {
        if (is_end()) {
            return {std::string_view{}, _empty_ref()};
        }
        auto mp = *m_it;
        if (mp.key == nullptr || mp.value == nullptr) {
            return {std::string_view{}, _empty_ref()};
        }
        ShmRecord* sh = mp.value->get();
        if (sh == nullptr) {
            return {mp.key->as_string_view(), _empty_ref()};
        }
        std::lock_guard lk(m_engine->m_pool_mutex);
        auto            pit = m_engine->m_pool.find(sh);
        const object_ptr_type& ref =
            pit != m_engine->m_pool.end() ? pit->second : _empty_ref();
        return {mp.key->as_string_view(), ref};
    }

    static const object_ptr_type& _empty_ref() noexcept
    {
        static const object_ptr_type e{};
        return e;
    }

    // 把 m_it 推进到第一个"在 heap pool 中存在 shared_ptr"的位置；
    // 如果遇到 end 就停。isolated 条目（heap pool 缺席）被静默跳过。
    void _skip_isolated() noexcept
    {
        if (m_engine == nullptr) {
            return;
        }
        const shm_cit end_sentinel{};
        while (m_it != end_sentinel) {
            auto mp = *m_it;
            if (mp.key == nullptr || mp.value == nullptr) {
                ++m_it;
                continue;
            }
            ShmRecord* sh = mp.value->get();
            if (sh == nullptr) {
                ++m_it;
                continue;
            }
            std::lock_guard lk(m_engine->m_pool_mutex);
            if (m_engine->m_pool.find(sh) != m_engine->m_pool.end()) {
                return;
            }
            ++m_it;
        }
    }

    const engine_type* m_engine = nullptr;
    std::size_t        m_index  = 0;
    shm_cit            m_it{};
};

} // namespace mc::db

#endif // MC_DATABASE_SHM_STORAGE_ENGINE_H
