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

#ifndef MC_FORMAT_H
#define MC_FORMAT_H

#include <string>
#include <string_view>

namespace mc {
class dict;

namespace fmt {
/**
 * @brief 使用字典格式化字符串
 * @param result 格式化结果的目标字符串
 * @param format_str 格式化字符串
 * @param args 参数字典
 */
void format_dict(std::string& result, std::string_view format_str, const mc::dict& args);

/**
 * @brief 使用字典格式化字符串
 * @param format_str 格式化字符串
 * @param args 参数字典
 * @return 格式化后的字符串
 */
std::string format_dict(std::string_view format_str, const mc::dict& args);

/**
 * @brief 使用字典格式化字符串（忽略大小写）
 * @param result 格式化结果的目标字符串
 * @param format_str 格式化字符串
 * @param args 参数字典
 */
void format_dict_icase(std::string& result, std::string_view format_str, const mc::dict& args);

/**
 * @brief 使用字典格式化字符串（忽略大小写）
 * @param format_str 格式化字符串
 * @param args 参数字典
 * @return 格式化后的字符串
 */
std::string format_dict_icase(std::string_view format_str, const mc::dict& args);

/**
 * @brief 使用可变参数格式化字符串
 * @param format 格式化字符串
 * @param ... 可变参数
 * @return 格式化后的字符串
 */
std::string format_v(const char* format, ...);

/**
 * @brief 使用可变参数格式化字符串
 * @param format 格式化字符串
 * @param args 可变参数
 * @return 格式化后的字符串
 */
std::string format_vv(const char* format, va_list args);

/**
 * @brief 使用可变参数格式化字符串，并追加到目标字符串
 * @param result 接收格式化结果的目标字符串
 * @param format 格式化字符串
 * @param ... 可变参数
 */
bool format_v(std::string& result, const char* format, ...);

/**
 * @brief 使用可变参数格式化字符串，并追加到目标字符串
 * @param result 接收格式化结果的目标字符串
 * @param format 格式化字符串
 * @param args 可变参数
 */
bool format_vv(std::string& result, const char* format, va_list args);

/**
 * @brief 解析格式化字符串，获取参数名称
 * @param format 格式化字符串
 * @param arg_names 存储参数名称的字典
 * @return 如果解析成功返回 true，否则返回 false
 */
bool get_format_args(std::string_view format, mc::dict& arg_names);

} // namespace fmt

// 导出到全局命名空间
using fmt::format_dict;
using fmt::format_dict_icase;
using fmt::format_v;
using fmt::format_vv;
} // namespace mc

#endif // MC_FORMAT_H