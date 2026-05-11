/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_COMPARISON_EQUAL_H
#define MC_PP_COMPARISON_EQUAL_H

#include <mc/pp/comparison/bool.h>
#include <mc/pp/comparison/logical.h>
#include <mc/pp/comparison/sub.h>

// MC_PP_EQUAL(x, y) => 1 if x == y, else 0  (x, y in 0..20)
// x == y iff neither x > y nor y > x
#define MC_PP_EQUAL(x, y) MC_PP_AND(MC_PP_NOT(MC_PP_BOOL(MC_PP_SUB(x, y))), MC_PP_NOT(MC_PP_BOOL(MC_PP_SUB(y, x))))

#endif // MC_PP_COMPARISON_EQUAL_H
