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
 * @file variant.h
 * @brief 定义了 mc 命名空间下的 variant 类，用于表示任意类型的数据
 */
#ifndef MC_VARIANT_H
#define MC_VARIANT_H

#include <mc/variant/variant_base.h>

namespace mc {

using variant       = variant_base<>;
using blob          = blob_base<>;
using variants      = typename variant::array_type;
using typed_variant = variant_base<variant_config<std::allocator<char>, true>>;

} // namespace mc

#endif // MC_VARIANT_H