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

#ifndef MC_DEBOUNCE_CONTINUE_H
#define MC_DEBOUNCE_CONTINUE_H
#include <cstdint>
#include <optional>
#include "mc/debounce/base.h"

namespace mc::debounce {
class MC_API Continue : public Base {
public:
    explicit Continue(int count);
    std::optional<int> get_debounce_val(int val) override;
    void clear_debounce_val() override;

private:
    int m_count;
    int m_stable_val;
    int m_unstable_val;
    int m_unstable_val_count;
    bool is_valid;
};
}

#endif // MC_DEBOUNCE_CONTINUE_H