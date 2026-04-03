/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_FACILITIES_H
#define MC_PP_FACILITIES_H

#include <mc/pp/cat.h>
#include <mc/pp/comparison/comparison.h>
#include <mc/pp/tuple.h>
#include <mc/pp/variadic.h>

// --- IS_EMPTY ---
// MC_PP_IS_EMPTY(...) => 1 if empty, 0 otherwise.
// Uses the same dummy+##__VA_ARGS__ technique as the existing codebase.
#define MC_PP_IS_EMPTY(...) MC_PP_EQUAL(MC_PP_VARIADIC_SIZE(dummy, ##__VA_ARGS__), 1)

// --- IS_BEGIN_PARENS ---
// MC_PP_IS_BEGIN_PARENS(...) => 1 if args start with '(', else 0.
// Variadic — correctly handles comma-containing arguments.
// Uses the "variadic probe + token-paste dispatch" technique:
//   1. MC_PP_IBP_PROBE is a variadic macro that returns 1 when invoked.
//      If __VA_ARGS__ starts with '(', PROBE is invoked and produces 1.
//      Otherwise PROBE is not invoked and remains as a token.
//   2. MC_PP_IBP_PRIM_CAT uses ## __VA_ARGS__ to concatenate R_ with either
//      the probe result (1) or the probe name (MC_PP_IBP_PROBE).
//   3. MC_PP_IBP_R_1 = "1," and MC_PP_IBP_R_MC_PP_IBP_PROBE = "0," provide
//      the dispatch values with trailing commas for SPLIT_0 extraction.
#define MC_PP_IS_BEGIN_PARENS(...) \
    MC_PP_IBP_SPLIT(0, MC_PP_IBP_CAT(MC_PP_IBP_R_, MC_PP_IBP_PROBE __VA_ARGS__))

#define MC_PP_IBP_PROBE(...)  1

#define MC_PP_IBP_CAT(a, ...)      MC_PP_IBP_PRIM_CAT(a, __VA_ARGS__)
#define MC_PP_IBP_PRIM_CAT(a, ...) a ## __VA_ARGS__

#define MC_PP_IBP_R_1               1,
#define MC_PP_IBP_R_MC_PP_IBP_PROBE 0,

#define MC_PP_IBP_SPLIT(i, ...)      MC_PP_IBP_PRIM_CAT(MC_PP_IBP_SPLIT_, i)(__VA_ARGS__)
#define MC_PP_IBP_SPLIT_0(a, ...)    a
#define MC_PP_IBP_SPLIT_1(a, ...)    __VA_ARGS__

// --- IDENTITY ---
// MC_PP_IDENTITY(item) => item MC_PP_EMPTY
// Returns the item with a trailing MC_PP_EMPTY, so that an invocation like
// MC_PP_IDENTITY(item)() expands to item (EMPTY consumes the extra parens).
// This matches BOOST_PP_IDENTITY behavior.
#define MC_PP_IDENTITY(item) item MC_PP_EMPTY

// --- REMOVE_PARENS ---
// MC_PP_REMOVE_PARENS(param) => strips ONE layer of outer parens if present.
//   MC_PP_REMOVE_PARENS((a, b, c)) => a, b, c
//   MC_PP_REMOVE_PARENS(x)         => x
//
// Uses the same IIF + IS_BEGIN_PARENS + TUPLE_ENUM + IDENTITY pattern as
// BOOST_PP_REMOVE_PARENS: check if param starts with '(', then either
// TUPLE_ENUM (strip parens) or IDENTITY (passthrough). The extra () invokes
// the selected function.
#define MC_PP_REMOVE_PARENS(param)                                                                                       \
    MC_PP_IIF(MC_PP_IS_BEGIN_PARENS(param), MC_PP_REMOVE_PARENS_DO, MC_PP_IDENTITY)(param)()

#define MC_PP_REMOVE_PARENS_DO(param) MC_PP_IDENTITY(MC_PP_TUPLE_ENUM(param))

// --- EMPTY ---
// MC_PP_EMPTY() => (expands to nothing)
// Note: also usable as MC_PP_EMPTY (without parens) when passed as a callback.
#define MC_PP_EMPTY(...)

#endif // MC_PP_FACILITIES_H
