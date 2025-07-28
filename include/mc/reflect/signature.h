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

#include <dbus/dbus.h>
#include <mc/reflect/signature_helper.h>
#include <mc/reflect/type_code.h>
#include <mc/variant.h>

#include <ostream>
#include <string>
#include <vector>

namespace mc {
namespace reflect {

constexpr char   empty_signature      = '\0';
constexpr size_t max_signature_length = 255;

inline char first_type(const std::string& sig) {
    if (sig.empty()) {
        return '\0';
    }
    if (sig[0] == DBUS_STRUCT_BEGIN_CHAR) {
        return DBUS_TYPE_STRUCT;
    }

    return sig[0];
}

class MC_API signature {
public:
    /**
     * 默认构造函数，创建空签名
     */
    signature();

    /**
     * 使用字符串构造签名
     *
     * @param sig 签名字符串
     * @throws invalid_signature_exception 如果签名格式无效
     */
    explicit signature(std::string sig);

    /**
     * 使用C风格字符串构造签名
     *
     * @param sig 签名C风格字符串
     * @throws invalid_signature_exception 如果签名格式无效
     */
    explicit signature(const char* sig);

    /**
     * 使用类型构造签名
     *
     * @param type 类型
     */
    explicit signature(type_code type);

    /**
     * 追加另一个签名
     *
     * @param other 要追加的签名
     * @return 追加后的签名引用
     */
    signature& operator+=(const signature& other);

    /**
     * 追加单个类型字符
     *
     * @param c 类型字符
     * @return 追加后的签名引用
     */
    signature& operator+=(char c);

    signature operator+(char c) const {
        return signature(*this) += c;
    }

    signature operator+(type_code c) const {
        return signature(*this) += type_to_char(c);
    }

    signature operator+(std::string_view str) const {
        return signature(*this) += str;
    }

    /**
     * 追加类型代码
     *
     * @param c 类型代码
     * @return 追加后的签名引用
     */
    signature& operator+=(type_code c) {
        operator+=(type_to_char(c));
        return *this;
    }

    /**
     * 追加字符串
     *
     * @param str 要追加的字符串
     * @return 追加后的签名引用
     */
    signature& operator+=(std::string_view str);

    /**
     * 连接两个签名
     *
     * @param other 要连接的签名
     * @return 连接后的新签名
     */
    signature operator+(const signature& other) const;

    signature& operator=(std::string_view str);
    signature& operator=(std::string str);
    signature& operator=(const char* str) {
        return operator=(std::string_view(str));
    }

    bool operator==(std::string_view str) const {
        return m_sig == str;
    }
    bool operator!=(std::string_view str) const {
        return m_sig != str;
    }
    friend bool operator==(std::string_view str, const signature& sig) {
        return sig.m_sig == str;
    }
    friend bool operator!=(std::string_view str, const signature& sig) {
        return sig.m_sig != str;
    }

    operator std::string_view() const;

    /**
     * 获取签名字符串
     *
     * @return 签名字符串
     */
    const std::string& str() const;

    /**
     * 获取签名长度
     *
     * @return 签名长度
     */
    size_t size() const;

    /**
     * 等于比较运算符
     *
     * @param other 要比较的签名
     * @return 如果签名相等返回true
     */
    bool operator==(const signature& other) const;

    /**
     * 不等于比较运算符
     *
     * @param other 要比较的签名
     * @return 如果签名不相等返回true
     */
    bool operator!=(const signature& other) const;

    /**
     * 检查签名是否为空
     *
     * @return 如果签名为空返回true
     */
    bool is_empty() const;

    /**
     * 清空签名
     */
    void clear();

    bool is_valid() const;

    /**
     * 验证签名字符串是否有效
     *
     * @param sig 要验证的签名字符串
     * @return 如果签名有效返回true
     */
    static bool is_valid(std::string_view sig);

    /**
     * 检查字符是否为完整类型
     *
     * @param c 要检查的字符
     * @return 如果字符是完整类型返回true
     */
    static bool is_complete_type(char c);
    static bool is_complete_type(type_code type);

    /**
     * 检查字符是否为基本类型
     *
     * @param c 要检查的字符
     * @return 如果字符是基本类型返回true
     */
    static bool is_basic_type(char c);
    static bool is_basic_type(type_code type);

    /**
     * 检查字符是否为容器类型
     *
     * @param c 要检查的字符
     * @return 如果字符是容器类型返回true
     */
    static bool is_container_type(char c);
    static bool is_container_type(type_code type);

    /**
     * 检查给定的签名是否表示一个单一的完整类型
     *
     * @param sig 要检查的签名
     * @return 如果签名是单一完整类型返回true
     */
    static bool is_single_complete_type(std::string_view sig);

    /**
     * 获取一个完整类型的长度
     *
     * @param sig 签名字符串
     * @param start_pos 起始位置
     * @return 完整类型的长度
     */
    static size_t get_complete_type_length(std::string_view sig, size_t start_pos = 0);

    /**
     * 获取签名的所有完整类型
     *
     * @return 完整类型列表
     */
    std::vector<signature> get_complete_types() const;

    void        validate() const;
    static void validate(std::string_view sig);

    type_code first_type_code() const;
    char      first_type() const;

private:
    std::string m_sig;
};

/**
 * 输出流操作符重载
 */
MC_API std::ostream& operator<<(std::ostream& os, const signature& sig);

/**
 * 用于遍历DBus签名的迭代器
 */
class MC_API signature_iterator {
public:
    signature_iterator();

    /**
     * 构造函数
     *
     * @param sig 要遍历的签名
     * @param pos 开始位置，默认为0
     */
    signature_iterator(const signature& sig, size_t pos = 0);

    /**
     * 使用字符串构造迭代器
     *
     * @param sig 要遍历的签名
     * @param pos 开始位置，默认为0
     */
    signature_iterator(std::string_view sig, size_t pos = 0);
    signature_iterator(const char* sig, size_t pos = 0)
        : signature_iterator(std::string_view(sig), pos) {
    }
    signature_iterator(const std::string& sig, size_t pos = 0)
        : signature_iterator(std::string_view(sig), pos) {
    }

    /**
     * 获取当前类型
     *
     * @return 当前类型的签名
     */
    std::string_view current_type() const;

    /**
     * 获取当前类型字符
     *
     * @return 当前类型字符
     */
    char current_type_char() const;

    /**
     * 获取当前类型的数据类型枚举值
     *
     * @return 当前类型的数据类型
     */
    type_code current_type_code() const;

    /**
     * 检查当前类型是否为容器类型
     *
     * @return 如果当前类型是容器类型返回true
     */
    bool is_container() const;

    /**
     * 检查当前类型是否为基本类型
     *
     * @return 如果当前类型是基本类型返回true
     */
    bool is_basic() const;

    /**
     * 检查当前位置是否有效
     *
     * @return 如果当前位置有效返回true
     */
    bool is_valid() const;

    /**
     * 检查签名是否为空
     *
     * @return 如果签名为空返回true
     */
    bool is_empty() const;

    /**
     * 检查迭代器是否已遍历到末尾
     *
     * @return 如果已遍历到末尾返回true
     */
    bool at_end() const;

    /**
     * 前进到下一个类型
     *
     * @return 移动后的迭代器引用
     */
    signature_iterator& next();

    /**
     * 如果当前类型是容器，获取其值类型的迭代器
     *
     * @return 值类型的迭代器
     */
    signature_iterator get_content_iterator() const;

    /**
     * 如果当前类型是字典项，获取其键类型的迭代器
     *
     * @return 键类型的迭代器
     */
    signature_iterator get_dict_key_iterator() const;

    /**
     * 如果当前类型是字典项，获取其值类型的迭代器
     *
     * @return 值类型的迭代器
     */
    signature_iterator get_dict_value_iterator() const;
    std::string_view   str() const;
    size_t             pos() const;

private:
    std::string_view m_sig{};
    size_t           m_pos{0};
};

namespace detail {
// 对 signature 的特化
template <>
struct signature_helper<mc::reflect::signature> {
    static void apply(std::string& sig) {
        sig += type_to_char(mc::reflect::type_code::signature_type);
    }
};
} // namespace detail

} // namespace reflect

inline void to_variant(const reflect::signature& sig, variant& v) {
    v = sig.str();
}

inline void from_variant(const variant& v, reflect::signature& sig) {
    sig = reflect::signature(v.as_string());
}

} // namespace mc

#endif // MC_REFLECT_SIGNATURE_H