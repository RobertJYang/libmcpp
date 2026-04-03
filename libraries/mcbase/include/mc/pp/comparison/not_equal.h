/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_COMPARISON_NOT_EQUAL_H
#define MC_PP_COMPARISON_NOT_EQUAL_H

#include <mc/pp/comparison/equal.h>
#include <mc/pp/comparison/logical.h>

// MC_PP_NOT_EQUAL(x, y) => 1 if x != y, else 0  (x, y in 0..20)
#define MC_PP_NOT_EQUAL(x, y) MC_PP_NOT(MC_PP_EQUAL(x, y))

#endif // MC_PP_COMPARISON_NOT_EQUAL_H
