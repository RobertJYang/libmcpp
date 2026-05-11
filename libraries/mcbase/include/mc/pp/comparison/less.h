/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_COMPARISON_LESS_H
#define MC_PP_COMPARISON_LESS_H

#include <mc/pp/comparison/greater.h>

// MC_PP_LESS(x, y) => 1 if x < y, else 0  (x, y in 0..20)
#define MC_PP_LESS(x, y) MC_PP_GREATER(y, x)

#endif // MC_PP_COMPARISON_LESS_H
