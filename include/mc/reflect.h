/*
 * Copyright (c) 2023 Huawei Technologies Co., Ltd.
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
 * @file reflect.h
 * @brief 反射相关的头文件总引入
 */
#ifndef MC_REFLECT_H
#define MC_REFLECT_H

#include <mc/reflect/method.h>
#include <mc/reflect/property.h>
#include <mc/reflect/reflect.h>
#include <mc/reflect/reflection.h>
#include <mc/reflect/reflection_factory.h>

namespace mc {
template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
std::string to_string(const T& v)
{
    mc::variant var;
    mc::reflect::to_variant(v, var);
    return mc::to_string(var);
}
} // namespace mc

#endif // MC_REFLECT_H