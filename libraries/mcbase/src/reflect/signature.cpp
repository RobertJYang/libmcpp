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

#include <mc/exception.h>
#include <mc/reflect/signature.h>

namespace mc {
namespace reflect {

// signature类实现

signature::signature()
{}

signature::signature(mc::string sig) : m_sig(std::move(sig))
{}

signature::signature(const char* sig) : signature(mc::string(sig))
{}

signature::signature(type_code type)
{
    m_sig = mc::string(1, type_to_char(type));
}

signature& signature::operator+=(const signature& other)
{
    m_sig += other.m_sig;
    return *this;
}

signature& signature::operator+=(char c)
{
    m_sig += c;
    return *this;
}

signature& signature::operator+=(mc::string_view str)
{
    m_sig += str;
    return *this;
}

signature signature::operator+(const signature& other) const
{
    signature result(*this);
    result += other;
    return result;
}

mc::string_view signature::str() const
{
    return m_sig;
}

size_t signature::size() const
{
    return m_sig.size();
}

bool signature::operator==(const signature& other) const
{
    return m_sig == other.m_sig;
}

bool signature::operator!=(const signature& other) const
{
    return !(*this == other);
}

signature& signature::operator=(mc::string_view str)
{
    m_sig = str;
    return *this;
}

signature& signature::operator=(mc::string str)
{
    m_sig = std::move(str);
    return *this;
}

signature::operator mc::string_view() const
{
    return m_sig;
}

bool signature::is_empty() const
{
    return m_sig.empty();
}

void signature::clear()
{
    m_sig.clear();
}

bool signature::is_valid() const
{
    return is_valid(m_sig);
}

bool signature::is_valid(mc::string_view sig)
{
    // 签名的最大长度为255
    if (sig.size() > max_signature_length) {
        return false;
    }

    // 验证每个完整类型
    size_t pos = 0;
    while (pos < sig.size()) {
        if (!is_complete_type(sig[pos])) {
            return false;
        }

        // 获取当前完整类型的长度并前进
        size_t type_len = get_complete_type_length(sig, pos);
        if (type_len == 0) {
            return false;
        }
        pos += type_len;
    }

    return true;
}

bool signature::is_complete_type(char c)
{
    return is_basic_type(c) || is_container_type(c);
}

bool signature::is_complete_type(type_code type)
{
    return is_complete_type(type_to_char(type));
}

bool signature::is_basic_type(char c)
{
    switch (char_to_type(c)) {
        case type_code::byte_type:        // 字节（byte）
        case type_code::boolean_type:     // 布尔（boolean）
        case type_code::int16_type:       // 16位整数（int16）
        case type_code::uint16_type:      // 16位无符号整数（uint16）
        case type_code::int32_type:       // 32位整数（int32）
        case type_code::uint32_type:      // 32位无符号整数（uint32）
        case type_code::int64_type:       // 64位整数（int64）
        case type_code::uint64_type:      // 64位无符号整数（uint64）
        case type_code::double_type:      // 双精度浮点数（double）
        case type_code::string_type:      // 字符串（string）
        case type_code::object_path_type: // 对象路径（object_path）
        case type_code::signature_type:   // 签名（signature）
        case type_code::unix_fd_type:     // 文件描述符（unix_fd）
            return true;
        default:
            return false;
    }
}

bool signature::is_basic_type(type_code type)
{
    return is_basic_type(type_to_char(type));
}

bool signature::is_container_type(char c)
{
    switch (char_to_type(c)) {
        case type_code::array_type:       // 数组
        case type_code::struct_type:      // 结构体开始
        case type_code::struct_start:     // 结构体开始
        case type_code::variant_type:     // 变体
        case type_code::dict_entry_type:  // 字典项开始
        case type_code::dict_entry_start: // 字典项开始
            return true;
        default:
            return false;
    }
}

bool signature::is_container_type(type_code type)
{
    return is_container_type(type_to_char(type));
}

bool signature::is_single_complete_type(mc::string_view sig)
{
    if (sig.empty()) {
        return false;
    }

    return get_complete_type_length(sig) == sig.size();
}

size_t signature::get_complete_type_length(mc::string_view sig, size_t start_pos)
{
    if (start_pos >= sig.size()) {
        return 0;
    }

    char c = sig[start_pos];

    // 基本类型的长度是1
    if (is_basic_type(c)) {
        return 1;
    }

    // 对于容器类型，需要计算整个类型的长度
    switch (char_to_type(c)) {
        case type_code::variant: // 变体
            return 1;

        case type_code::array_start: // 数组
            if (start_pos + 1 >= sig.size()) {
                return 0; // 数组后面必须有一个完整类型
            }
            return 1 + get_complete_type_length(sig, start_pos + 1);

        case type_code::struct_start: { // 结构体
            size_t pos     = start_pos + 1;
            int    nesting = 1;

            while (pos < sig.size() && nesting > 0) {
                if (sig[pos] == type_to_char(type_code::struct_start)) {
                    nesting++;
                } else if (sig[pos] == type_to_char(type_code::struct_end)) {
                    nesting--;
                }
                pos++;
            }

            if (nesting > 0) {
                return 0; // 结构体没有闭合
            }

            return pos - start_pos;
        }

        case type_code::dict_entry_start: { // 字典项
            size_t pos = start_pos + 1;

            // 字典项必须有一个基本类型作为键
            if (pos >= sig.size() || !is_basic_type(sig[pos])) {
                return 0;
            }

            // 键之后必须有一个值类型
            size_t key_len = get_complete_type_length(sig, pos);
            if (key_len == 0) {
                return 0;
            }

            pos += key_len;

            // 值类型之后必须是结束括号
            size_t value_len = get_complete_type_length(sig, pos);
            if (value_len == 0) {
                return 0;
            }

            pos += value_len;

            if (pos >= sig.size() || sig[pos] != type_to_char(type_code::dict_entry_end)) {
                return 0;
            }

            return pos - start_pos + 1;
        }

        default:
            return 0; // 无效的容器类型
    }
}

std::vector<signature> signature::get_complete_types() const
{
    std::vector<signature> types;
    size_t                 pos = 0;

    while (pos < m_sig.size()) {
        size_t type_len = get_complete_type_length(m_sig, pos);
        if (type_len == 0) {
            break;
        }

        types.push_back(signature(m_sig.substr(pos, type_len)));
        pos += type_len;
    }

    return types;
}

void signature::validate() const
{
    validate(m_sig);
}

void signature::validate(mc::string_view sig)
{
    if (!is_valid(sig)) {
        MC_THROW(mc::invalid_arg_exception, "invalid signature: ${sig}", ("sig", sig));
    }
}

type_code signature::first_type_code() const
{
    if (is_empty()) {
        return type_code::invalid_type;
    }

    return char_to_type(m_sig[0]);
}

char signature::first_type() const
{
    if (is_empty()) {
        return empty_signature;
    }

    return m_sig[0];
}

std::ostream& operator<<(std::ostream& os, const signature& sig)
{
    os << sig.str();
    return os;
}

// signature_iterator类实现

signature_iterator::signature_iterator()
{}

signature_iterator::signature_iterator(const signature& sig, size_t pos) : signature_iterator(sig.str(), pos)
{}

signature_iterator::signature_iterator(mc::string_view sig, size_t pos) : m_sig(sig), m_pos(pos)
{
    signature::validate(sig);
}

mc::string_view signature_iterator::current_type() const
{
    size_t type_len = signature::get_complete_type_length(m_sig, m_pos);
    return m_sig.substr(m_pos, type_len);
}

char signature_iterator::current_type_char() const
{
    if (!is_valid()) {
        return empty_signature;
    }

    return m_sig[m_pos];
}

type_code signature_iterator::current_type_code() const
{
    return char_to_type(current_type_char());
}

bool signature_iterator::is_container() const
{
    return signature::is_container_type(current_type_char());
}

bool signature_iterator::is_basic() const
{
    return signature::is_basic_type(current_type_char());
}

bool signature_iterator::is_valid() const
{
    return m_pos < m_sig.size();
}

bool signature_iterator::is_empty() const
{
    return m_sig.empty();
}

bool signature_iterator::at_end() const
{
    return !is_valid();
}

signature_iterator& signature_iterator::next()
{
    if (is_valid()) {
        size_t type_len = signature::get_complete_type_length(m_sig, m_pos);
        m_pos += type_len;
    }

    return *this;
}

signature_iterator signature_iterator::get_content_iterator() const
{
    if (current_type_char() == type_to_char(type_code::array_start)) {
        size_t type_len = signature::get_complete_type_length(m_sig, m_pos);
        if (type_len == 0) {
            MC_THROW(mc::invalid_arg_exception, "invalid signature array: ${sig}, pos: ${pos}",
                     ("sig", m_sig)("pos", m_pos));
        }
        return signature_iterator(m_sig.substr(m_pos + 1, type_len - 1), 0);
    }

    if (current_type_char() == type_to_char(type_code::struct_start)) {
        size_t type_len = signature::get_complete_type_length(m_sig, m_pos);
        if (type_len == 0) {
            MC_THROW(mc::invalid_arg_exception, "invalid signature struct: ${sig}, pos: ${pos}",
                     ("sig", m_sig)("pos", m_pos));
        }
        return signature_iterator(m_sig.substr(m_pos + 1, type_len - 2), 0);
    }

    return {};
}

signature_iterator signature_iterator::get_dict_key_iterator() const
{
    if (current_type_char() != type_to_char(type_code::dict_entry_start)) {
        return {};
    }

    return signature_iterator(m_sig, m_pos + 1);
}

signature_iterator signature_iterator::get_dict_value_iterator() const
{
    if (current_type_char() != type_to_char(type_code::dict_entry_start)) {
        return {};
    }

    size_t key_pos = m_pos + 1;
    size_t key_len = signature::get_complete_type_length(m_sig, key_pos);

    return signature_iterator(m_sig, key_pos + key_len);
}

mc::string_view signature_iterator::str() const
{
    return m_sig;
}

size_t signature_iterator::pos() const
{
    return m_pos;
}

} // namespace reflect
} // namespace mc