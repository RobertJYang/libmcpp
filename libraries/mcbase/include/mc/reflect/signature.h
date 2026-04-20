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

#ifndef MC_REFLECT_SIGNATURE_H
#define MC_REFLECT_SIGNATURE_H

#include <mc/reflect/signature_helper.h>
#include <mc/reflect/type_code.h>
#include <mc/variant.h>

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace mc {
namespace reflect {

constexpr char   empty_signature      = '\0';
constexpr size_t max_signature_length = 255;
constexpr size_t max_path_length      = 1023;

inline char first_type(mc::string_view sig)
{
    if (sig.empty()) {
        return '\0';
    }
    if (sig[0] == signature_chars::STRUCT_BEGIN) {
        return MC_TYPE_STRUCT;
    }

    return sig[0];
}

class MC_API signature {
public:
    signature();

    explicit signature(mc::string sig);

    explicit signature(const char* sig);

    explicit signature(type_code type);

    signature& operator+=(const signature& other);

    signature& operator+=(char c);

    signature operator+(char c) const
    {
        return signature(*this) += c;
    }

    signature operator+(type_code c) const
    {
        return signature(*this) += type_to_char(c);
    }

    signature operator+(mc::string_view str) const
    {
        return signature(*this) += str;
    }

    signature& operator+=(type_code c)
    {
        operator+=(type_to_char(c));
        return *this;
    }

    signature& operator+=(mc::string_view str);

    signature operator+(const signature& other) const;

    signature& operator=(mc::string_view str);
    signature& operator=(mc::string str);
    signature& operator=(std::string_view str)
    {
        return operator=(mc::string_view(str));
    }
    signature& operator=(const std::string& str)
    {
        return operator=(mc::string_view(str.data(), str.size()));
    }
    signature& operator=(const char* str)
    {
        return operator=(mc::string_view(str));
    }

    bool operator==(mc::string_view str) const
    {
        return m_sig == str;
    }
    bool operator!=(mc::string_view str) const
    {
        return m_sig != str;
    }
    friend bool operator==(mc::string_view str, const signature& sig)
    {
        return sig.m_sig == str;
    }
    friend bool operator!=(mc::string_view str, const signature& sig)
    {
        return sig.m_sig != str;
    }

    operator mc::string_view() const;

    mc::string_view str() const;

    size_t size() const;

    bool operator==(const signature& other) const;

    bool operator!=(const signature& other) const;

    bool is_empty() const;

    void clear();

    bool is_valid() const;

    static bool is_valid(mc::string_view sig);

    static bool is_complete_type(char c);
    static bool is_complete_type(type_code type);

    static bool is_basic_type(char c);
    static bool is_basic_type(type_code type);

    static bool is_container_type(char c);
    static bool is_container_type(type_code type);

    static bool is_single_complete_type(mc::string_view sig);

    static size_t get_complete_type_length(mc::string_view sig, size_t start_pos = 0);

    std::vector<signature> get_complete_types() const;

    void        validate() const;
    static void validate(mc::string_view sig);

    type_code first_type_code() const;
    char      first_type() const;

private:
    mc::string m_sig;
};

inline std::ostream& operator<<(std::ostream& os, const signature& sig)
{
    return os << sig.str();
}

class MC_API signature_iterator {
public:
    signature_iterator();

    signature_iterator(const signature& sig, size_t pos = 0);

    signature_iterator(mc::string_view sig, size_t pos = 0);
    signature_iterator(std::string_view sig, size_t pos = 0) : signature_iterator(mc::string_view(sig), pos)
    {}
    signature_iterator(const char* sig, size_t pos = 0) : signature_iterator(mc::string_view(sig), pos)
    {}
    signature_iterator(const std::string& sig, size_t pos = 0)
        : signature_iterator(mc::string_view(sig.data(), sig.size()), pos)
    {}
    signature_iterator(const mc::string& sig, size_t pos = 0) : signature_iterator(mc::string_view(sig), pos)
    {}

    mc::string_view current_type() const;

    char current_type_char() const;

    type_code current_type_code() const;

    bool is_container() const;

    bool is_basic() const;

    bool is_valid() const;

    bool is_empty() const;

    bool at_end() const;

    signature_iterator& next();

    signature_iterator get_content_iterator() const;

    signature_iterator get_dict_key_iterator() const;

    signature_iterator get_dict_value_iterator() const;
    mc::string_view    str() const;
    size_t             pos() const;

private:
    mc::string_view m_sig{};
    size_t          m_pos{0};
};

namespace detail {
// 对 signature 的特化
template <>
struct signature_helper<mc::reflect::signature> {
    static void apply(mc::string& sig)
    {
        sig += type_to_char(mc::reflect::type_code::signature_type);
    }
};
} // namespace detail

} // namespace reflect

inline void to_variant(const reflect::signature& sig, variant& v)
{
    v = sig.str();
}

inline void from_variant(const variant& v, reflect::signature& sig)
{
    sig = reflect::signature(v.as_string());
}

} // namespace mc

#endif // MC_REFLECT_SIGNATURE_H