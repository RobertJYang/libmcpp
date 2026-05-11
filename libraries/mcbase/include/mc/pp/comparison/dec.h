/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_COMPARISON_DEC_H
#define MC_PP_COMPARISON_DEC_H

#include <mc/pp/cat.h>

// MC_PP_DEC(n) => n - 1, saturating at 0  (n in 0..20)
#define MC_PP_DEC(n) MC_PP_CAT(MC_PP_DEC_, n)

#define MC_PP_DEC_0  0
#define MC_PP_DEC_1  0
#define MC_PP_DEC_2  1
#define MC_PP_DEC_3  2
#define MC_PP_DEC_4  3
#define MC_PP_DEC_5  4
#define MC_PP_DEC_6  5
#define MC_PP_DEC_7  6
#define MC_PP_DEC_8  7
#define MC_PP_DEC_9  8
#define MC_PP_DEC_10 9
#define MC_PP_DEC_11 10
#define MC_PP_DEC_12 11
#define MC_PP_DEC_13 12
#define MC_PP_DEC_14 13
#define MC_PP_DEC_15 14
#define MC_PP_DEC_16 15
#define MC_PP_DEC_17 16
#define MC_PP_DEC_18 17
#define MC_PP_DEC_19 18
#define MC_PP_DEC_20 19

#endif // MC_PP_COMPARISON_DEC_H
