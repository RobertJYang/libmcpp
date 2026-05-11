/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_COMPARISON_GREATER_EQUAL_H
#define MC_PP_COMPARISON_GREATER_EQUAL_H

#include <mc/pp/comparison/greater.h>
#include <mc/pp/comparison/logical.h>

// MC_PP_GREATER_EQUAL(x, y) => 1 if x >= y, else 0  (x, y in 0..20)
#define MC_PP_GREATER_EQUAL(x, y) MC_PP_NOT(MC_PP_GREATER(y, x))

#endif // MC_PP_COMPARISON_GREATER_EQUAL_H
