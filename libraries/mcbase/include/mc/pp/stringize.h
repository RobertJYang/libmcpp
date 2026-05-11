/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_PP_STRINGIZE_H
#define MC_PP_STRINGIZE_H

// Convert token to string — MC_PP_STRINGIZE(x) => "x"
#define MC_PP_STRINGIZE(x)    MC_PP_STRINGIZE_I(x)
#define MC_PP_STRINGIZE_I(x)  #x

#endif // MC_PP_STRINGIZE_H
