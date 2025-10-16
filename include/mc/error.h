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

#ifndef MC_ERROR_H
#define MC_ERROR_H

#include <mc/common.h>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/log/log_message.h>
#include <mc/memory.h>
#include <mc/reflect/base.h>
#include <mc/singleton.h>
#include <mc/string.h>

#include <optional>

namespace mc {
using error_level = mc::log::level;

/**
 * @brief 错误信息
 */
struct error_info {
    MC_REFLECTABLE("mc.error_info");

    error_info()                                       = default;
    error_info(const error_info& other)                = default;
    error_info& operator=(const error_info& other)     = default;
    error_info(error_info&& other) noexcept            = default;
    error_info& operator=(error_info&& other) noexcept = default;

    constexpr explicit error_info(std::string_view name, std::string_view format = {},
                                  error_level level = error_level::error)
        : name(name), format(format), level(level) {
    }

    bool operator==(const error_info& other) const {
        return name == other.name && format == other.format;
    }

    bool operator!=(const error_info& other) const {
        return !(*this == other);
    }

    bool is_valid() {
        return !name.empty();
    }

    /**
     * @brief 错误名称
     */
    std::string_view name;

    /**
     * @brief 错误格式
     */
    std::string_view format;

    /**
     * @brief 错误级别
     */
    error_level level = error_level::error;
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
struct MC_API error : public mc::enable_shared_from_this<error>, public error_info {
    MC_REFLECTABLE("mc.error");

    error();
    error(const error_info& info);
    error(std::string_view name, std::string_view format,
          error_level level = error_level::error);

    error(const error& other);
    error& operator=(const error& other);
    error(error&& other) noexcept;
    error& operator=(error&& other) noexcept;

    std::string_view get_name() const;
    std::string_view get_format() const;
    const mc::dict&  get_args() const;
    std::string      get_message() const;

    error_level get_level() const;
    void        set_level(error_level level);

    void set_name(std::string_view name);
    void set_format(std::string_view format);
    void reset();

    void set_prev_error(mc::shared_ptr<error> other);

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
    bool        has_error(std::string_view name) const;

    bool operator==(const error& other) const;
    bool operator!=(const error& other) const;

    mc::log::message to_log_message() const;

    static mc::shared_ptr<error> from_exception(std::exception_ptr e);
    static mc::shared_ptr<error> from_exception(const mc::exception& e);
    static mc::shared_ptr<error> from_exception(const std::exception& e);

    void to_exception(mc::exception& e) const;

    // 错误参数
    mc::dict args;

    /**
     * @brief 前一个错误
     */
    mc::shared_ptr<error> prev_error;
};

using error_ptr = mc::shared_ptr<error>;

// 由于 error 类的错误名字和format字符串是常量，这里提供一个继承类可以持有 name 和 format 的 owner,
// 方便某些需要动态构造错误名或格式化字符串的场景使用
class MC_API error_with_owner : public error {
public:
    error_with_owner();
    error_with_owner(std::string name, std::string format);

private:
    std::string m_name_owner;
    std::string m_format_owner;
};

/*------------------- 一些辅助函数 -------------------*/
/*
 * 检查错误名称是否有效
 *
 * @param name 错误名称
 * @return 如果有效返回 true，否则返回 false
 */
MC_API bool is_valid_error_name(std::string_view name);

/**
 * @brief 解析 format 字符串，找到 ${name} 格式的占位符，并将其添加到 arg_names 中
 *
 * @param format 格式化字符串
 * @param arg_names 存储占位符名称的字典
 * @return 如果解析成功返回 true，否则返回 false
 */
MC_API bool get_error_format_args(std::string_view format, mc::dict& arg_names);

/*
 * 创建错误，如果 name 不满足要求抛出错误
 *
 * @param name 错误名称
 * @param format 格式化字符串
 * @return 创建的错误
 *
 * 注意：这个方法可以创建任意错误，并不要求错误必须注册到错误引擎中
 */
MC_API error_ptr make_error(std::string_view name, std::string_view format);
MC_API error_ptr make_error(const error_info& info);

} // namespace mc

#endif // MC_ERROR_H
