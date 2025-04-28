/**
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

#ifndef MC_ENGINE_ERROR_H
#define MC_ENGINE_ERROR_H
#include <mc/common.h>
#include <mc/dict.h>
#include <mc/reflect.h>
#include <mc/singleton.h>
#include <string>
#include <string_view>

namespace mc::engine {

/**
 * @brief 错误信息
 */
struct error_info {
    constexpr explicit error_info(std::string_view name = {}, std::string_view format = {})
        : name(name), format(format) {
    }

    bool operator==(const error_info& other) const {
        return name == other.name && format == other.format;
    }

    bool operator!=(const error_info& other) const {
        return !(*this == other);
    }

    /**
     * @brief 错误名称
     */
    std::string_view name;

    /**
     * @brief 错误格式
     */
    std::string_view format;
};

/**
 * @brief 错误类，用于表示错误信息
 *
 * 该类用于表示错误信息，并提供一些方法来获取错误信息
 * 错误信息由错误名称和格式化字符串组成，可以包含一些参数
 *
 * 错误名称和格式化字符串必须是常量，内部不持有的所有权，这么做的原因是配合错误引擎使用，一般错误名称
 * 和格式化字符串是引擎内部定义的，不需要动态构造
 *
 * 如果需要动态构造错误名或格式化字符串，请使用 error_with_owner 类
 */
class error : public error_info {
public:
    error();
    error(std::string_view name, std::string_view format);

    error(const error& other)                = default;
    error& operator=(const error& other)     = default;
    error(error&& other) noexcept            = default;
    error& operator=(error&& other) noexcept = default;

    std::string_view get_name() const;
    std::string_view get_format() const;
    const mc::dict&  get_args() const;
    std::string      get_message() const;

    void set_name(std::string_view name);
    void set_format(std::string_view format);
    void reset();

    // 添加参数
    template <typename T>
    error& operator()(std::string_view key, T&& value) {
        args[key] = std::forward<T>(value);
        return *this;
    }

    template <typename T>
    error& append_arg(std::string_view key, T&& value) {
        args[key] = std::forward<T>(value);
        return *this;
    }

    error& operator%(mc::dict args) {
        this->args = args;
        return *this;
    }

    error& set_args(const mc::dict& args);

    std::string to_string() const;
    bool        is_set() const;

    bool operator==(const error& other) const;
    bool operator!=(const error& other) const;

    mc::mutable_dict args;
};

// 由于 error 类的错误名字和format字符串是常量，这里提供一个继承类可以持有 name 和 format 的 owner,
// 方便某些需要动态构造错误名或格式化字符串的场景使用
class error_with_owner : public mc::engine::error {
public:
    error_with_owner();
    error_with_owner(std::string name, std::string format);

private:
    std::string m_name_owner;
    std::string m_format_owner;
};

} // namespace mc::engine

MC_REFLECT(mc::engine::error_info, (name)(format))
MC_REFLECT(mc::engine::error, (name)(format)(args))

#endif // MC_ENGINE_ERROR_H
