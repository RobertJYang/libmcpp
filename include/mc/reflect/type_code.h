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

#ifndef MC_REFLECT_TYPE_CODE_H
#define MC_REFLECT_TYPE_CODE_H
#include <string_view>

namespace mc::reflect {

/**
 * @brief 类型代码常量
 *
 * 定义类型的字符代码，避免代码中硬编码字符
 */
enum class type_code {
    invalid_type     = '\0',
    byte_type        = 'y', // 字节
    boolean_type     = 'b', // 布尔
    int16_type       = 'n', // 16位整数
    uint16_type      = 'q', // 16位无符号整数
    int32_type       = 'i', // 32位整数
    uint32_type      = 'u', // 32位无符号整数
    int64_type       = 'x', // 64位整数
    uint64_type      = 't', // 64位无符号整数
    double_type      = 'd', // 双精度浮点数
    string_type      = 's', // 字符串
    object_path_type = 'o', // 对象路径
    signature_type   = 'g', // 签名
    array_type       = 'a', // 数组
    struct_type      = 'r', // 结构
    variant_type     = 'v', // 变体
    dict_entry_type  = 'e', // 字典条目
    unix_fd_type     = 'h', // Unix文件描述符

    // 容器类型
    array_start      = 'a', // 数组开始
    struct_start     = '(', // 结构体开始
    struct_end       = ')', // 结构体结束
    variant          = 'v', // 变体类型
    dict_entry_start = '{', // 字典条目开始
    dict_entry_end   = '}', // 字典条目结束
};

inline char type_to_char(type_code dt) {
    return static_cast<char>(dt);
}

inline type_code char_to_type(char c) {
    return static_cast<type_code>(c);
}

// 类型组合
namespace container {
constexpr std::string_view array_of_byte    = "ay";    // 字节数组
constexpr std::string_view array_of_string  = "as";    // 字符串数组
constexpr std::string_view array_of_variant = "av";    // 变体数组
constexpr std::string_view dict_string_var  = "a{sv}"; // 字符串到变体的字典
} // namespace container

} // namespace mc::reflect

#endif // MC_REFLECT_TYPE_CODE_H