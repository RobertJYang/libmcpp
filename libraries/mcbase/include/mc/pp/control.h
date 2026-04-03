/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 */

#ifndef MC_PP_CONTROL_H
#define MC_PP_CONTROL_H

// MC_PP_IIF(cond, t, f) — immediate conditional: cond must be 0 or 1
// Uses a "resolve-then-paste" pattern: the condition is first resolved through
// MC_PP_IIF_RESOLVE which forces full expansion via argument substitution,
// then the resolved 0/1 value is pasted with MC_PP_IIF_ in a separate step.
#define MC_PP_IIF(cond, t, f) MC_PP_IIF_RESOLVE(cond)(t, f)
#define MC_PP_IIF_RESOLVE(cond) MC_PP_IIF_PASTE(MC_PP_IIF_, cond)
#define MC_PP_IIF_PASTE(prefix, val) prefix##val
#define MC_PP_IIF_0(t, f) f
#define MC_PP_IIF_1(t, f) t

// MC_PP_IF(cond, t, f) — like IIF but cond is expanded first
#define MC_PP_IF(cond, t, f) MC_PP_IIF(cond, t, f)

#endif // MC_PP_CONTROL_H
