/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
 * @file variant.h
 * @brief 定义了 mc 命名空间下的 variant 类，用于表示任意类型的数据
 */
#ifndef MC_VARIANT_H
#define MC_VARIANT_H

#include <array>
#include <cstdint>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <mc/common.h>

namespace mc {

// 前置声明
class variant;
class dict;
class mutable_dict;

// 定义 blob 类型，用于存储二进制数据
struct blob {
    std::vector<char> data;

    /**
     * @brief 比较两个 blob 对象是否相等
     * @param other 要比较的 blob 对象
     * @return 如果两个对象相等则返回 true，否则返回 false
     */
    bool operator==(const blob& other) const {
        return data == other.data;
    }

    /**
     * @brief 比较两个 blob 对象是否不相等
     * @param other 要比较的 blob 对象
     * @return 如果两个对象不相等则返回 true，否则返回 false
     */
    bool operator!=(const blob& other) const {
        return !(*this == other);
    }
};

// 定义 variants 类型，用于存储 variant 数组
using variants = std::vector<variant>;

/**
 * @brief 将各种类型转换为 variant 类型的函数声明
 */
void to_variant(const blob& var, mc::variant& vo);
void from_variant(const mc::variant& var, blob& vo);

void to_variant(const bool& var, mc::variant& vo);
void from_variant(const mc::variant& var, bool& vo);

void to_variant(const std::string& var, mc::variant& vo);
void from_variant(const mc::variant& var, std::string& vo);

void to_variant(const dict& var, mc::variant& vo);
void from_variant(const mc::variant& var, dict& vo);

void to_variant(const mutable_dict& var, mc::variant& vo);
void from_variant(const mc::variant& var, mutable_dict& vo);

void to_variant(const variants& var, mc::variant& vo);
void from_variant(const mc::variant& var, variants& vo);

void to_variant(const std::vector<char>& var, mc::variant& vo);
void from_variant(const mc::variant& var, std::vector<char>& vo);

/**
 * @brief variant 类，用于表示任意类型的数据
 */
class variant {
public:
    /**
     * @brief 数据类型枚举
     */
    enum class type_id {
        null_type = 0, ///< 空类型
        int8_type,     ///< 有符号8位整数
        uint8_type,    ///< 无符号8位整数
        int16_type,    ///< 有符号16位整数
        uint16_type,   ///< 无符号16位整数
        int32_type,    ///< 有符号32位整数
        uint32_type,   ///< 无符号32位整数
        int64_type,    ///< 有符号64位整数
        uint64_type,   ///< 无符号64位整数
        double_type,   ///< 双精度浮点数
        bool_type,     ///< 布尔类型
        string_type,   ///< 字符串类型
        array_type,    ///< 数组类型
        object_type,   ///< 对象类型
        blob_type,     ///< 二进制数据类型
        max_type       ///< 最大类型值（用于边界检查）
    };

    /**
     * @brief 默认构造函数，创建一个空的 variant
     */
    variant();

    /**
     * @brief 从 nullptr 构造一个空的 variant
     */
    explicit variant(std::nullptr_t);

    /**
     * @brief 从指定类型构造 variant
     */
    explicit variant(type_id type);

    /**
     * @brief 从各种基本类型构造 variant
     */
    explicit variant(const char* str);
    explicit variant(char* str);
    explicit variant(float val);
    explicit variant(uint8_t val);
    explicit variant(int8_t val);
    explicit variant(uint16_t val);
    explicit variant(int16_t val);
    explicit variant(uint32_t val);
    explicit variant(int32_t val);
    explicit variant(uint64_t val);
    explicit variant(int64_t val);
    explicit variant(double val);
    explicit variant(bool val);
    explicit variant(blob val);
    explicit variant(std::string val);
    explicit variant(dict obj);
    explicit variant(mutable_dict obj);
    explicit variant(variants arr);

    /**
     * @brief 从 std::pair 构造
     */
    template <typename K, typename T>
    explicit variant(const std::pair<K, T>& pair);

    /**
     * @brief 拷贝构造函数
     */
    explicit variant(const variant& other);

    /**
     * @brief 移动构造函数
     */
    explicit variant(variant&& other) noexcept;

    /**
     * @brief 析构函数
     */
    ~variant();

    /**
     * @brief 访问者接口，用于访问 variant 中的数据
     */
    class visitor {
    public:
        virtual ~visitor() {
        }

        virtual void handle() const = 0;
        virtual void handle(const int64_t& v) const = 0;
        virtual void handle(const uint64_t& v) const = 0;
        virtual void handle(const double& v) const = 0;
        virtual void handle(const bool& v) const = 0;
        virtual void handle(const std::string& v) const = 0;
        virtual void handle(const dict& v) const = 0;
        virtual void handle(const variants& v) const = 0;
        virtual void handle(const blob& v) const = 0;
    };

    /**
     * @brief 访问 variant 中的数据
     */
    void visit(const visitor& v) const;

    /**
     * @brief 获取 variant 的类型
     */
    type_id get_type() const;

    /**
     * @brief 判断 variant 是否为空
     */
    bool is_null() const;

    /**
     * @brief 判断 variant 是否为字符串
     */
    bool is_string() const;

    /**
     * @brief 判断 variant 是否为布尔值
     */
    bool is_bool() const;

    /**
     * @brief 判断 variant 是否为有符号8位整数
     */
    bool is_int8() const;

    /**
     * @brief 判断 variant 是否为无符号8位整数
     */
    bool is_uint8() const;

    /**
     * @brief 判断 variant 是否为有符号16位整数
     */
    bool is_int16() const;

    /**
     * @brief 判断 variant 是否为无符号16位整数
     */
    bool is_uint16() const;

    /**
     * @brief 判断 variant 是否为有符号32位整数
     */
    bool is_int32() const;

    /**
     * @brief 判断 variant 是否为无符号32位整数
     */
    bool is_uint32() const;

    /**
     * @brief 判断 variant 是否为有符号64位整数
     */
    bool is_int64() const;

    /**
     * @brief 判断 variant 是否为无符号64位整数
     */
    bool is_uint64() const;

    /**
     * @brief 判断 variant 是否为双精度浮点数
     */
    bool is_double() const;

    /**
     * @brief 判断 variant 是否为对象
     */
    bool is_object() const;

    /**
     * @brief 判断 variant 是否为数组
     */
    bool is_array() const;

    /**
     * @brief 判断 variant 是否为二进制数据
     */
    bool is_blob() const;

    /**
     * @brief 判断 variant 是否为数值类型（整数或浮点数）
     */
    bool is_numeric() const;

    /**
     * @brief 判断 variant 是否为整数类型
     */
    bool is_integer() const;

    /**
     * @brief 将 variant 转换为有符号64位整数
     */
    int64_t as_int64() const;

    /**
     * @brief 将 variant 转换为无符号64位整数
     */
    uint64_t as_uint64() const;

    /**
     * @brief 将 variant 转换为布尔值
     */
    bool as_bool() const;

    /**
     * @brief 将 variant 转换为双精度浮点数
     */
    double as_double() const;

    /**
     * @brief 将 variant 转换为二进制数据
     */
    blob as_blob() const;

    /**
     * @brief 将 variant 转换为字符串
     */
    std::string as_string() const;

    /**
     * @brief 获取数组中指定位置的元素
     */
    const variant& operator[](size_t pos) const;

    /**
     * @brief 获取数组的大小
     */
    size_t size() const;

    /**
     * @brief 将 variant 转换为指定类型
     */
    template <typename T>
    T as() const {
        T v;
        from_variant(*this, v);
        return v;
    }

    /**
     * @brief 将 variant 转换为指定类型，如果转换失败则返回默认值
     * @param default_value 转换失败时返回的默认值
     * @return 转换后的值或默认值
     */
    template <typename T>
    T as(const T& default_value) const {
        try {
            T v;
            from_variant(*this, v);
            return v;
        } catch (const std::exception&) {
            return default_value;
        }
    }

    /**
     * @brief 将 variant 转换为指定类型并存储到引用中
     */
    template <typename T>
    void as(T& v) const {
        from_variant(*this, v);
    }

    /**
     * @brief 赋值运算符
     */
    variant& operator=(const variant& other);
    variant& operator=(variant&& other) noexcept;

    /**
     * @brief 从各种类型赋值
     */
    template <typename T>
    variant& operator=(T&& v) {
        return *this = variant(std::forward<T>(v));
    }

    /**
     * @brief 从 std::optional 构造 variant
     */
    template <typename T>
    explicit variant(const std::optional<T>& v) {
        if (v.has_value()) {
            *this = variant(v.value());
        }
    }

    /**
     * @brief 清空 variant
     */
    void clear();

    /**
     * @brief 比较两个 variant 对象是否相等
     * @param other 要比较的 variant 对象
     * @return 如果两个对象相等则返回 true，否则返回 false
     */
    bool operator==(const variant& other) const;

    /**
     * @brief 比较两个 variant 对象是否不相等
     * @param other 要比较的 variant 对象
     * @return 如果两个对象不相等则返回 true，否则返回 false
     */
    bool operator!=(const variant& other) const {
        return !(*this == other);
    }

    /**
     * @brief
     * 比较运算符重载说明：整数类型只要求值相等；字符串类型支持与string_type和blob_type比较；其他类型要求类型和值精确匹配
     */
    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    bool operator==(const T& other) const {
        return as<T>() == other;
    }
    bool operator==(const char* other) const;
    bool operator==(const std::string& other) const;
    bool operator==(const std::string_view& other) const;
    bool operator==(const variants& other) const;
    bool operator==(const blob& other) const;
    bool operator==(const dict& other) const;
    bool operator==(const mutable_dict& other) const;
    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    friend bool operator==(const T& val, const variant& var) {
        return var == val;
    }
    friend bool operator==(const char* val, const variant& var) {
        return var == val;
    }
    friend bool operator==(const std::string& val, const variant& var) {
        return var == val;
    }
    friend bool operator==(const std::string_view& val, const variant& var) {
        return var == val;
    }
    friend bool operator==(const variants& val, const variant& var) {
        return var == val;
    }
    friend bool operator==(const blob& val, const variant& var) {
        return var == val;
    }
    friend bool operator==(const dict& val, const variant& var) {
        return var == val;
    }
    friend bool operator==(const mutable_dict& val, const variant& var) {
        return var == val;
    }

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    bool operator!=(const T& other) const {
        return !(*this == other);
    }
    bool operator!=(const char* other) const {
        return !(*this == other);
    }
    bool operator!=(const std::string& other) const {
        return !(*this == other);
    }
    bool operator!=(const std::string_view& other) const {
        return !(*this == other);
    }
    bool operator!=(const variants& other) const {
        return !(*this == other);
    }
    bool operator!=(const blob& other) const {
        return !(*this == other);
    }
    bool operator!=(const dict& other) const {
        return !(*this == other);
    }
    bool operator!=(const mutable_dict& other) const {
        return !(*this == other);
    }

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    friend bool operator!=(const T& val, const variant& var) {
        return !(var == val);
    }

    /**
     * @brief 将variant转换为bool类型
     * @return 对于bool类型的false，整数类型的0，浮点数类型的0返回false，其他情况返回true
     */
    explicit operator bool() const;
    friend bool operator!=(const char* val, const variant& var) {
        return !(var == val);
    }
    friend bool operator!=(const std::string& val, const variant& var) {
        return !(var == val);
    }
    friend bool operator!=(const std::string_view& val, const variant& var) {
        return !(var == val);
    }
    friend bool operator!=(const variants& val, const variant& var) {
        return !(var == val);
    }
    friend bool operator!=(const blob& val, const variant& var) {
        return !(var == val);
    }
    friend bool operator!=(const dict& val, const variant& var) {
        return !(var == val);
    }
    friend bool operator!=(const mutable_dict& val, const variant& var) {
        return !(var == val);
    }

    /**
     * @brief 获取数组类型
     * @return 数组引用
     * @throw std::bad_cast 如果类型不匹配
     */
    const variants& get_array() const {
        if (m_type != type_id::array_type) {
            throw std::runtime_error("类型错误: 期望类型为 array，实际类型为 " +
                                     std::string(get_type_name(m_type)));
        }
        return *static_cast<variants*>(m_array_ptr);
    }

    /**
     * @brief 获取对象类型
     * @return 对象引用
     * @throw std::bad_cast 如果类型不匹配
     */
    const dict& get_object() const {
        if (m_type != type_id::object_type) {
            throw std::runtime_error("类型错误: 期望类型为 object，实际类型为 " +
                                     std::string(get_type_name(m_type)));
        }
        return *static_cast<dict*>(m_object_ptr);
    }

    /**
     * @brief 获取类型名称
     * @param type 类型ID
     * @return 类型名称
     */
    static const char* get_type_name(type_id type);

protected:
    /**
     * @brief 存储数据的联合体
     */
    union {
        double m_double;
        int64_t m_int64;
        uint64_t m_uint64;
        bool m_bool;
        void* m_string_ptr;
        void* m_blob_ptr;
        void* m_array_ptr;
        void* m_object_ptr;
    };

    /**
     * @brief 数据类型
     */
    type_id m_type;
};

/**
 * @brief 类型锁定的 variant 类，赋值时保持类型不变
 */
class typed_variant : public variant {
public:
    /**
     * @brief 默认构造函数，创建一个空的 typed_variant
     */
    typed_variant();

    /**
     * @brief 从 variant 构造 typed_variant
     */
    explicit typed_variant(const variant& other);

    /**
     * @brief 从 variant 移动构造 typed_variant
     */
    explicit typed_variant(variant&& other) noexcept;

    /**
     * @brief 从各种基本类型构造 typed_variant
     */
    template <typename T>
    explicit typed_variant(T&& val) : variant(std::forward<T>(val)) {
    }

    /**
     * @brief 从 type_id 构造指定类型的 typed_variant
     */
    explicit typed_variant(type_id type);

    /**
     * @brief 赋值运算符，保持类型不变
     */
    typed_variant& operator=(const variant& other);

    /**
     * @brief 移动赋值运算符，保持类型不变
     */
    typed_variant& operator=(variant&& other);

    /**
     * @brief 从各种类型赋值，保持类型不变
     */
    template <typename T>
    std::enable_if_t<!std::is_same_v<std::decay_t<T>, variant>, typed_variant&> operator=(T&& v) {
        *this = variant(std::forward<T>(v));
        return *this;
    }

    /**
     * @brief 比较两个 typed_variant 对象是否相等
     * @param other 要比较的 typed_variant 对象
     * @return 如果两个对象相等则返回 true，否则返回 false
     */
    bool operator==(const typed_variant& other) const {
        return variant::operator==(other);
    }

    /**
     * @brief 比较 typed_variant 与 variant 对象是否相等
     * @param other 要比较的 variant 对象
     * @return 如果两个对象相等则返回 true，否则返回 false
     */
    bool operator==(const variant& other) const {
        return variant::operator==(other);
    }

    /**
     * @brief 比较两个 typed_variant 对象是否不相等
     * @param other 要比较的 typed_variant 对象
     * @return 如果两个对象不相等则返回 true，否则返回 false
     */
    bool operator!=(const typed_variant& other) const {
        return !(*this == other);
    }

    /**
     * @brief 比较 typed_variant 与 variant 对象是否不相等
     * @param other 要比较的 variant 对象
     * @return 如果两个对象不相等则返回 true，否则返回 false
     */
    bool operator!=(const variant& other) const {
        return !(*this == other);
    }
};

template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
void to_variant(const T& var, mc::variant& vo) {
    vo = variant(var);
}

template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
void from_variant(const mc::variant& var, T& vo) {
    if (std::is_signed_v<T>) {
        vo = static_cast<T>(var.as_int64());
    } else {
        vo = static_cast<T>(var.as_uint64());
    }
}

template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
void to_variant(const T& var, mc::variant& vo) {
    vo = variant(static_cast<double>(var));
}

template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
void from_variant(const mc::variant& var, T& vo) {
    vo = static_cast<T>(var.as_double());
}

} // namespace mc

#endif // MC_VARIANT_H