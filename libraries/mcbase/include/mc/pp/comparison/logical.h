/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_COMPARISON_LOGICAL_H
#define MC_PP_COMPARISON_LOGICAL_H

#include <mc/pp/control.h>

// MC_PP_NOT(n) => 1 if n == 0, else 0
#define MC_PP_NOT(n) MC_PP_IIF(n, 0, 1)

// MC_PP_AND(a, b) => 1 if both a and b are non-zero, else 0  (a, b in {0, 1})
#define MC_PP_AND(a, b) MC_PP_IIF(a, b, 0)

// MC_PP_OR(a, b) => 1 if either a or b is non-zero, else 0  (a, b in {0, 1})
#define MC_PP_OR(a, b) MC_PP_IIF(a, 1, b)

#endif // MC_PP_COMPARISON_LOGICAL_H
