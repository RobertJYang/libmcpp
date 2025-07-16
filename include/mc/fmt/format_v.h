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

#ifndef MC_FORMAT_V_H
#define MC_FORMAT_V_H

#include <stdarg.h>
#include <string>
#include <string_view>

#include <mc/common.h>

namespace mc {

namespace fmt {
/**
 * @brief C 风格可变参数格式化字符串
 * @param format 格式化字符串
 * @param ... 可变参数
 * @return 格式化后的字符串
 */
MC_API std::string format_v(const char* format, ...);

/**
 * @brief C 风格可变参数格式化字符串
 * @param format 格式化字符串
 * @param args 可变参数
 * @return 格式化后的字符串
 */
MC_API std::string format_vv(const char* format, va_list args);

/**
 * @brief C 风格可变参数格式化字符串，并追加到目标字符串
 * @param result 接收格式化结果的目标字符串
 * @param format 格式化字符串
 * @param ... 可变参数
 */
MC_API bool format_v(std::string& result, const char* format, ...);

/**
 * @brief C 风格可变参数格式化字符串，并追加到目标字符串
 * @param result 接收格式化结果的目标字符串
 * @param format 格式化字符串
 * @param args 可变参数
 */
MC_API bool format_vv(std::string& result, const char* format, va_list args);

} // namespace fmt

// 导出到全局命名空间
using fmt::format_v;
using fmt::format_vv;
} // namespace mc

#endif // MC_FORMAT_V_H