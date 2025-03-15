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

/**
 * @file yaml.cpp
 * @brief 实现YAML编解码功能
 */
#include <mc/yaml.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <mc/exception.h>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <limits>
#include <cstdint>
#include <climits>
#include <algorithm>
#include <vector>
#include <string>

namespace mc {
namespace yaml {

// 用于YAML编码的辅助类
class encoder {
public:
    explicit encoder(const yaml_encode_options& options = yaml_encode_options::default_encode_options) 
        : m_stream(), m_options(options), m_current_depth(0) 
    {
        // 规范化选项值
        m_options.normalize();
        
        // 如果指定了浮点数精度，设置流的精度
        if (m_options.float_precision >= 0) {
            m_stream.precision(m_options.float_precision);
        } else {
            m_stream.precision(std::numeric_limits<double>::max_digits10);
        }
    }

    // 编码入口函数
    std::string encode(const variant& value) {
        encode_value(value);
        return m_stream.str();
    }

private:
    std::ostringstream m_stream;
    yaml_encode_options m_options;
    int m_current_depth;

    // 检查嵌套深度
    void check_depth() {
        if (m_current_depth >= m_options.max_depth) {
            MC_THROW(mc::parse_error_exception, "YAML嵌套深度超过限制");
        }
    }

    // 进入新的嵌套层级
    void enter_scope() {
        ++m_current_depth;
        check_depth();
    }

    // 离开嵌套层级
    void leave_scope() {
        --m_current_depth;
    }

    // 添加缩进
    void add_indent() {
        if (m_options.pretty_print) {
            m_stream << '\n' << std::string(m_current_depth * m_options.indent_size, ' ');
        }
    }

    // 判断字符串是否需要引号
    bool needs_quotes(const std::string& str) {
        // 空字符串需要引号
        if (str.empty()) {
            return true;
        }

        // 检查是否包含特殊字符或YAML保留字
        static const std::string special_chars = "\"'{}[],:&*#?|->%@`";
        for (char c : str) {
            // 控制字符、空白字符或特殊字符需要引号
            if (c <= ' ' || special_chars.find(c) != std::string::npos) {
                return true;
            }
        }

        // 检查是否是YAML保留字
        static const std::vector<std::string> reserved_words = {
            "true", "false", "null", "~", "yes", "no", "on", "off"
        };
        for (const auto& word : reserved_words) {
            if (str == word) {
                return true;
            }
        }

        // 检查是否以数字开头（避免被解析为数字）
        if (std::isdigit(str[0]) || str[0] == '-' || str[0] == '+' || str[0] == '.') {
            return true;
        }

        return false;
    }

    // 编码字符串
    void encode_string(const std::string& str) {
        if (m_options.single_quote) {
            // 使用单引号
            m_stream << "'";
            for (char c : str) {
                if (c == '\'')
                    m_stream << "''";
                else
                    m_stream << c;
            }
            m_stream << "'";
        } else if (needs_quotes(str)) {
            // 根据内容决定是否需要引号
            m_stream << "\"";
            for (char c : str) {
                switch (c) {
                    case '\"': m_stream << "\\\""; break;
                    case '\\': m_stream << "\\\\"; break;
                    case '\b': m_stream << "\\b"; break;
                    case '\f': m_stream << "\\f"; break;
                    case '\n': m_stream << "\\n"; break;
                    case '\r': m_stream << "\\r"; break;
                    case '\t': m_stream << "\\t"; break;
                    default:
                        if (c < 0x20) {
                            m_stream << "\\u" << std::hex << std::setw(4) 
                                    << std::setfill('0') << static_cast<int>(c);
                        } else {
                            m_stream << c;
                        }
                }
            }
            m_stream << "\"";
        } else {
            // 不需要引号的情况
            m_stream << str;
        }
    }

    // 编码数字
    void encode_number(double num) {
        if (std::isfinite(num)) {
            if (m_options.float_precision >= 0) {
                // 使用 fixed 和 setprecision 控制小数位数
                m_stream << std::fixed << std::setprecision(m_options.float_precision) << num;
            } else {
                // 使用默认精度
                m_stream << std::defaultfloat << num;
            }
        } else {
            MC_THROW(mc::parse_error_exception, "无效的数值");
        }
    }

    // 编码数组
    void encode_array(const variants& arr) {
        enter_scope();
        
        if (arr.empty()) {
            m_stream << "[]";
            leave_scope();
            return;
        }
        
        if (m_options.flow_style) {
            // 流式风格 [item1, item2, ...]
            m_stream << "[";
            bool first = true;
            for (const auto& item : arr) {
                if (!first) {
                    m_stream << ",";
                }
                if (m_options.pretty_print) {
                    m_stream << " ";
                }
                encode_value(item);
                first = false;
            }
            m_stream << "]";
        } else {
            // 块式风格
            // - item1
            // - item2
            bool first = true;
            for (const auto& item : arr) {
                if (!first) {
                    add_indent();
                }
                m_stream << "-";
                
                // 检查是否是复杂类型，决定是否需要换行
                bool is_complex = item.is_array() || item.is_object();
                
                if (is_complex) {
                    ++m_current_depth;
                    add_indent();
                    --m_current_depth;
                    encode_value(item);
                } else {
                    m_stream << " ";
                    encode_value(item);
                }
                
                first = false;
            }
        }
        
        leave_scope();
    }
    
    // 编码对象
    void encode_object(const dict& obj) {
        enter_scope();
        
        if (obj.empty()) {
            m_stream << "{}";
            leave_scope();
            return;
        }
        
        // 获取所有键并可能排序
        std::vector<std::reference_wrapper<const std::string>> keys;
        keys.reserve(obj.size());
        for (const auto& entry : obj) {
            keys.push_back(std::cref(entry.key));
        }
        if (m_options.sort_keys) {
            std::sort(keys.begin(), keys.end(), 
                [](const auto& a, const auto& b) { return a.get() < b.get(); });
        }
        
        if (m_options.flow_style) {
            // 流式风格 {key1: value1, key2: value2, ...}
            m_stream << "{";
            bool first = true;
            for (const auto& key_ref : keys) {
                const auto& key = key_ref.get();
                if (!first) {
                    m_stream << ",";
                }
                if (m_options.pretty_print) {
                    m_stream << " ";
                }
                encode_string(key);
                m_stream << ":";
                if (m_options.pretty_print) {
                    m_stream << " ";
                }
                encode_value(obj[key]);
                first = false;
            }
            m_stream << "}";
        } else {
            // 块式风格
            // key1: value1
            // key2: value2
            bool first = true;
            for (const auto& key_ref : keys) {
                const auto& key = key_ref.get();
                if (!first) {
                    add_indent();
                }
                encode_string(key);
                m_stream << ":";
                
                // 检查是否是复杂类型，决定是否需要换行
                const variant& value = obj[key];
                bool is_complex = value.is_array() || value.is_object();
                
                if (is_complex) {
                    ++m_current_depth;
                    add_indent();
                    --m_current_depth;
                    encode_value(value);
                } else {
                    m_stream << " ";
                    encode_value(value);
                }
                
                first = false;
            }
        }
        
        leave_scope();
    }
    
    // 编码任意值
    void encode_value(const variant& value) {
        switch (value.get_type()) {
            case variant::type_id::null_type:
                m_stream << "null";
                break;
            case variant::type_id::bool_type:
                m_stream << (value.as_bool() ? "true" : "false");
                break;
            case variant::type_id::int8_type:
            case variant::type_id::int16_type:
            case variant::type_id::int32_type:
            case variant::type_id::int64_type:
                m_stream << value.as_int64();
                break;
            case variant::type_id::uint8_type:
            case variant::type_id::uint16_type:
            case variant::type_id::uint32_type:
            case variant::type_id::uint64_type:
                m_stream << value.as_uint64();
                break;
            case variant::type_id::double_type:
                encode_number(value.as_double());
                break;
            case variant::type_id::string_type:
                encode_string(value.as_string());
                break;
            case variant::type_id::array_type:
                encode_array(value.get_array());
                break;
            case variant::type_id::object_type:
                encode_object(value.get_object());
                break;
            case variant::type_id::blob_type:
                encode_string(value.as_string()); // blob 类型转换为字符串处理
                break;
            default:
                MC_THROW(mc::parse_error_exception, "不支持的值类型");
                break;
        }
    }
};

// 用于YAML解码的辅助类
class decoder {
public:
    explicit decoder(std::string_view yaml, const yaml_decode_options& options = yaml_decode_options::default_decode_options)
        : m_input(yaml), m_pos(0), m_options(options), m_current_depth(0)
    {
        m_options.normalize();
        
        // 检查输入YAML字符串的总长度
        if (m_input.length() > m_options.max_input_length) {
            MC_THROW(mc::parse_error_exception, "输入YAML字符串长度超过限制");
        }
    }

    // 解码入口函数
    variant decode() {
        // 解码功能将在后续实现
        MC_THROW(mc::parse_error_exception, "YAML解码功能暂未实现");
        return variant();
    }

private:
    std::string_view m_input;
    size_t m_pos;
    yaml_decode_options m_options;
    int m_current_depth;
};

// 实现yaml_encode函数
std::string yaml_encode(const variant& value, const yaml_encode_options& options) {
    try {
        encoder enc(options);
        return enc.encode(value);
    } catch (const mc::parse_error_exception&) {
        // 如果已经是parse_error_exception，直接重新抛出
        throw;
    } catch (const std::exception& e) {
        // 将其他异常转换为parse_error_exception
        MC_THROW(mc::parse_error_exception, std::string("YAML编码失败: ") + e.what());
    }
}

// 实现yaml_decode函数
variant yaml_decode(std::string_view yaml, const yaml_decode_options& options) {
    try {
        decoder dec(yaml, options);
        return dec.decode();
    } catch (const mc::parse_error_exception&) {
        // 如果已经是parse_error_exception，直接重新抛出
        throw;
    } catch (const std::exception& e) {
        // 将其他异常转换为parse_error_exception
        MC_THROW(mc::parse_error_exception, std::string("YAML解码失败: ") + e.what());
    }
}

// 实现yaml_decode_all函数
std::vector<variant> yaml_decode_all(std::string_view yaml, const yaml_decode_options& options) {
    // 多文档解码功能将在后续实现
    MC_THROW(mc::parse_error_exception, "多文档YAML解码功能暂未实现");
    return std::vector<variant>();
}

// 初始化全局默认配置
yaml_encode_options yaml_encode_options::default_encode_options;
yaml_decode_options yaml_decode_options::default_decode_options;

} // namespace yaml
} // namespace mc