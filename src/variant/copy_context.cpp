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

#include <mc/variant/copy_context.h>

namespace mc::detail {

copy_context::~copy_context()
{
    clear();
}

void copy_context::clear()
{
    m_copied_objects.clear();
}

size_t copy_context::size() const
{
    return m_copied_objects.size();
}

bool copy_context::empty() const
{
    return m_copied_objects.empty();
}

bool copy_context::has_copied_impl(const void* ptr) const
{
    if (!ptr) {
        return false;
    }
    return m_copied_objects.count(ptr) > 0;
}

mc::memory::shared_base* copy_context::get_copied_impl(const void* ptr) const
{
    if (!ptr) {
        return nullptr;
    }

    auto it = m_copied_objects.find(ptr);
    if (it == m_copied_objects.end()) {
        return nullptr;
    }

    return it->second;
}

void copy_context::record_copied_impl(const void* original_ptr, mc::memory::shared_base* copied_base)
{
    if (!original_ptr || !copied_base) {
        return;
    }

    m_copied_objects[original_ptr] = copied_base;
}

} // namespace mc::detail
