/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_COMPARISON_INC_H
#define MC_PP_COMPARISON_INC_H

#include <mc/pp/cat.h>

// MC_PP_INC(n) => n + 1  (saturating at 20, n in 0~20)
#define MC_PP_INC(n) MC_PP_CAT(MC_PP_INC_, n)

#define MC_PP_INC_0 1
#define MC_PP_INC_1 2
#define MC_PP_INC_2 3
#define MC_PP_INC_3 4
#define MC_PP_INC_4 5
#define MC_PP_INC_5 6
#define MC_PP_INC_6 7
#define MC_PP_INC_7 8
#define MC_PP_INC_8 9
#define MC_PP_INC_9 10
#define MC_PP_INC_10 11
#define MC_PP_INC_11 12
#define MC_PP_INC_12 13
#define MC_PP_INC_13 14
#define MC_PP_INC_14 15
#define MC_PP_INC_15 16
#define MC_PP_INC_16 17
#define MC_PP_INC_17 18
#define MC_PP_INC_18 19
#define MC_PP_INC_19 20
#define MC_PP_INC_20 20

#endif // MC_PP_COMPARISON_INC_H
