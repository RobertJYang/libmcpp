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

#ifndef MC_EXCEPTION_H
#define MC_EXCEPTION_H

#include <mc/log/log_message.h>

#include <chrono>
#include <exception>
#include <string>

namespace mc {

// 前向声明
namespace detail {
class exception_impl;
}

/**
 * @brief 异常代码枚举
 *
 * 定义MC库中所有可能的异常类型代码
 */
enum exception_code {
    // 通用异常代码
    unknow_exception_code           = 0,  // 未知异常代码
    unhandled_exception_code        = 1,  // 未处理的第三方异常
    timeout_exception_code          = 2,  // 超时异常
    file_not_found_exception_code   = 3,  // 文件未找到异常
    parse_error_exception_code      = 4,  // 解析错误异常
    invalid_arg_exception_code      = 5,  // 无效参数异常
    key_not_found_exception_code    = 6,  // 键未找到异常
    bad_cast_exception_code         = 7,  // 类型转换异常
    out_of_range_exception_code     = 8,  // 越界异常
    canceled_exception_code         = 9,  // 取消操作异常
    assert_exception_code           = 10, // 断言异常
    eof_exception_code              = 11, // 文件结束异常
    system_error_code               = 12, // 系统错误异常
    std_exception_code              = 13, // 标准库异常
    invalid_op_exception_code       = 14, // 无效操作异常
    null_optional_code              = 15, // 空可选值异常
    overflow_code                   = 16, // 溢出异常
    underflow_code                  = 17, // 下溢异常
    divide_by_zero_code             = 18, // 除零异常
    bad_function_call_code          = 19, // 函数调用异常
    bad_alloc_code                  = 20, // 内存分配异常
    busy_exception_code             = 21, // 繁忙异常
    method_call_exception_code      = 22, // 方法调用异常
    not_implemented_exception_code  = 23, // 未实现异常
    bad_property_exception_code     = 24, // 属性错误
    bad_method_exception_code       = 25, // 方法错误
    bad_type_exception_code         = 26, // 类型错误
    not_found_exception_code        = 27, // 未找到异常
    invalid_argument_exception_code = 28, // 无效参数异常
    runtime_exception_code          = 29, // 运行时异常
    format_error_code               = 30, // 格式化错误异常
};

/**
 * @brief MC库异常基类
 *
 * 所有MC库中的异常都应该继承自此类
 */
class exception : public std::exception {
public:
    // 格式化异常信息的时间限制（毫秒）
    static constexpr std::chrono::milliseconds format_time_limit = std::chrono::milliseconds(10);

    // 异常代码枚举
    enum code_enum { code_value = unknow_exception_code };

    /**
     * @brief 构造函数
     *
     * @param code 异常代码
     * @param name_value 异常名称
     * @param what_value 异常描述
     */
    exception(int64_t            code       = unknow_exception_code,
              const std::string& name_value = "unknow_exception",
              const std::string& what_value = "未指定异常");

    /**
     * @brief 带日志消息的构造函数
     *
     * @param msg 日志消息
     * @param code 异常代码
     * @param name_value 异常名称
     * @param what_value 异常描述
     */
    exception(mc::log::message&& msg, int64_t code = unknow_exception_code,
              const std::string& name_value = "unknow_exception",
              const std::string& what_value = "未指定异常");

    /**
     * @brief 带多条日志消息的构造函数
     *
     * @param msgs 日志消息列表
     * @param code 异常代码
     * @param name_value 异常名称
     * @param what_value 异常描述
     */
    exception(mc::log::messages&& msgs, int64_t code = unknow_exception_code,
              const std::string& name_value = "exception",
              const std::string& what_value = "未指定异常");

    /**
     * @brief 带多条日志消息的构造函数（常量引用版本）
     *
     * @param msgs 日志消息列表
     * @param code 异常代码
     * @param name_value 异常名称
     * @param what_value 异常描述
     */
    exception(const mc::log::messages& msgs, int64_t code = unknow_exception_code,
              const std::string& name_value = "exception",
              const std::string& what_value = "未指定异常");

    // 复制构造函数
    exception(const exception& e);

    // 移动构造函数
    exception(exception&& e);

    // 析构函数
    virtual ~exception();

    // 获取异常代码
    int64_t code() const noexcept;

    // 设置异常代码
    void set_code(int64_t code);

    // 获取异常名称
    std::string_view name() const noexcept;

    // 设置异常名称
    void set_name(std::string_view name);

    // 获取异常描述
    virtual const char* what() const noexcept override;

    // 添加日志消息
    void append_log(mc::log::message msg) const;

    // 添加日志消息
    void append_log(mc::log::messages msgs) const;

    // 获取详细异常信息
    virtual std::string to_detail_string(mc::log::level ll = mc::log::level::all) const;

    // 获取简要异常信息
    virtual std::string to_string(mc::log::level ll = mc::log::level::info) const;

    // 获取顶层异常消息
    const std::string& top_message() const;

    // 动态重新抛出异常
    virtual void dynamic_rethrow_exception() const;

    // 动态复制异常
    virtual std::shared_ptr<exception> dynamic_copy_exception() const;

    const mc::log::messages& messages() const;

    mc::log::messages take_messages() const;

protected:
    // 异常实现细节
    std::unique_ptr<detail::exception_impl> m_impl;
};

using exception_ptr = std::shared_ptr<exception>;

/**
 * @brief 统一的异常类定义宏
 *
 * 同时完成异常类的声明和实现
 *
 * @param class_name 异常类名
 * @param code_enum_value 异常代码枚举值
 * @param default_msg 默认异常消息
 * @param class_name_str 异常类名字符串
 *
 * @example
 * // 在头文件中定义异常类
 * MC_DEFINE_EXCEPTION_CLASS(timeout_exception, timeout_exception_code, "操作超时", "timeout")
 *
 * // 在代码中使用
 * MC_THROW(timeout_exception, "连接超时");
 */
#define MC_DEFINE_EXCEPTION_CLASS_BASE(class_name, code_enum_value, default_msg, class_name_str,  \
                                       base_class)                                                \
    class class_name : public base_class {                                                        \
    public:                                                                                       \
        enum code_enum { code_value = code_enum_value };                                          \
                                                                                                  \
        class_name(mc::log::message&& msg = mc::log::message(mc::log::level::error, default_msg)) \
            : base_class(std::move(msg), code_enum_value, class_name_str, default_msg) {          \
        }                                                                                         \
                                                                                                  \
        class_name(const class_name& e) : base_class(e) {                                         \
        }                                                                                         \
        class_name(class_name&& e) : base_class(std::move(e)) {                                   \
        }                                                                                         \
                                                                                                  \
        /* 从基类构造 */                                                                          \
        explicit class_name(const base_class& e)                                                  \
            : base_class(code_enum_value, class_name_str, default_msg) {                          \
        }                                                                                         \
                                                                                                  \
        virtual std::shared_ptr<mc::exception> dynamic_copy_exception() const override {          \
            return std::make_shared<class_name>(*this);                                           \
        }                                                                                         \
                                                                                                  \
        virtual void dynamic_rethrow_exception() const override {                                 \
            throw *this;                                                                          \
        }                                                                                         \
    };

#define MC_DEFINE_EXCEPTION_CLASS(class_name, code_enum_value, default_msg, class_name_str)  \
    MC_DEFINE_EXCEPTION_CLASS_BASE(class_name, code_enum_value, default_msg, class_name_str, \
                                   exception)

/**
 * @brief 未处理异常包装类
 *
 * 用于包装未处理的第三方异常
 */
class unhandled_exception : public exception {
public:
    enum code_enum {
        code_value = unhandled_exception_code,
    };

    // 构造函数
    unhandled_exception(mc::log::message&& msg, std::exception_ptr e = std::current_exception());
    unhandled_exception(mc::log::messages msgs);
    unhandled_exception(const exception& e);

    // 获取内部异常
    std::exception_ptr get_inner_exception() const;

    // 动态重新抛出异常
    virtual void dynamic_rethrow_exception() const override;

    // 动态复制异常
    virtual std::shared_ptr<exception> dynamic_copy_exception() const override;

private:
    std::exception_ptr m_inner;
};

/**
 * @brief 标准异常包装类
 *
 * 用于包装标准库异常
 */
class std_exception_wrapper : public exception {
public:
    enum code_enum {
        code_value = std_exception_code,
    };

    // 构造函数
    explicit std_exception_wrapper(mc::log::message&& msg,
                                   std::exception_ptr e          = std::current_exception(),
                                   const std::string& name_value = "exception",
                                   const std::string& what_value = "未指定异常");

    // 获取内部异常
    std::exception_ptr get_inner_exception() const;

    // 从当前异常创建包装器
    static std_exception_wrapper from_current_exception(const std::exception& e);

    // 获取详细异常信息
    std::string to_detail_string(mc::log::level ll = mc::log::level::all) const override;

    // 动态重新抛出异常
    virtual void dynamic_rethrow_exception() const override;

    // 动态复制异常
    virtual std::shared_ptr<exception> dynamic_copy_exception() const override;

private:
    std::exception_ptr m_inner;
};

// 常用异常类定义
MC_DEFINE_EXCEPTION_CLASS(timeout_exception, timeout_exception_code, "操作超时", "timeout")
MC_DEFINE_EXCEPTION_CLASS(file_not_found_exception, file_not_found_exception_code, "文件未找到",
                          "file_not_found")
MC_DEFINE_EXCEPTION_CLASS(parse_error_exception, parse_error_exception_code, "解析错误",
                          "parse_error")
MC_DEFINE_EXCEPTION_CLASS(invalid_arg_exception, invalid_arg_exception_code, "无效参数",
                          "invalid_arg")
MC_DEFINE_EXCEPTION_CLASS(key_not_found_exception, key_not_found_exception_code, "键未找到",
                          "key_not_found")
MC_DEFINE_EXCEPTION_CLASS(assert_exception, assert_exception_code, "断言失败", "assert")
MC_DEFINE_EXCEPTION_CLASS(bad_cast_exception, bad_cast_exception_code, "类型转换错误", "bad_cast")
MC_DEFINE_EXCEPTION_CLASS(out_of_range_exception, out_of_range_exception_code, "索引越界",
                          "out_of_range")
MC_DEFINE_EXCEPTION_CLASS(canceled_exception, canceled_exception_code, "操作已取消", "canceled")
MC_DEFINE_EXCEPTION_CLASS(eof_exception, eof_exception_code, "文件结束", "eof")
MC_DEFINE_EXCEPTION_CLASS(system_exception, system_error_code, "系统错误", "system")
MC_DEFINE_EXCEPTION_CLASS(invalid_op_exception, invalid_op_exception_code, "无效操作",
                          "invalid_operation")
MC_DEFINE_EXCEPTION_CLASS(null_optional_exception, null_optional_code, "访问空可选值",
                          "null_optional")
MC_DEFINE_EXCEPTION_CLASS(overflow_exception, overflow_code, "数值溢出", "overflow")
MC_DEFINE_EXCEPTION_CLASS(underflow_exception, underflow_code, "数值下溢", "underflow")
MC_DEFINE_EXCEPTION_CLASS(divide_by_zero_exception, divide_by_zero_code, "除零错误",
                          "divide_by_zero")
MC_DEFINE_EXCEPTION_CLASS(file_open_exception, file_not_found_exception_code, "无法打开文件",
                          "file_open")
MC_DEFINE_EXCEPTION_CLASS(not_implemented_exception, not_implemented_exception_code, "未实现",
                          "not_implemented")
MC_DEFINE_EXCEPTION_CLASS(bad_function_call_exception, bad_function_call_code, "函数调用错误",
                          "bad_function_call")
MC_DEFINE_EXCEPTION_CLASS(bad_alloc_exception, bad_alloc_code, "内存分配错误", "bad_alloc")
MC_DEFINE_EXCEPTION_CLASS(busy_exception, busy_exception_code, "繁忙", "busy")
MC_DEFINE_EXCEPTION_CLASS(method_call_exception, method_call_exception_code, "方法调用错误",
                          "method_call")
MC_DEFINE_EXCEPTION_CLASS(bad_property_exception, bad_property_exception_code, "属性错误", "bad_property")
MC_DEFINE_EXCEPTION_CLASS(bad_method_exception, bad_method_exception_code, "方法错误", "bad_method")
MC_DEFINE_EXCEPTION_CLASS(bad_type_exception, bad_type_exception_code, "类型错误", "bad_type")

// 未找到异常
MC_DEFINE_EXCEPTION_CLASS(not_found_exception, not_found_exception_code, "未找到", "not_found")

// 无效参数异常
MC_DEFINE_EXCEPTION_CLASS(invalid_argument_exception, invalid_argument_exception_code, "无效参数", "invalid_argument")

// 运行时异常
MC_DEFINE_EXCEPTION_CLASS(runtime_exception, runtime_exception_code, "运行时错误", "runtime")

// 格式化错误异常类
MC_DEFINE_EXCEPTION_CLASS(format_error, format_error_code, "格式化错误", "format_error")

/**
 * @brief 完整自定义异常类宏（MC_DEFINE_EXCEPTION_CLASS 的别名）
 *
 * 同时创建异常类的声明和实现
 *
 * @param class_name 异常类名
 * @param code_enum_value 异常代码枚举值
 * @param default_msg 默认异常消息
 * @param class_name_str 异常类名字符串
 *
 * @example
 * // 在头文件中定义异常类
 * MC_DEFINE_CUSTOM_EXCEPTION(network_error, 200, "网络连接错误", "network_error")
 *
 * // 抛出异常
 * MC_THROW(network_error, "连接超时");
 *
 * // 捕获异常
 * try {
 *     // 可能抛出异常的代码
 * } catch (const network_error& e) {
 *     std::cout << e.to_string() << std::endl;
 * }
 */
#define MC_DEFINE_CUSTOM_EXCEPTION(class_name, code_enum_value, default_msg, class_name_str) \
    MC_DEFINE_EXCEPTION_CLASS(class_name, code_enum_value, default_msg, class_name_str)

// 宏定义

/**
 * @brief 构造异常宏
 *
 * 构造指定类型的异常
 */
#define MC_MAKE_EXCEPTION(EXCEPTION, FORMAT, ...) \
    EXCEPTION(MC_LOG_MESSAGE(error, FORMAT, __VA_ARGS__))

/**
 * @brief 断言宏
 *
 * 如果条件为假，则抛出断言异常
 */
#define MC_ASSERT(CONDITION, FORMAT, ...)                                       \
    do {                                                                        \
        if (!(CONDITION)) {                                                     \
            throw MC_MAKE_EXCEPTION(mc::assert_exception, FORMAT, __VA_ARGS__); \
        }                                                                       \
    } while (0)

/**
 * @brief 断言宏
 *
 * 如果条件为假，则抛出指定异常
 */
#define MC_ASSERT_THROW(CONDITION, EXCEPTION, FORMAT, ...)           \
    do {                                                             \
        if (!(CONDITION)) {                                          \
            throw MC_MAKE_EXCEPTION(EXCEPTION, FORMAT, __VA_ARGS__); \
        }                                                            \
    } while (0)

/**
 * @brief 抛出异常宏
 *
 * 抛出指定类型的异常
 */
#define MC_THROW(EXCEPTION_TYPE, FORMAT, ...) \
    throw MC_MAKE_EXCEPTION(EXCEPTION_TYPE, FORMAT, __VA_ARGS__)

/**
 * @brief 捕获并重新抛出异常宏
 *
 * 捕获异常并添加上下文信息后重新抛出
 */
#define MC_RETHROW_EXCEPTION(EXCEPTION, FORMAT, ...)                      \
    do {                                                                  \
        EXCEPTION.append_log(MC_LOG_MESSAGE(error, FORMAT, __VA_ARGS__)); \
        throw EXCEPTION;                                                  \
    } while (0)

/**
 * @brief 捕获并包装标准异常宏
 *
 * 捕获标准异常并包装为MC异常
 */
#define MC_CAPTURE_AND_WRAP_EXCEPTION(FORMAT, ...)                                   \
    catch (const std::exception& e) {                                                \
        throw mc::std_exception_wrapper(MC_LOG_MESSAGE(error, FORMAT, __VA_ARGS__)); \
    }

} // namespace mc

#endif // MC_EXCEPTION_H