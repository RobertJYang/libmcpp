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

#include "region_registry.h"

#include <algorithm>
#include <cstddef>
#include <shared_mutex>
#include <vector>
#include <mutex>

namespace mc::shm::detail {

namespace {

// 内部条目：按 user_base 排序
struct entry {
    std::byte*  user_base;
    std::size_t user_size;
};

struct registry_state {
    mutable std::shared_mutex lock;
    std::vector<entry>        entries; // 按 user_base 升序
};

// 使用 leaky singleton：首次访问时在堆上构造，永不析构。
// 原因：region_registry 被 shm_region 析构路径 unregister_region 调用，而
// shm_region 通常由 mc::singleton<shm_runtime> 持有。如果用 Meyer's singleton，
// registry_state 的构造发生在 singleton_manager 之后（lazy），反向析构顺序会
// 导致 registry_state 先于 shm_runtime 被销毁 —— shm_runtime 析构触发
// shm_region 析构再去 unregister_region，命中已析构的 shared_mutex，抛出
// system_error("mutex lock failed: Invalid argument") 并触发 terminate。
// registry_state 内存占用极小（一个 vector + 一个 mutex），泄漏可接受。
registry_state& state() noexcept
{
    static registry_state* s = new registry_state();
    return *s;
}

// 按 user_base 二分：返回首个 it->user_base > key 的位置
inline auto upper_bound_by_base(const std::vector<entry>& v, const std::byte* key) noexcept
{
    return std::upper_bound(v.begin(), v.end(), key,
                            [](const std::byte* k, const entry& e) noexcept {
                                return k < e.user_base;
                            });
}

} // namespace

void register_region(void* user_base, std::size_t user_size) noexcept
{
    if (user_base == nullptr || user_size == 0) {
        return;
    }

    auto* base  = static_cast<std::byte*>(user_base);
    auto& st    = state();
    auto  guard = std::unique_lock(st.lock);

    // 查是否已存在：若存在则覆盖 size（幂等注册）
    auto it = std::lower_bound(st.entries.begin(), st.entries.end(), base,
                               [](const entry& e, const std::byte* k) noexcept {
                                   return e.user_base < k;
                               });
    if (it != st.entries.end() && it->user_base == base) {
        it->user_size = user_size;
        return;
    }

    // upper_bound 得到首个 > base 的位置，直接 insert
    auto pos = upper_bound_by_base(st.entries, base);
    st.entries.insert(pos, entry{base, user_size});
}

void unregister_region(void* user_base) noexcept
{
    if (user_base == nullptr) {
        return;
    }

    auto* base  = static_cast<std::byte*>(user_base);
    auto& st    = state();
    auto  guard = std::unique_lock(st.lock);

    auto it = std::lower_bound(st.entries.begin(), st.entries.end(), base,
                               [](const entry& e, const std::byte* k) noexcept {
                                   return e.user_base < k;
                               });
    if (it != st.entries.end() && it->user_base == base) {
        st.entries.erase(it);
    }
}

std::optional<region_info> find_region_for(const void* ptr) noexcept
{
    if (ptr == nullptr) {
        return std::nullopt;
    }

    const auto* addr  = static_cast<const std::byte*>(ptr);
    auto&       st    = state();
    auto        guard = std::shared_lock(st.lock);

    auto it = upper_bound_by_base(st.entries, addr);
    if (it == st.entries.begin()) {
        return std::nullopt;
    }
    --it;
    if (addr >= it->user_base && addr < it->user_base + it->user_size) {
        return region_info{it->user_base, it->user_size};
    }
    return std::nullopt;
}

std::size_t registered_region_count() noexcept
{
    auto& st    = state();
    auto  guard = std::shared_lock(st.lock);
    return st.entries.size();
}

} // namespace mc::shm::detail
