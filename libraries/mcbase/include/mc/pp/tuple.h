/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_TUPLE_H
#define MC_PP_TUPLE_H

// MC_PP_TUPLE_ELEM(n, tuple) => element at index n (0-based, n in 0~20)
#define MC_PP_TUPLE_ELEM(n, tuple) MC_PP_TUPLE_ELEM_I(n, tuple)
#define MC_PP_TUPLE_ELEM_I(n, tuple) MC_PP_TUPLE_ELEM_##n tuple

#define MC_PP_TUPLE_ELEM_0(e0, ...) e0
#define MC_PP_TUPLE_ELEM_1(e0, e1, ...) e1
#define MC_PP_TUPLE_ELEM_2(e0, e1, e2, ...) e2
#define MC_PP_TUPLE_ELEM_3(e0, e1, e2, e3, ...) e3
#define MC_PP_TUPLE_ELEM_4(e0, e1, e2, e3, e4, ...) e4
#define MC_PP_TUPLE_ELEM_5(e0, e1, e2, e3, e4, e5, ...) e5
#define MC_PP_TUPLE_ELEM_6(e0, e1, e2, e3, e4, e5, e6, ...) e6
#define MC_PP_TUPLE_ELEM_7(e0, e1, e2, e3, e4, e5, e6, e7, ...) e7
#define MC_PP_TUPLE_ELEM_8(e0, e1, e2, e3, e4, e5, e6, e7, e8, ...) e8
#define MC_PP_TUPLE_ELEM_9(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, ...) e9
#define MC_PP_TUPLE_ELEM_10(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, ...) e10
#define MC_PP_TUPLE_ELEM_11(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, ...) e11
#define MC_PP_TUPLE_ELEM_12(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, ...) e12
#define MC_PP_TUPLE_ELEM_13(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, ...) e13
#define MC_PP_TUPLE_ELEM_14(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, ...) e14
#define MC_PP_TUPLE_ELEM_15(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, ...) e15
#define MC_PP_TUPLE_ELEM_16(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, ...) e16
#define MC_PP_TUPLE_ELEM_17(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, ...) e17
#define MC_PP_TUPLE_ELEM_18(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, ...) e18
#define MC_PP_TUPLE_ELEM_19(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, ...) e19
#define MC_PP_TUPLE_ELEM_20(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, ...) e20

// MC_PP_TUPLE_ENUM(tuple) => strip outer parens and enumerate elements with commas
// MC_PP_TUPLE_ENUM((a, b, c)) => a, b, c
#define MC_PP_TUPLE_ENUM(tuple) MC_PP_TUPLE_ENUM_I tuple
#define MC_PP_TUPLE_ENUM_I(...) __VA_ARGS__

#endif // MC_PP_TUPLE_H
