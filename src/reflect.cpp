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
 * @file reflect.cpp
 * @brief 实现反射相关的功能
 */
#include <mc/reflect/reflect.h>
#include <mc/exception.h>
#include <sstream>

namespace mc {
namespace reflect {

/**
 * @brief 抛出枚举转换异常
 * 
 * @param i 整数值
 * @param e 枚举类型名称
 */
void throw_bad_enum_cast(int64_t i, const char* e) {
    std::stringstream ss;
    ss << "无法将整数 " << i << " 转换为枚举类型 " << e;
    throw mc::bad_cast_exception(ss.str());
}

/**
 * @brief 抛出枚举转换异常
 * 
 * @param k 字符串键
 * @param e 枚举类型名称
 */
void throw_bad_enum_cast(const char* k, const char* e) {
    std::stringstream ss;
    ss << "无法将字符串 '" << k << "' 转换为枚举类型 " << e;
    throw mc::bad_cast_exception(ss.str());
}

} // namespace reflect
} // namespace mc 