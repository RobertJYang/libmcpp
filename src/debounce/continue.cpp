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

#include "mc/debounce/continue.h"
#include <mc/log.h>

namespace mc::debounce {

Continue::Continue(int count) : m_count(count), m_stable_val(-1), m_unstable_val(-1),
    m_unstable_val_count(0), is_valid(false)
{
    if (count <= 0) {
        throw std::runtime_error("count must be greater than 0");
    }
}

std::optional<int> Continue::get_debounce_val(int new_val)
{
    if (m_stable_val == new_val) {
        m_unstable_val = m_stable_val;
        m_unstable_val_count = 0;
    } else {
        if (m_unstable_val == new_val) {
            m_unstable_val_count++;
        } else {
            m_unstable_val = new_val;
            m_unstable_val_count = 1;
        }
    }

    dlog("m_unstable_val: ${val1}, m_stable_val: ${val2}, m_unstable_val_count: ${val3}",
        ("val1", m_unstable_val)("val2", m_stable_val)("val3", m_unstable_val_count));

    if (m_unstable_val_count >= m_count) {
        m_stable_val = m_unstable_val;
        m_unstable_val_count = 0;
        is_valid = true;
    }

    if (!is_valid) {
        return std::nullopt;
    }

    return m_stable_val;
}

void Continue::clear_debounce_val()
{
   m_count = 0;
   m_stable_val = -1;
   m_unstable_val = -1;
   m_unstable_val_count = 0;
   is_valid = false;
}

}