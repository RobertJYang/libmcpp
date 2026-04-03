/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_COMPARISON_BOOL_H
#define MC_PP_COMPARISON_BOOL_H

#include <mc/pp/cat.h>

// MC_PP_BOOL(n) => 0 if n == 0, else 1  (n in 0..20)
#define MC_PP_BOOL(n) MC_PP_CAT(MC_PP_BOOL_, n)

#define MC_PP_BOOL_0  0
#define MC_PP_BOOL_1  1
#define MC_PP_BOOL_2  1
#define MC_PP_BOOL_3  1
#define MC_PP_BOOL_4  1
#define MC_PP_BOOL_5  1
#define MC_PP_BOOL_6  1
#define MC_PP_BOOL_7  1
#define MC_PP_BOOL_8  1
#define MC_PP_BOOL_9  1
#define MC_PP_BOOL_10 1
#define MC_PP_BOOL_11 1
#define MC_PP_BOOL_12 1
#define MC_PP_BOOL_13 1
#define MC_PP_BOOL_14 1
#define MC_PP_BOOL_15 1
#define MC_PP_BOOL_16 1
#define MC_PP_BOOL_17 1
#define MC_PP_BOOL_18 1
#define MC_PP_BOOL_19 1
#define MC_PP_BOOL_20 1

#endif // MC_PP_COMPARISON_BOOL_H
