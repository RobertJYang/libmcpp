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
#include <mc/string_view.h>
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

inline char type_to_char(type_code dt)
{
    return static_cast<char>(dt);
}

inline type_code char_to_type(char c)
{
    return static_cast<type_code>(c);
}

// 类型组合
namespace container {
constexpr mc::string_view array_of_byte{"ay", 2};         // 字节数组
constexpr mc::string_view array_of_string{"as", 2};       // 字符串数组
constexpr mc::string_view array_of_variant{"av", 2};      // 变体数组
constexpr mc::string_view dict_string_var{"a{sv}", 5};    // 字符串到 variant 的 dict
constexpr mc::string_view dict_string_string{"a{ss}", 5}; // 字符串到字符串的 dict
} // namespace container

/**
 * @brief 类型签名常量（兼容 DBUS 定义）
 */
namespace signature_chars {
constexpr char STRUCT_BEGIN     = '(';  // 结构体类型开始
constexpr char STRUCT_END       = ')';  // 结构体类型结束
constexpr char DICT_ENTRY_BEGIN = '{';  // 字典条目开始
constexpr char DICT_ENTRY_END   = '}';  // 字典条目结束
constexpr char ARRAY            = 'a';  // 数组类型
constexpr char INVALID          = '\0'; // 无效类型
} // namespace signature_chars

#define MC_STRUCT_BEGIN_CHAR     mc::reflect::signature_chars::STRUCT_BEGIN
#define MC_STRUCT_END_CHAR       mc::reflect::signature_chars::STRUCT_END
#define MC_DICT_ENTRY_BEGIN_CHAR mc::reflect::signature_chars::DICT_ENTRY_BEGIN
#define MC_DICT_ENTRY_END_CHAR   mc::reflect::signature_chars::DICT_ENTRY_END
#define MC_TYPE_ARRAY            mc::reflect::signature_chars::ARRAY
#define MC_TYPE_INVALID          mc::reflect::signature_chars::INVALID
#define MC_TYPE_STRUCT           'r'


#define MC_TYPE_DICT_ENTRY       'e'
#define MC_TYPE_STRING           's'


#define MC_TYPE_SIGNATURE        'g'
#define MC_TYPE_OBJECT_PATH      'o'
#define MC_TYPE_UNIX_FD          'h'
#define MC_TYPE_INT32            'i'
#define MC_TYPE_UINT32           'u'
#define MC_TYPE_INT16            'n'
#define MC_TYPE_UINT16           'q'
#define MC_TYPE_INT64            'x'
#define MC_TYPE_UINT64           't'
#define MC_TYPE_BOOLEAN          'b'
#define MC_TYPE_BYTE             'y'
#define MC_TYPE_DOUBLE           'd'
#define MC_TYPE_VARIANT          'v'

} // namespace mc::reflect

#endif // MC_REFLECT_TYPE_CODE_H