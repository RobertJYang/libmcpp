/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_COMPARISON_SUB_H
#define MC_PP_COMPARISON_SUB_H

#include <mc/pp/cat.h>
#include <mc/pp/comparison/dec.h>

// MC_PP_SUB(x, y) => x - y, saturating at 0  (x, y in 0..20)
// Dispatches on y, then chains DEC y times.
#define MC_PP_SUB(x, y) MC_PP_CAT(MC_PP_SUB_, y)(x)

#define MC_PP_SUB_0(x)  x
#define MC_PP_SUB_1(x)  MC_PP_DEC(x)
#define MC_PP_SUB_2(x)  MC_PP_DEC(MC_PP_DEC(x))
#define MC_PP_SUB_3(x)  MC_PP_DEC(MC_PP_DEC(MC_PP_DEC(x)))
#define MC_PP_SUB_4(x)  MC_PP_DEC(MC_PP_SUB_3(x))
#define MC_PP_SUB_5(x)  MC_PP_DEC(MC_PP_SUB_4(x))
#define MC_PP_SUB_6(x)  MC_PP_DEC(MC_PP_SUB_5(x))
#define MC_PP_SUB_7(x)  MC_PP_DEC(MC_PP_SUB_6(x))
#define MC_PP_SUB_8(x)  MC_PP_DEC(MC_PP_SUB_7(x))
#define MC_PP_SUB_9(x)  MC_PP_DEC(MC_PP_SUB_8(x))
#define MC_PP_SUB_10(x) MC_PP_DEC(MC_PP_SUB_9(x))
#define MC_PP_SUB_11(x) MC_PP_DEC(MC_PP_SUB_10(x))
#define MC_PP_SUB_12(x) MC_PP_DEC(MC_PP_SUB_11(x))
#define MC_PP_SUB_13(x) MC_PP_DEC(MC_PP_SUB_12(x))
#define MC_PP_SUB_14(x) MC_PP_DEC(MC_PP_SUB_13(x))
#define MC_PP_SUB_15(x) MC_PP_DEC(MC_PP_SUB_14(x))
#define MC_PP_SUB_16(x) MC_PP_DEC(MC_PP_SUB_15(x))
#define MC_PP_SUB_17(x) MC_PP_DEC(MC_PP_SUB_16(x))
#define MC_PP_SUB_18(x) MC_PP_DEC(MC_PP_SUB_17(x))
#define MC_PP_SUB_19(x) MC_PP_DEC(MC_PP_SUB_18(x))
#define MC_PP_SUB_20(x) MC_PP_DEC(MC_PP_SUB_19(x))

#endif // MC_PP_COMPARISON_SUB_H
