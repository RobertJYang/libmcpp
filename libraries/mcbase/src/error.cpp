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
#include <mc/exception.h>
#include <mc/fmt/format_dict.h>
#include <mc/json.h>
#include <mc/log/log.h>
#include <mc/string_utils.h>

#include <algorithm>
#include <cxxabi.h>
#include <execinfo.h>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mc {

namespace {

mc::string format_percent_placeholders(mc::string_view template_msg, const mc::dict& args)
{
    mc::string  result(template_msg);
    std::string result_std(mc::to_std_string(result));

    static const std::regex placeholder_pattern("%(\\d+)");

    mc::string output;
    auto       begin = std::sregex_iterator(result_std.begin(), result_std.end(), placeholder_pattern);
    auto       end   = std::sregex_iterator();

    size_t last_pos = 0;
    for (auto it = begin; it != end; ++it) {
        const std::smatch& match = *it;
        output.append(result_std.data() + last_pos, static_cast<std::size_t>(match.position() - last_pos));

        int key_index = std::stoi(match.str(1)) - 1;
        if (args.contains(key_index)) {
            const mc::variant& arg_value = args[key_index];
            switch (arg_value.get_type()) {
                case mc::type_id::string_type:
                    output += arg_value.as<mc::string>();
                    break;
                case mc::type_id::int8_type:
                case mc::type_id::int16_type:
                case mc::type_id::int32_type:
                case mc::type_id::int64_type:
                    output += mc::to_string(arg_value.as<int64_t>());
                    break;
                case mc::type_id::uint8_type:
                case mc::type_id::uint16_type:
                case mc::type_id::uint32_type:
                case mc::type_id::uint64_type:
                    output += mc::to_string(arg_value.as<uint64_t>());
                    break;
                case mc::type_id::double_type:
                    output += mc::to_string(arg_value.as<double>());
                    break;
                case mc::type_id::bool_type:
                    output += arg_value.as<bool>() ? "true" : "false";
                    break;
                default:
                    output += mc::to_string(arg_value);
                    break;
            }
        } else {
            wlog("消息格式化缺少参数: 占位符 ${placeholder} 没有对应的参数值", ("placeholder", match.str()));
            {
                const auto placeholder = match.str();
                output.append(placeholder.data(), placeholder.size());
            }
        }

        last_pos = match.position() + match.length();
    }

    if (last_pos < result_std.length()) {
        output.append(result_std.data() + last_pos, result_std.length() - last_pos);
    }

    return output;
}

} // namespace

error::error() = default;

error::error(const error_info& info) : mc::enable_shared_from_this<error>(), error_info(info)
{}

error::error(mc::string_view name, mc::string_view format, error_level level)
    : mc::enable_shared_from_this<error>(), error_info(name, format, level)
{
    // 存储 name 和 format 的副本，确保 string_view 的生命周期
    m_name_storage   = name;
    m_format_storage = format;
    // 更新基类的 string_view 指向存储的副本
    this->name   = m_name_storage;
    this->format = m_format_storage;
}

error::error(const error& other)
    : mc::enable_shared_from_this<error>(other), error_info(other.m_name_storage, other.m_format_storage, other.level),
      m_name_storage(other.m_name_storage), m_format_storage(other.m_format_storage)
{
    this->args = other.args;
    // 不复制缓存，让新对象懒加载

    if (other.prev_error) {
        this->prev_error.reset(new error(*other.prev_error));
    }

    // 确保 string_view 指向存储的副本
    this->name   = m_name_storage;
    this->format = m_format_storage;
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

        this->m_name_storage   = other.m_name_storage;
        this->m_format_storage = other.m_format_storage;
        this->name             = m_name_storage;
        this->format           = m_format_storage;
        this->level            = other.level;
        this->args             = other.args;
        this->registry_prefix  = other.registry_prefix;
        this->m_cached_message.reset(); // 清除缓存

        if (other.prev_error) {
            this->prev_error.reset(new error(*other.prev_error));
        }
    }

    return *this;
}

mc::string_view error::get_name() const
{
    return this->name;
}

mc::string_view error::get_format() const
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

mc::string error::get_message() const
{
    // 懒加载：如果缓存中有格式化消息，直接返回
    if (m_cached_message.has_value()) {
        return mc::string(m_cached_message.value());
    }

    // 获取 format：如果为空，尝试从错误引擎查找
    mc::string_view format_to_use = this->format;
    bool            from_registry = false;
    if (format_to_use.empty() && !this->name.empty()) {
        auto info = mc::error_engine::get_instance().get_error_info(this->name);
        if (info.is_valid()) {
            format_to_use = info.format;
            from_registry = true;
        }
    }

    if (format_to_use.empty()) {
        return mc::string();
    }

    // 首次访问时格式化消息并缓存
    mc::string formatted;
    // 检测是否包含 %数字 占位符
    static const std::regex placeholder_pattern("%(\\d+)");
    bool has_percent_placeholder = std::regex_search(format_to_use.begin(), format_to_use.end(), placeholder_pattern);

    if (from_registry || has_percent_placeholder) {
        formatted = format_percent_placeholders(format_to_use, args);
    } else {
        // 直接设置的 format，使用 format_dict 格式化
        formatted = mc::format_dict(format_to_use, args);
    }
    m_cached_message = formatted;
    return mc::string(formatted);
}

error_level error::get_level() const
{
    return this->level;
}

void error::set_level(error_level level)
{
    this->level = level;
}

void error::set_name(mc::string_view name)
{
    m_name_storage = name;
    this->name     = m_name_storage;
}

void error::set_format(mc::string_view format)
{
    m_format_storage = format;
    this->format     = m_format_storage;
}

mc::string_view error::get_registry_prefix() const
{
    return this->registry_prefix;
}

void error::set_registry_prefix(mc::string_view prefix)
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

mc::string error::to_string() const
{
    std::ostringstream oss;

    // 输出错误名称和消息
    oss << get_name() << ": " << get_message().view();

    // 如果有调用栈信息，追加显示
    if (!m_traceback.empty()) {
        oss << "\n" << m_traceback;
    }

    return mc::string(oss.str());
}

mc::string error::to_string_format_inplace() const
{
    mc::dict error_data;
    error_data["name"]   = this->name;
    error_data["format"] = mc::string_view(get_message().view());
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

bool error::has_error(mc::string_view name) const
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
    return mc::log::message(this->level, mc::log::context("", this->name, 0), mc::string(this->format), this->args);
}

error_with_owner::error_with_owner()
{}

error_with_owner::error_with_owner(mc::string_view name, mc::string_view format)
    : m_name_owner(name), m_format_owner(format)
{
    this->name   = m_name_owner;
    this->format = m_format_owner;
}

bool get_error_format_args(mc::string_view format, mc::dict& arg_names)
{
    return mc::fmt::get_format_args(format, arg_names);
}

// ============================================================================
// 新增辅助函数实现
// ============================================================================

/**
 * @brief 查找参数位置索引(string版本)
 */
int error::get_param_index(mc::string_view param_name, mc::string_view param_value)
{
    if (param_name == param_value) {
        return 0;
    }
    return -1;
}

/**
 * @brief 查找参数位置索引(dict版本)
 * param_struct 是一个数组，每个元素是一个 dict，包含 name 字段
 * 遍历数组，找到 name 字段匹配的元素索引
 */
int error::get_param_index(mc::string_view param_name, const mc::dict& param_struct)
{
    if (param_name.empty()) {
        return -1;
    }

    // 遍历 param_struct 数组（使用整数索引 1, 2, 3, ...）
    for (const auto& entry : param_struct) {
        const mc::variant& key   = entry.key;
        const mc::variant& value = entry.value;

        // 只处理整数键（数组类型）
        if (key.is_int64()) {
            // 检查 value 是否是 dict 类型
            if (value.is_object()) {
                mc::dict dict_value = value.get_object();

                // 检查 dict 中是否有 name 字段
                if (dict_value.contains("name")) {
                    const mc::variant& name_variant = dict_value["name"];

                    // 比较 name 字段与 param_name
                    if (name_variant.is_string()) {
                        mc::string name_str(name_variant.get_string());
                        if (name_str == param_name) {
                            // 返回位置索引（从 0 开始，对应 Lua 的从 1 开始）
                            return static_cast<int>(key.as<int64_t>()) - 1;
                        }
                    }
                }
            }
        }
    }

    return -1;
}

/**
 * @brief 参数名映射为位置索引(string版本)
 */
void error::post_process(mc::string_view param_struct)
{
    post_process_impl([&param_struct](mc::string_view param_name) {
        return get_param_index(param_name, param_struct);
    });
}

/**
 * @brief 参数名映射为位置索引(dict版本)
 */
void error::post_process(const mc::dict& param_struct)
{
    post_process_impl([&param_struct](mc::string_view param_name) {
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
    int      index          = 0;
    bool     has_index_flag = false;

    // 遍历 args 的值
    for (const auto& entry : args) {
        const mc::variant& value = entry.value;
        const mc::variant& key   = entry.key;

        if (value.is_string()) {
            mc::string        arg_value(value.get_string());
            const std::string arg_std = mc::to_std_string(arg_value);
            std::smatch       match;
            try {
                if (std::regex_search(arg_std, match, param_pattern) && match.size() > 1) {
                    // 提取参数名 (第一个捕获组)
                    mc::string param_name = match.str(1);
                    int        pos        = index_lookup(param_name);
                    if (pos >= 0) {
                        has_index_flag = true;
                        // 更新 args: 提取冒号后的内容，或者去掉%
                        const size_t colon_pos = arg_std.find(':');
                        if (colon_pos != std::string::npos) {
                            new_args[key] = mc::string(arg_std.substr(colon_pos + 1));
                        } else {
                            // 去掉所有的 %
                            mc::string cleaned = arg_value;
                            cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '%'), cleaned.end());
                            new_args[key] = cleaned;
                        }
                        // 创建 args_with_index: 将参数名替换为位置索引
                        mc::string new_format  = "%" + mc::to_string(pos + 1); // 位置索引从1开始
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
    if (has_index_flag) {
        args              = new_args;
        m_args_with_index = args_with_index;
    }
}

/**
 * @brief 序列化为 JSON
 */
mc::string error::encode(const mc::json::json_encode_options& options) const
{
    mc::dict result;

    // 序列化 name, message, args, format
    result["name"]    = mc::string(get_name());
    result["message"] = mc::string_view(get_message().view());
    result["format"]  = mc::string(get_format());
    result["params"]  = get_args();

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
        result["args_with_index"] = m_args_with_index;
    }

    return mc::json::json_encode(result, options);
}

/**
 * @brief 从 JSON 反序列化(静态方法)
 */
mc::shared_ptr<error> error::decode(mc::string_view json, const mc::json::json_decode_options& options)
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
    mc::string name    = data["name"].as<mc::string>();
    mc::string message = data["message"].as<mc::string>();

    auto err = mc::make_shared<error_with_owner>(std::move(name), std::move(message));

    // 还原 format (如果存在)
    if (data.contains("format")) {
        if (data["format"].get_type() == mc::type_id::string_type) {
            err->set_format(data["format"].as<mc::string>());
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
            err->m_traceback = data["traceback"].as<mc::string>();
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
        // 使用 __cxxabiv1::__cxa_demangle 符号化 C++ 函数名
        int   status;
        char* demangled = __cxxabiv1::__cxa_demangle(strings[i], nullptr, nullptr, &status);
        if (demangled != nullptr && status == 0) {
            oss << "[" << i << "] " << demangled << "\n";
        } else {
            oss << "[" << i << "] " << strings[i] << "\n";
        }
    }

    m_traceback = mc::string(oss.str());
}

/**
 * @brief 抛出异常
 */
[[noreturn]] void error::raise() const
{
    // 将 error 对象序列化为 JSON 字符串
    mc::string error_json = encode();
    throw mc::error_exception(this->name.data(), error_json, mc::error_engine_exception_code);
}

} // namespace mc
