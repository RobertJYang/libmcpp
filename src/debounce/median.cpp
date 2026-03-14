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

#include "mc/debounce/median.h"
#include <algorithm>

namespace mc::debounce {

Median::Median(int size, bool is_signed) : m_size(size), is_signed(is_signed)
{
    if (size <= 0) {
        throw std::runtime_error("size must be greater than 0");
    }
}

std::optional<int> Median::get_debounce_val(int raw_val)
{
    int new_val = raw_val;
    if (is_signed && new_val > 127) {
        new_val = new_val - 256;
    }
    m_data.push_back(new_val);
    // 如果窗口超过指定大小，移除最旧的数据
    const std::size_t window_size = static_cast<std::size_t>(m_size);
    if (m_data.size() > window_size) {
        m_data.pop_front();
    }

    if (m_data.size() < window_size) {
        return std::nullopt;
    }

    // 复制窗口数据以便排序
    std::vector<int> values(m_data.begin(), m_data.end());

    // 排序以便找到最小和最大值
    std::sort(values.begin(), values.end());
    values.erase(values.begin()); // 移除最小值
    values.pop_back();            // 移除最大值
    // 计算中位数
    size_t n       = values.size();
    int    dbd_val = values[n / 2];
    // 计算平均值
    return adjust_signed(dbd_val);
}

void Median::clear_debounce_val()
{
    m_data.clear();
}

int Median::adjust_signed(int value) const
{
    if (is_signed && value < 0) {
        return value + 256;
    }
    return value;
}

} // namespace mc::debounce