/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_COMPARISON_GREATER_H
#define MC_PP_COMPARISON_GREATER_H

#include <mc/pp/comparison/bool.h>
#include <mc/pp/comparison/sub.h>

// MC_PP_GREATER(x, y) => 1 if x > y, else 0  (x, y in 0..20)
// Derived: SUB(x, y) > 0 iff x > y
#define MC_PP_GREATER(x, y) MC_PP_BOOL(MC_PP_SUB(x, y))

#endif // MC_PP_COMPARISON_GREATER_H
