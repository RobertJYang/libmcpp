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
#ifndef MC_MUTABLE_DICT_H
#define MC_MUTABLE_DICT_H

#include <mc/dict/dict.h>

namespace mc {

/**
 * @brief mutable_dict 是 dict 的别名
 *
 * @note 为了向后兼容保留此别名。dict 本身就是可变的。
 */
using mutable_dict = dict;

} // namespace mc

#endif
