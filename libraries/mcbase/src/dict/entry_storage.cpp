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

#include <dict/include/entry_storage.h>

#include <mc/dict/entry.h>
#include <mc/memory/object_pool.h>

namespace mc::dict_types {
namespace {

mc::memory::object_pool<entry>& global_entry_storage_pool()
{
    static mc::memory::object_pool<entry> s_pool([] {
        mc::memory::object_pool<entry>::options opts;
        opts.initial_slab_slots   = 256;
        opts.max_slab_slots       = 4096;
        opts.local_cache_capacity = 256;
        return opts;
    }());
    return s_pool;
}

} // namespace

entry* entry_storage_create(mc::variant key, mc::variant value)
{
    return global_entry_storage_pool().create(std::move(key), std::move(value));
}

void entry_storage_destroy(entry* item) noexcept
{
    global_entry_storage_pool().destroy(item);
}

void entry_storage_reserve(std::size_t slot_count)
{
    global_entry_storage_pool().reserve(slot_count);
}

} // namespace mc::dict_types
