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

#ifndef MC_DEBOUNCE_BASE_H
#define MC_DEBOUNCE_BASE_H
#include <cstdint>
#include <deque>
#include <mc/common.h>
#include <numeric>
#include <optional>
#include <vector>

namespace mc::debounce {
class MC_API Base {
public:
    virtual std::optional<int> get_debounce_val(int val) = 0;
    virtual void               clear_debounce_val()      = 0;
};
} // namespace mc::debounce

#endif // MC_DEBOUNCE_BASE_H