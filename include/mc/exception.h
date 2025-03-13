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

#include <chrono>
#include <exception>
#include <mc/log/log_message.h>
#include <mc/log/logger.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
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
    unknow_exception_code = 0,             // 未知异常代码
    unhandled_exception_code = 1,          // 未处理的第三方异常
    timeout_exception_code = 2,            // 超时异常
    file_not_found_exception_code = 3,     // 文件未找到异常
    parse_error_exception_code = 4,        // 解析错误异常
    invalid_arg_exception_code = 5,        // 无效参数异常
    key_not_found_exception_code = 6,      // 键未找到异常
    bad_cast_exception_code = 7,           // 类型转换异常
    out_of_range_exception_code = 8,       // 越界异常
    canceled_exception_code = 9,           // 取消操作异常
    assert_exception_code = 10,            // 断言异常
    eof_exception_code = 11,               // 文件结束异常
    system_error_code = 12,                // 标准库异常
    std_exception_code = 13,               // 标准库异常
    invalid_op_exception_code = 14,        // 无效操作异常
    null_optional_code = 15,               // 空可选值异常
    overflow_code = 16,                    // 溢出异常
    underflow_code = 17,                   // 下溢异常
    divide_by_zero_code = 18,              // 除零异常

    // MC库特定异常代码 (从100开始)
    variant_error_code = 100,     // 变体类型异常
    dict_error_code = 101,        // 字典异常
    future_error_code = 102,      // Future异常
    object_error_code = 103,      // 对象异常
    event_error_code = 104,       // 事件异常
    signal_slot_error_code = 105, // 信号槽异常
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
    exception(int64_t code = unknow_exception_code,
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

    // 获取异常名称
    const char* name() const noexcept;

    // 获取异常描述
    virtual const char* what() const noexcept override;

    // 添加日志消息
    void append_log(mc::log::message msg);

    // 获取详细异常信息
    virtual std::string to_detail_string(mc::log::level ll = mc::log::level::all) const;

    // 获取简要异常信息
    virtual std::string to_string(mc::log::level ll = mc::log::level::info) const;

    // 获取顶层异常消息
    std::string top_message() const;

    // 动态重新抛出异常
    virtual void dynamic_rethrow_exception() const;

    // 动态复制异常
    virtual std::shared_ptr<exception> dynamic_copy_exception() const;

    const mc::log::messages &messages() const;

protected:
    // 异常实现细节
    std::unique_ptr<detail::exception_impl> m_impl;
};

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
#define MC_DEFINE_EXCEPTION_CLASS(class_name, code_enum_value, default_msg, class_name_str)        \
    class class_name : public exception {                                                          \
    public:                                                                                        \
        enum code_enum {                                                                           \
            code_value = code_enum_value,                                                          \
        };                                                                                         \
                                                                                                   \
        class_name(mc::log::message&& msg = mc::log::message(mc::log::level::error, default_msg))  \
            : exception(std::move(msg), code_enum_value, class_name_str, default_msg) {            \
        }                                                                                          \
                                                                                                   \
        class_name(const class_name& e) : exception(e) {                                           \
        }                                                                                          \
        class_name(class_name&& e) : exception(std::move(e)) {                                     \
        }                                                                                          \
                                                                                                   \
        /* 从基类构造 */                                                                           \
        explicit class_name(const exception& e)                                                    \
            : exception(code_enum_value, class_name_str, default_msg) {                            \
        }                                                                                          \
                                                                                                   \
        virtual std::shared_ptr<exception> dynamic_copy_exception() const override {               \
            return std::make_shared<class_name>(*this);                                            \
        }                                                                                          \
                                                                                                   \
        virtual void dynamic_rethrow_exception() const override {                                  \
            throw *this;                                                                           \
        }                                                                                          \
    };

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
    // 构造函数
    explicit std_exception_wrapper(mc::log::message&& msg,
                                   std::exception_ptr e = std::current_exception(),
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

/**
 * @brief 异常工厂类
 *
 * 用于注册和创建异常
 */
class exception_factory {
public:
    // 异常构建器基类
    struct base_exception_builder {
        virtual void rethrow(const exception& e) const = 0;
        virtual ~base_exception_builder() = default;
    };

    // 异常构建器模板类
    template <typename T>
    struct exception_builder : public base_exception_builder {
        virtual void rethrow(const exception& e) const override {
            throw T(e);
        }
    };

    // 获取单例实例
    static exception_factory& instance();

    // 注册异常类型
    template <typename T>
    void register_exception() {
        static exception_builder<T> builder;
        m_registered_exceptions[T::code_enum::code_value] = &builder;
    }

    // 重新抛出异常
    void rethrow(const exception& e) const;

private:
    // 私有构造函数（单例模式）
    exception_factory() = default;

    // 注册的异常映射表
    std::unordered_map<int64_t, base_exception_builder*> m_registered_exceptions;
};

/**
 * @brief 注册异常类宏
 *
 * 用于注册异常类到异常工厂
 *
 * @param exception_class 要注册的异常类名
 */
#define MC_REGISTER_EXCEPTION(exception_class)                                                     \
    mc::exception_factory::instance().register_exception<exception_class>()

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
#define MC_DEFINE_CUSTOM_EXCEPTION(class_name, code_enum_value, default_msg, class_name_str)       \
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
#define MC_ASSERT(CONDITION, FORMAT, ...)                                                          \
    do {                                                                                           \
        if (!(CONDITION)) {                                                                        \
            throw MC_MAKE_EXCEPTION(mc::assert_exception, FORMAT, __VA_ARGS__);                \
        }                                                                                          \
    } while (0)

/**
 * @brief 抛出异常宏
 *
 * 抛出指定类型的异常
 */
#define MC_THROW(EXCEPTION_TYPE, FORMAT, ...)                                                      \
    throw MC_MAKE_EXCEPTION(EXCEPTION_TYPE, FORMAT, __VA_ARGS__)

/**
 * @brief 捕获并重新抛出异常宏
 *
 * 捕获异常并添加上下文信息后重新抛出
 */
#define MC_RETHROW_EXCEPTION(EXCEPTION, FORMAT, ...)                                               \
    do {                                                                                           \
        EXCEPTION.append_log(MC_LOG_MESSAGE(error, FORMAT, __VA_ARGS__));                          \
        throw EXCEPTION;                                                                           \
    } while (0)

/**
 * @brief 捕获并包装标准异常宏
 *
 * 捕获标准异常并包装为MC异常
 */
#define MC_CAPTURE_AND_WRAP_EXCEPTION(FORMAT, ...)                                                 \
    catch (const std::exception& e) {                                                              \
        throw mc::std_exception_wrapper(MC_LOG_MESSAGE(error, FORMAT, __VA_ARGS__));               \
    }

} // namespace mc

#endif // MC_EXCEPTION_H