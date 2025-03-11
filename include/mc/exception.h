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

#include <exception>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>

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
    general_error_code               = 0,  // 未指定异常代码
    external_error_code              = 1,  // 未处理的第三方异常
    time_limit_error_code            = 2,  // 超时异常
    missing_file_error_code          = 3,  // 文件未找到异常
    syntax_error_code                = 4,  // 解析错误异常
    argument_error_code              = 5,  // 无效参数异常
    missing_key_error_code           = 6,  // 键未找到异常
    type_error_code                  = 7,  // 类型转换异常
    range_error_code                 = 8,  // 越界异常
    abort_error_code                 = 9,  // 取消操作异常
    condition_error_code             = 10, // 断言异常
    stream_end_error_code            = 11, // 文件结束异常
    system_error_code                = 12, // 标准库异常
    operation_error_code             = 13, // 无效操作异常
    empty_value_error_code           = 14, // 空可选值异常
    numeric_overflow_error_code      = 15, // 溢出异常
    numeric_underflow_error_code     = 16, // 下溢异常
    division_error_code              = 17, // 除零异常
    
    // MC库特定异常代码 (从100开始)
    variant_error_code               = 100, // 变体类型异常
    dict_error_code                  = 101, // 字典异常
    future_error_code                = 102, // Future异常
    object_error_code                = 103, // 对象异常
    event_error_code                 = 104, // 事件异常
    signal_slot_error_code           = 105, // 信号槽异常
};

/**
 * @brief 日志级别枚举
 */
enum class log_level {
    all,    // 所有日志
    debug,  // 调试日志
    info,   // 信息日志
    warn,   // 警告日志
    error,  // 错误日志
    off     // 关闭日志
};

/**
 * @brief 日志消息结构
 */
struct log_message {
    log_level                    m_level;
    std::string                  m_message;
    std::string                  m_file;
    uint32_t                     m_line;
    std::chrono::system_clock::time_point m_timestamp;
    
    log_message(log_level level = log_level::info,
                std::string message = "",
                std::string file = "",
                uint32_t line = 0)
        : m_level(level), m_message(std::move(message)), 
          m_file(std::move(file)), m_line(line),
          m_timestamp(std::chrono::system_clock::now()) {}
};

using log_messages = std::vector<log_message>;

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
    enum code_enum {
        code_value = general_error_code
    };
    
    /**
     * @brief 构造函数
     * 
     * @param code 异常代码
     * @param name_value 异常名称
     * @param what_value 异常描述
     */
    exception(int64_t code = general_error_code,
              const std::string& name_value = "exception",
              const std::string& what_value = "未指定异常");
    
    /**
     * @brief 带日志消息的构造函数
     * 
     * @param msg 日志消息
     * @param code 异常代码
     * @param name_value 异常名称
     * @param what_value 异常描述
     */
    exception(log_message&& msg,
              int64_t code = general_error_code,
              const std::string& name_value = "exception",
              const std::string& what_value = "未指定异常");
    
    /**
     * @brief 带多条日志消息的构造函数
     * 
     * @param msgs 日志消息列表
     * @param code 异常代码
     * @param name_value 异常名称
     * @param what_value 异常描述
     */
    exception(log_messages&& msgs,
              int64_t code = general_error_code,
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
    exception(const log_messages& msgs,
              int64_t code = general_error_code,
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
    void append_log(log_message msg);
    
    // 获取详细异常信息
    virtual std::string to_detail_string(log_level ll = log_level::all) const;
    
    // 获取简要异常信息
    virtual std::string to_string(log_level ll = log_level::info) const;
    
    // 获取顶层异常消息
    std::string top_message() const;
    
    // 动态重新抛出异常
    virtual void dynamic_rethrow_exception() const;
    
    // 动态复制异常
    virtual std::shared_ptr<exception> dynamic_copy_exception() const;
    
protected:
    // 异常实现细节
    std::unique_ptr<detail::exception_impl> m_impl;
};

/**
 * @brief 未处理异常包装类
 * 
 * 用于包装未处理的第三方异常
 */
class unhandled_exception : public exception {
public:
    enum code_enum {
        code_value = external_error_code,
    };
    
    // 构造函数
    unhandled_exception(log_message&& msg, std::exception_ptr e = std::current_exception());
    unhandled_exception(log_messages msgs);
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
    explicit std_exception_wrapper(
        log_message&& msg,
        std::exception_ptr e = std::current_exception(),
        const std::string& name_value = "exception",
        const std::string& what_value = "未指定异常");
    
    // 获取内部异常
    std::exception_ptr get_inner_exception() const;
    
    // 从当前异常创建包装器
    static std_exception_wrapper from_current_exception(const std::exception& e);
    
    // 获取详细异常信息
    std::string to_detail_string(log_level ll = log_level::all) const override;
    
    // 动态重新抛出异常
    virtual void dynamic_rethrow_exception() const override;
    
    // 动态复制异常
    virtual std::shared_ptr<exception> dynamic_copy_exception() const override;
    
private:
    std::exception_ptr m_inner;
};

/**
 * @brief 复制异常
 * 
 * @tparam T 异常类型
 * @param e 异常对象
 * @return std::shared_ptr<exception> 异常指针
 */
template<typename T>
std::shared_ptr<exception> copy_exception(T&& e) {
    return std::make_shared<std::decay_t<T>>(std::forward<T>(e));
}

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
    template<typename T>
    struct exception_builder : public base_exception_builder {
        virtual void rethrow(const exception& e) const override {
            throw T(e);
        }
    };
    
    // 获取单例实例
    static exception_factory& instance();
    
    // 注册异常类型
    template<typename T>
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

// 辅助函数

/**
 * @brief 获取当前异常的字符串表示
 * 
 * @return std::string 异常字符串
 */
std::string except_str();

/**
 * @brief 记录断言触发
 * 
 * @param filename 文件名
 * @param lineno 行号
 * @param expr 表达式
 */
void record_assert_trip(
    const char* filename,
    uint32_t lineno,
    const char* expr
);

// 常用异常类定义

/**
 * @brief 超时异常
 */
class timeout_exception : public exception {
public:
    enum code_enum {
        code_value = time_limit_error_code,
    };
    
    timeout_exception(log_message&& msg = log_message(log_level::error, "操作超时"));
    timeout_exception(const timeout_exception& e);
    timeout_exception(timeout_exception&& e);
    
    // 从基类构造
    explicit timeout_exception(const exception& e);
    
    virtual std::shared_ptr<exception> dynamic_copy_exception() const override;
    virtual void dynamic_rethrow_exception() const override;
};

/**
 * @brief 文件未找到异常
 */
class file_not_found_exception : public exception {
public:
    enum code_enum {
        code_value = missing_file_error_code,
    };
    
    file_not_found_exception(log_message&& msg = log_message(log_level::error, "文件未找到"));
    file_not_found_exception(const file_not_found_exception& e);
    file_not_found_exception(file_not_found_exception&& e);
    
    // 从基类构造
    explicit file_not_found_exception(const exception& e);
    
    virtual std::shared_ptr<exception> dynamic_copy_exception() const override;
    virtual void dynamic_rethrow_exception() const override;
};

/**
 * @brief 解析错误异常
 */
class parse_error_exception : public exception {
public:
    enum code_enum {
        code_value = syntax_error_code,
    };
    
    parse_error_exception(log_message&& msg = log_message(log_level::error, "解析错误"));
    parse_error_exception(const parse_error_exception& e);
    parse_error_exception(parse_error_exception&& e);
    
    // 从基类构造
    explicit parse_error_exception(const exception& e);
    
    virtual std::shared_ptr<exception> dynamic_copy_exception() const override;
    virtual void dynamic_rethrow_exception() const override;
};

/**
 * @brief 无效参数异常
 */
class invalid_arg_exception : public exception {
public:
    enum code_enum {
        code_value = argument_error_code,
    };
    
    invalid_arg_exception(log_message&& msg = log_message(log_level::error, "无效参数"));
    invalid_arg_exception(const invalid_arg_exception& e);
    invalid_arg_exception(invalid_arg_exception&& e);
    
    // 从基类构造
    explicit invalid_arg_exception(const exception& e);
    
    virtual std::shared_ptr<exception> dynamic_copy_exception() const override;
    virtual void dynamic_rethrow_exception() const override;
};

/**
 * @brief 键未找到异常
 */
class key_not_found_exception : public exception {
public:
    enum code_enum {
        code_value = missing_key_error_code,
    };
    
    key_not_found_exception(log_message&& msg = log_message(log_level::error, "键未找到"));
    key_not_found_exception(const key_not_found_exception& e);
    key_not_found_exception(key_not_found_exception&& e);
    
    // 从基类构造
    explicit key_not_found_exception(const exception& e);
    
    virtual std::shared_ptr<exception> dynamic_copy_exception() const override;
    virtual void dynamic_rethrow_exception() const override;
};

/**
 * @brief 断言异常
 */
class assert_exception : public exception {
public:
    enum code_enum {
        code_value = condition_error_code,
    };
    
    assert_exception(log_message&& msg = log_message(log_level::error, "断言失败"));
    assert_exception(const assert_exception& e);
    assert_exception(assert_exception&& e);
    
    // 从基类构造
    explicit assert_exception(const exception& e);
    
    virtual std::shared_ptr<exception> dynamic_copy_exception() const override;
    virtual void dynamic_rethrow_exception() const override;
};

// 宏定义

/**
 * @brief 断言宏
 * 
 * 如果条件为假，则抛出断言异常
 */
#define MC_ASSERT(CONDITION, MSG) \
    do { \
        if (!(CONDITION)) { \
            mc::record_assert_trip(__FILE__, __LINE__, #CONDITION); \
            throw mc::assert_exception(mc::log_message(mc::log_level::error, MSG, __FILE__, __LINE__)); \
        } \
    } while(0)

/**
 * @brief 抛出异常宏
 * 
 * 抛出指定类型的异常
 */
#define MC_THROW(EXCEPTION_TYPE, MSG) \
    throw EXCEPTION_TYPE(mc::log_message(mc::log_level::error, MSG, __FILE__, __LINE__))

/**
 * @brief 捕获并重新抛出异常宏
 * 
 * 捕获异常并添加上下文信息后重新抛出
 */
#define MC_RETHROW_EXCEPTION(EXCEPTION, MSG) \
    do { \
        EXCEPTION.append_log(mc::log_message(mc::log_level::error, MSG, __FILE__, __LINE__)); \
        throw EXCEPTION; \
    } while(0)

/**
 * @brief 捕获并包装标准异常宏
 * 
 * 捕获标准异常并包装为MC异常
 */
#define MC_CAPTURE_AND_WRAP_EXCEPTION(MSG) \
    catch (const std::exception& e) { \
        throw mc::std_exception_wrapper( \
            mc::log_message(mc::log_level::error, std::string(MSG) + ": " + e.what(), __FILE__, __LINE__) \
        ); \
    }

} // namespace mc

#endif // MC_EXCEPTION_H 