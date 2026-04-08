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

#ifndef MC_PRETTY_NAME_H
#define MC_PRETTY_NAME_H

#include <cstddef>

namespace mc {

namespace detail {
// 用于获取类型的美化名称
struct pretty_tag_msvc {};
struct pretty_tag_gcc {};

#define PRETTY_NAME_MAX_LENGTH 128

// 根据编译器选择合适的标签
#if defined(_MSC_VER) && !defined(__clang__)
using pretty_default_tag = pretty_tag_msvc;
#else
using pretty_default_tag = pretty_tag_gcc;
#endif

// MSVC版本实现
template <typename T>
constexpr inline const char* pretty_name_impl(pretty_tag_msvc, char (&result)[PRETTY_NAME_MAX_LENGTH])
{
#if defined(_MSC_VER) && !defined(__clang__)
    const char* name = __FUNCSIG__;
    // 跳过前缀 "const char *__cdecl mc::detail::pretty_name_impl<"
    size_t begin = 0;
    for (size_t i = 0; name[i]; ++i) {
        if (name[i] == '<') {
            begin = i + 1;
            break;
        }
    }
    // 跳过结尾的 ">(mc::detail::pretty_tag_msvc)"
    size_t end = 0;
    for (size_t i = begin; name[i]; ++i) {
        if (name[i] == '>') {
            end = i;
            break;
        }
    }

    // 返回裁剪后的类型名称
    size_t len = end > begin ? end - begin : 0;
    len        = len < (PRETTY_NAME_MAX_LENGTH - 1) ? len : (PRETTY_NAME_MAX_LENGTH - 1);
    for (size_t i = 0; i < len; ++i) {
        result[i] = name[begin + i];
    }
    result[len] = '\0';
    return result;
#else
    MC_UNUSED(result);
    return "unknown";
#endif
}

// GCC/Clang版本实现
template <typename T>
constexpr inline const char* pretty_name_impl(pretty_tag_gcc, char (&result)[PRETTY_NAME_MAX_LENGTH])
{
#if defined(__GNUC__) || defined(__clang__)
    const char* name = __PRETTY_FUNCTION__;
    // 跳过前缀，寻找等号
    size_t begin = 0;
    for (size_t i = 0; name[i]; ++i) {
        if (name[i] == '=') {
            begin = i + 2; // 跳过"= "
            break;
        }
    }
    // 跳过结尾的 "]"
    size_t end = 0;
    for (size_t i = begin; name[i]; ++i) {
        if (name[i] == ']') {
            end = i;
            break;
        }
    }

    // 返回裁剪后的类型名称
    size_t len = end > begin ? end - begin : 0;
    len        = len < (PRETTY_NAME_MAX_LENGTH - 1) ? len : (PRETTY_NAME_MAX_LENGTH - 1);
    for (size_t i = 0; i < len; ++i) {
        result[i] = name[begin + i];
    }
    result[len] = '\0';
    return result;
#else
    MC_UNUSED(result);
    return "unknown";
#endif
}

template <typename T>
constexpr inline const char* pretty_name_impl(char (&result)[PRETTY_NAME_MAX_LENGTH])
{
    return pretty_name_impl<T>(pretty_default_tag{}, result);
}

} // namespace detail

/**
 * @brief 获取类型的美化名称
 * @tparam T 要获取名称的类型
 * @return 类型的美化名称
 */
template <typename T>
inline const char* pretty_name()
{
    static thread_local char result[PRETTY_NAME_MAX_LENGTH];
    return detail::pretty_name_impl<T>(result);
}

} // namespace mc

#endif // MC_PRETTY_NAME_H