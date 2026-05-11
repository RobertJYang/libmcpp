/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#pragma once

#include <cstdint>

namespace mc::engine {

// 属性类型枚举
enum class p_type : uint32_t {
    normal     = 0, // 普通属性
    sync       = 1, // 同步属性
    reference  = 2, // 引用属性
    ref_object = 3  // 引用对象属性
};

// 定义常量以便与 int 类型兼容
namespace property_options {
constexpr int memory               = 1;
constexpr int from_mdb             = 2;
constexpr int with_object_property = 4;
} // namespace property_options

} // namespace mc::engine