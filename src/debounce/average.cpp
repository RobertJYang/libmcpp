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

#include "mc/debounce/average.h"
#include <algorithm>

namespace mc::debounce {

Average::Average(int size, bool is_signed) : m_size(size), is_signed(is_signed)
{
    if (size <= 0) {
        throw std::runtime_error("size must be greater than 0");
    }
}

std::optional<int> Average::get_debounce_val(int raw_val)
{
    int new_val = raw_val;
    if (is_signed && new_val > 127) {
        new_val = new_val - 256;
    }
    m_data.push_back(new_val);
    // 如果窗口超过指定大小，移除最旧的数据
    if (m_data.size() > m_size) {
        m_data.pop_front();
    }

    if (m_data.size() < m_size) {
        return std::nullopt;
    }
    int dbd_val;
    if (m_size < 3) {
        int sum = std::accumulate(m_data.begin(), m_data.end(), 0);
        dbd_val = sum / m_size;
    } else {
        // 复制窗口数据以便排序
        std::vector<int> values(m_data.begin(), m_data.end());
        // 排序以便找到最小和最大值
        std::sort(values.begin(), values.end());
        int sum = std::accumulate(values.begin() + 1, values.end() - 1, 0);
        dbd_val = sum / (m_size - 2);
    }
    // 计算平均值
    return adjust_signed(dbd_val);
}

void Average::clear_debounce_val()
{
    m_data.clear();
}

int Average::adjust_signed(int value) const
{
    if (is_signed && value < 0) {
        return value + 256;
    }
    return value;
}

}