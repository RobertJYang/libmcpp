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

#ifndef MC_MODULE_H
#define MC_MODULE_H

#include <algorithm>
#include <array>
#include <iostream>
#include <mc/module/module_manager.h>
#include <string>

namespace mc::module {
namespace detail {

#define MAX_MODULE_NAME_LENGTH 64

constexpr inline bool is_valid_module_name(std::string_view name) {
    if (name.empty() || name.size() > MAX_MODULE_NAME_LENGTH) {
        return false;
    }

    // 不能以分隔符开头或结尾
    if (name[0] == '_' || name[name.size() - 1] == '_') {
        return false;
    }

    bool segment_start = true; // 标记当前是否是段的开始
    for (size_t pos = 0; pos < name.size(); pos++) {
        char c = name[pos];

        if (c == '_') {
            // 不允许连续的分隔符
            if (segment_start) {
                return false;
            }
            segment_start = true;
        } else if (segment_start) {
            // 段的第一个字符必须是字母或下划线
            if (!mc::is_first_identifier_char(c)) {
                return false;
            }
            segment_start = false;
        } else {
            // 其他字符必须是合法的标识符字符
            if (!mc::is_identifier_char(c)) {
                return false;
            }
        }
    }

    return true;
}

// 编译时字符串长度计算
constexpr size_t str_len(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        ++len;
    }
    return len;
}

// 编译时将下划线替换为点号
constexpr auto convert_underscore_to_dot(const char* str) {
    std::array<char, MAX_MODULE_NAME_LENGTH> result{};
    for (size_t i = 0; str[i] != 0; ++i) {
        result[i]     = (str[i] == '_') ? '.' : str[i];
        result[i + 1] = '\0';
    }
    return result;
}

// 模块命名空间结构
template <const char* RawName>
struct module_namespace {
    static constexpr auto             converted_name = convert_underscore_to_dot(RawName);
    static constexpr std::string_view factory_name{converted_name.data(), str_len(converted_name.data())};
};

} // namespace detail

/**
 * @brief 定义模块
 *
 * @param module_name 模块标识符（使用下划线分隔，会自动转换为点号）
 *
 * 使用方法：
 * 1. 先定义模块名称常量
 * 2. 再使用该常量定义模块
 *
 * 示例:
 * ```cpp
 * // 定义模块名称常量
 * MC_MODULE_NAME(test_module, mc_test_module);
 *
 * // 使用模块
 * MC_BUILTIN_MODULE_IMPL(test_module);
 * MC_MODULE_REFLECT(test_module, ...);
 * ```
 */
#define MC_MODULE(module_name)                                                  \
    namespace mc::module::detail {                                              \
    constexpr char module_name##_name[] = #module_name;                         \
    static_assert(mc::module::detail::is_valid_module_name(module_name##_name), \
                  "Invalid module name");                                       \
    }                                                                           \
    using module_name##_namespace =                                             \
        mc::module::detail::module_namespace<mc::module::detail::module_name##_name>;

/**
 * @brief 注册内建模块到全局反射工厂
 *
 * @param module_name 模块变量（由 MC_MODULE_NAME 定义）
 */
#define MC_REGISTER_BUILTIN_MODULE(module_name)                           \
    []() {                                                                \
        auto& global_factory = mc::reflect::reflection_factory::global(); \
        global_factory.register_factory(                                  \
            mc::reflect::reflection_factory::instance_ptr<                \
                module_name##_namespace>());                              \
        return true;                                                      \
    }()

/**
 * @brief 内建模块的实现
 *
 * @param module_name 模块变量（由 MC_MODULE_NAME 定义）
 */
#define MC_BUILTIN_MODULE_IMPL(module_name)                     \
    namespace mc::module::detail {                              \
    [[maybe_unused]] static bool s_##module_name##_registered = \
        MC_REGISTER_BUILTIN_MODULE(module_name);                \
    }

#define MC_UNREGISTER_BUILTIN_MODULE(module_name)                         \
    {                                                                     \
        auto& global_factory = mc::reflect::reflection_factory::global(); \
        global_factory.unregister_factory(                                \
            module_name##_namespace::factory_name);                       \
    }

/**
 * @brief 为模块注册类型的反射信息
 *
 * @param module_name 模块变量（由 MC_MODULE_NAME 定义）
 * @param ... 类型的反射信息，格式与 MC_REFLECT 相同
 */
#define MC_MODULE_REFLECT(module_name, ...) \
    MC_REFLECT_WITH_NAMESPACE(module_name##_namespace, __VA_ARGS__)

/**
 * @brief 为模块注册枚举的反射信息
 *
 * @param module_name 模块变量（由 MC_MODULE_NAME 定义）
 * @param ... 枚举的反射信息，格式与 MC_REFLECT_ENUM 相同
 */
#define MC_MODULE_REFLECT_ENUM(module_name, ...) \
    MC_REFLECT_ENUM_WITH_NAMESPACE(module_name##_namespace, __VA_ARGS__)

/**
 * @brief 导出动态库模块的符号
 *
 * @param module_name 模块变量（由 MC_MODULE 定义）
 *
 * @note 此宏用于导出动态库模块所需的符号。它定义了两个关键函数：
 * - mc_open_module_name：创建并返回模块的工厂实例
 * - mc_close_module_name：模块卸载时的清理函数
 *
 * @note 当前这个实现很简单，只是在 mc_open 时返回了模块反射工厂，如果
 * 有特殊的初始化业务可以自行实现 mc_open_* 和 mc_close_*，没有必要使用
 * 这个 MC_EXPORT_MODULE 宏：
 * 如果需要自行实现模块导出，需要确保：
 * 1. 导出 C 风格的函数符号（使用 extern "C"）
 * 2. mc_open_* 函数返回工厂实例指针（void*）
 * 3. mc_close_* 函数处理必要的清理工作
 * 4. 函数命名必须遵循 mc_open_module_name 和 mc_close_module_name 的格式
 *
 * @note 注意我们这里并没有将模块反射工厂注册到全局反射工厂，因为模块管理器会负责管理
 * 模块的反射工厂，这样能更好的做到模块隔离，避免模块之间相互影响。
 * 如果后续有需要也可以注册到全局反射工厂，比如某些底层机制想要绕过模块管理体系。
 */
#define MC_EXPORT_MODULE(module_name)                                                          \
    extern "C" {                                                                               \
    void* mc_open_##module_name() {                                                            \
        return mc::reflect::reflection_factory::instance_ptr<module_name##_namespace>().get(); \
    }                                                                                          \
    void mc_close_##module_name() {                                                            \
        /* 空实现：模块的清理由 module_manager 通过引用计数管理 */                             \
    }                                                                                          \
    }
} // namespace mc::module

namespace mc {

namespace module {
/**
 * @brief 加载模块
 */
inline mc::module::module_ptr
load_module(std::string_view module_name) {
    return mc::module::module_manager::instance().require(module_name);
}

inline mc::module::module_manager& get_module_manager() {
    return mc::module::module_manager::instance();
}

} // namespace module

using module::get_module_manager;
using module::load_module;

} // namespace mc

#endif // MC_MODULE_H