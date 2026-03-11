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

#include <mc/error.h>
#include <mc/error_engine.h>
#include <mc/error_message_parser.h>
#include <mc/exception.h>
#include <mc/fmt/format_dict.h>
#include <mc/json.h>
#include <mc/log.h>
#include <mc/string.h>

#include <algorithm>
#include <cxxabi.h>
#include <execinfo.h>
#include <regex>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace mc {

error::error() = default;

error::error(const error_info& info)
    : mc::enable_shared_from_this<error>(), error_info(info)
{
}

error::error(std::string_view name, std::string_view format, error_level level)
    : mc::enable_shared_from_this<error>(), error_info(name, format, level)
{
}

error::error(const error& other)
    : mc::enable_shared_from_this<error>(other),
      error_info(other.name, other.format, other.level)
{
    this->args = other.args;
    // 不复制缓存，让新对象懒加载

    if (other.prev_error) {
        this->prev_error.reset(new error(*other.prev_error));
    }
}

error_ptr error::from_exception(std::exception_ptr e)
{
    try {
        std::rethrow_exception(e);
    } catch (mc::error_exception& e) {
        return from_exception(e);
    } catch (mc::exception& e) {
        return from_exception(e);
    } catch (std::exception& e) {
        return from_exception(e);
    } catch (...) {
        return from_exception(mc::exception());
    }
}

error_ptr error::from_exception(const mc::exception& e)
{
    auto err = mc::make_shared<error>();

    err->set_name(e.name());
    auto& msgs = e.messages();
    if (!msgs.empty()) {
        auto& msg = msgs.back();
        err->set_format(msg.get_format_template());
        err->set_level(msg.get_level());
        err->set_args(msg.get_args());
    }

    return err;
}

error_ptr error::from_exception(const mc::error_exception& e)
{
    auto err = mc::make_shared<error>();

    err->set_name(e.name());
    err->set_args(e.args());

    return err;
}

error_ptr error::from_exception(const std::exception& e)
{
    return from_exception(mc::std_exception_wrapper::from_current_exception(e));
}

void error::to_exception(mc::exception& e) const
{
    e.set_name(this->name);
    e.append_log(this->to_log_message());
}

error& error::operator=(const error& other)
{
    if (this != &other) {
        mc::enable_shared_from_this<error>::operator=(other);

        this->name            = other.name;
        this->format          = other.format;
        this->level           = other.level;
        this->args            = other.args;
        this->registry_prefix = other.registry_prefix;
        this->m_cached_message.reset(); // 清除缓存

        if (other.prev_error) {
            this->prev_error.reset(new error(*other.prev_error));
        }
    }

    return *this;
}

std::string_view error::get_name() const
{
    return this->name;
}

std::string_view error::get_format() const
{
    return this->format;
}

const mc::dict& error::get_args() const
{
    return args;
}

const mc::dict& error::get_args_with_index() const
{
    return m_args_with_index;
}

std::string error::get_message() const
{
    // 懒加载：如果缓存中有格式化消息，直接返回
    if (m_cached_message.has_value()) {
        return m_cached_message.value();
    }

    // 获取 format：如果为空，尝试从错误引擎查找
    std::string_view format_to_use = this->format;
    bool             from_registry = false;
    if (format_to_use.empty() && !this->name.empty()) {
        auto info = mc::error_engine::get_instance().get_error_info(this->name);
        if (info.is_valid()) {
            format_to_use = info.format;
            from_registry = true;
        }
    }

    if (format_to_use.empty()) {
        return {};
    }

    // 首次访问时格式化消息并缓存
    std::string formatted;
    if (from_registry) {
        // 从错误引擎查找到的，使用 error_message_parser 格式化（支持 %1, %2 等占位符）
        formatted = mc::error_message_parser::format_message(format_to_use, args);
    } else {
        // 直接设置的 format，使用 format_dict 格式化
        formatted = mc::format_dict(format_to_use, args);
    }
    m_cached_message = formatted;
    return formatted;
}

error_level error::get_level() const
{
    return this->level;
}

void error::set_level(error_level level)
{
    this->level = level;
}

void error::set_name(std::string_view name)
{
    this->name = name;
}

void error::set_format(std::string_view format)
{
    this->format = format;
}

const std::string& error::get_registry_prefix() const
{
    return this->registry_prefix;
}

void error::set_registry_prefix(std::string_view prefix)
{
    this->registry_prefix = prefix;
}

void error::set_prev_error(error_ptr other)
{
    this->prev_error = std::move(other);
}

void error::reset()
{
    this->name   = {};
    this->format = {};
    this->args.clear();
    this->prev_error = nullptr;
    this->m_cached_message.reset();
}

error& error::set_args(const mc::dict& args)
{
    this->args = args;
    // 清除缓存，因为参数已更改
    m_cached_message.reset();
    return *this;
}

std::string error::to_string() const
{
    std::ostringstream oss;

    // 输出错误名称和消息
    oss << get_name() << ": " << get_message();

    // 如果有调用栈信息，追加显示
    if (!m_traceback.empty()) {
        oss << "\n"
            << m_traceback;
    }

    return oss.str();
}

std::string error::to_string_format_inplace() const
{
    mc::dict error_data;
    error_data["name"]   = this->name;
    error_data["format"] = get_message();
    return error_data.to_string();
}

bool error::is_set() const
{
    if (!this->name.empty()) {
        return true;
    }

    if (this->prev_error) {
        return this->prev_error->is_set();
    }

    return false;
}

bool error::has_error(std::string_view name) const
{
    if (this->name == name) {
        return true;
    }

    if (this->prev_error) {
        return this->prev_error->has_error(name);
    }

    return false;
}

bool error::operator==(const error& other) const
{
    return this->name == other.name && this->format == other.format && this->args == other.args;
}

bool error::operator!=(const error& other) const
{
    return !(*this == other);
}

mc::log::message error::to_log_message() const
{
    return mc::log::message(
        this->level,
        mc::log::context("", std::string(this->name), 0),
        std::string(this->format),
        this->args);
}

error_with_owner::error_with_owner()
{
}

error_with_owner::error_with_owner(std::string name, std::string format)
    : m_name_owner(std::move(name)), m_format_owner(std::move(format))
{
    this->name   = m_name_owner;
    this->format = m_format_owner;
}

bool get_error_format_args(std::string_view format, mc::dict& arg_names)
{
    return mc::fmt::get_format_args(format, arg_names);
}

// ============================================================================
// 新增辅助函数实现
// ============================================================================

/**
 * @brief 查找参数位置索引(string版本)
 */
int error::get_param_index(std::string_view param_name, std::string_view param_value)
{
    if (param_name == param_value) {
        return 0;
    }
    return -1;
}

/**
 * @brief 查找参数位置索引(dict版本)
 */
int error::get_param_index(std::string_view param_name, const mc::dict& param_struct)
{
    if (param_name.empty()) {
        return -1;
    }

    // 遍历查找匹配的键
    int index = 0;
    for (const auto& entry : param_struct) {
        if (entry.key.is_string()) {
            std::string key = entry.key.get_string();
            if (param_name == key) {
                return index;
            }
            index++;
        }
    }

    return -1;
}

/**
 * @brief 参数名映射为位置索引(string版本)
 */
void error::post_process(const std::string& param_struct)
{
    post_process_impl([&param_struct](std::string_view param_name) {
        return get_param_index(param_name, param_struct);
    });
}

/**
 * @brief 参数名映射为位置索引(dict版本)
 */
void error::post_process(const mc::dict& param_struct)
{
    post_process_impl([&param_struct](std::string_view param_name) {
        return get_param_index(param_name, param_struct);
    });
}

/**
 * @brief post_process 通用实现 (模板函数)
 */
template <typename IndexLookupFunc>
void error::post_process_impl(IndexLookupFunc&& index_lookup)
{
    if (args.empty()) {
        return;
    }

    // 使用正则表达式提取 %参数名: 格式
    static const std::regex param_pattern("^%([^:[]+)");

    mc::dict new_args;
    mc::dict args_with_index;
    int      index = 0;

    // 遍历 args 的值
    for (const auto& entry : args) {
        const mc::variant& value = entry.value;
        const mc::variant& key   = entry.key;

        if (value.is_string()) {
            std::string arg_value = value.get_string();
            std::smatch match;
            try {
                if (std::regex_search(arg_value, match, param_pattern) && match.size() > 1) {
                    // 提取参数名 (第一个捕获组)
                    std::string param_name = match.str(1);
                    int         pos        = index_lookup(param_name);
                    if (pos >= 0) {
                        // 更新 args: 提取冒号后的内容，或者去掉%
                        size_t colon_pos = arg_value.find(':');
                        if (colon_pos != std::string::npos) {
                            new_args[key] = arg_value.substr(colon_pos + 1);
                        } else {
                            // 去掉所有的 %
                            std::string cleaned = arg_value;
                            cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '%'), cleaned.end());
                            new_args[key] = cleaned;
                        }
                        // 创建 args_with_index: 将参数名替换为位置索引
                        std::string new_format = "%" + std::to_string(pos + 1); // 位置索引从1开始
                        args_with_index[index] = new_format;
                        index++;
                        continue;
                    }
                }
            } catch (const std::regex_error&) {
                // 正则表达式错误,跳过
            }
        }
        // 如果没有匹配或不是字符串，保留原值
        new_args[key]          = value;
        args_with_index[index] = value;
        index++;
    }

    // 更新 args
    args              = new_args;
    m_args_with_index = args_with_index;
}

/**
 * @brief 序列化为 JSON
 */
std::string error::encode(const mc::json::json_encode_options& options) const
{
    mc::dict result;

    // 序列化 name, message, args, format
    result["name"]    = std::string(get_name());
    result["message"] = get_message();
    result["format"]  = std::string(get_format());
    result["params"]  = mc::json::json_encode(get_args(), options);

    // 序列化 registry_prefix (如果存在)
    if (!registry_prefix.empty()) {
        result["registry_prefix"] = registry_prefix;
    }

    // 序列化 traceback (如果存在)
    if (!m_traceback.empty()) {
        result["traceback"] = m_traceback;
    }

    // 序列化 args_with_index (如果存在)
    if (!m_args_with_index.empty()) {
        result["args_with_index"] = mc::json::json_encode(m_args_with_index, options);
    }

    return mc::json::json_encode(result, options);
}

/**
 * @brief 从 JSON 反序列化(静态方法)
 */
mc::shared_ptr<error> error::decode(std::string_view                     json,
                                    const mc::json::json_decode_options& options)
{
    // 解析 JSON 字符串
    mc::variant var = mc::json::json_decode(json, options);

    // 验证返回值为 dict 类型
    if (var.get_type() != mc::type_id::object_type) {
        MC_THROW(mc::parse_error_exception, "JSON parse result must be dict type");
    }

    mc::dict data = var.as<mc::dict>();

    // 验证必填字段
    if (!data.contains("name") || !data.contains("message")) {
        MC_THROW(mc::parse_error_exception, "JSON must contain name and message fields");
    }

    // 创建新的 error 对象
    std::string name    = data["name"].as<std::string>();
    std::string message = data["message"].as<std::string>();

    auto err = mc::make_shared<error_with_owner>(std::move(name), std::move(message));

    // 还原 format (如果存在)
    if (data.contains("format")) {
        if (data["format"].get_type() == mc::type_id::string_type) {
            err->set_format(data["format"].as<std::string>());
        }
    }

    // 还原 args (如果存在)
    if (data.contains("args")) {
        if (data["args"].get_type() == mc::type_id::object_type) {
            err->set_args(data["args"].as<mc::dict>());
        }
    }

    // 还原 traceback (如果存在)
    if (data.contains("traceback")) {
        if (data["traceback"].get_type() == mc::type_id::string_type) {
            err->m_traceback = data["traceback"].as<std::string>();
        }
    }

    // 还原 args_with_index (如果存在)
    if (data.contains("args_with_index")) {
        if (data["args_with_index"].get_type() == mc::type_id::object_type) {
            err->m_args_with_index = data["args_with_index"].as<mc::dict>();
        }
    }

    return err;
}

/**
 * @brief 获取并保存调用栈
 */
void error::traceback()
{
    std::ostringstream oss;

    // 使用 execinfo.h 获取调用栈
    void* buffer[64];
    int   ntrs = backtrace(buffer, 64);
    if (ntrs <= 0) {
        return; // 无法获取调用栈
    }

    char** strings = backtrace_symbols(buffer, ntrs);
    if (strings == nullptr) {
        return; // 无法获取符号
    }

    // 格式化调用栈信息
    for (int i = 0; i < ntrs; i++) {
        // 使用 abi::__cxa_demangle 符号化 C++ 函数名
        int   status;
        char* demangled = abi::__cxa_demangle(strings[i], nullptr, nullptr, &status);
        if (demangled != nullptr && status == 0) {
            oss << "[" << i << "] " << demangled << "\n";
        } else {
            oss << "[" << i << "] " << strings[i] << "\n";
        }
    }

    m_traceback = oss.str();
}

/**
 * @brief 抛出异常
 */
[[noreturn]] void error::raise() const
{
    // 将 error 对象序列化为 JSON 字符串
    std::string error_json = encode();
    throw mc::error_exception(this->name.data(), error_json, mc::error_engine_exception_code);
}

} // namespace mc
