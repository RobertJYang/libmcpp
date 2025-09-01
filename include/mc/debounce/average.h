/**
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

#ifndef MC_DEBOUNCE_AVERAGE_H
#define MC_DEBOUNCE_AVERAGE_H
#include <optional>
#include <deque>
#include <cstdint>
#include <numeric>
#include <vector>
#include "mc/debounce/base.h"

namespace mc::debounce {
class MC_API Average : public Base {
public:
    Average(int size, bool is_signed);
    std::optional<int> get_debounce_val(int val) override;
    void clear_debounce_val() override;

private:
    int adjust_signed(int value) const;
    int m_size;
    bool is_signed;
    std::deque<int> m_data;
};
}

#endif // MC_DEBOUNCE_AVERAGE_H