/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/**
 * @file set.h
 * @brief 封装 Boost 的侵入式有序集合容器
 */
#ifndef MC_INTRUSIVE_SET_H
#define MC_INTRUSIVE_SET_H

#include <boost/intrusive/set.hpp>
#include <functional>
#include <mc/intrusive/hook.h>

namespace mc::intrusive {

using boost::intrusive::set;

} // namespace mc::intrusive

#endif // MC_INTRUSIVE_SET_H