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
};

// 定义 variants 类型，用于存储 variant 数组
using variants = std::vector<variant>;

/**
 * @brief 将各种类型转换为 variant 类型的函数声明
 */
void to_variant(const blob& var, mc::variant& vo);
void from_variant(const mc::variant& var, blob& vo);

void to_variant(const uint8_t& var, mc::variant& vo);
void from_variant(const mc::variant& var, uint8_t& vo);
void to_variant(const int8_t& var, mc::variant& vo);
void from_variant(const mc::variant& var, int8_t& vo);

void to_variant(const uint16_t& var, mc::variant& vo);
void from_variant(const mc::variant& var, uint16_t& vo);
void to_variant(const int16_t& var, mc::variant& vo);
void from_variant(const mc::variant& var, int16_t& vo);

void to_variant(const uint32_t& var, mc::variant& vo);
void from_variant(const mc::variant& var, uint32_t& vo);
void to_variant(const int32_t& var, mc::variant& vo);
void from_variant(const mc::variant& var, int32_t& vo);

void to_variant(const uint64_t& var, mc::variant& vo);
void from_variant(const mc::variant& var, uint64_t& vo);
void to_variant(const int64_t& var, mc::variant& vo);
void from_variant(const mc::variant& var, int64_t& vo);

void to_variant(const double& var, mc::variant& vo);
void from_variant(const mc::variant& var, double& vo);
void to_variant(const float& var, mc::variant& vo);
void from_variant(const mc::variant& var, float& vo);

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
        blob_type      ///< 二进制数据类型
    };

    /**
     * @brief 默认构造函数，创建一个空的 variant
     */
    variant();

    /**
     * @brief 从 nullptr 构造一个空的 variant
     */
    variant(std::nullptr_t);

    /**
     * @brief 从各种基本类型构造 variant
     */
    variant(const char* str);
    variant(char* str);
    variant(float val);
    variant(uint8_t val);
    variant(int8_t val);
    variant(uint16_t val);
    variant(int16_t val);
    variant(uint32_t val);
    variant(int32_t val);
    variant(uint64_t val);
    variant(int64_t val);
    variant(double val);
    variant(bool val);
    variant(blob val);
    variant(std::string val);
    variant(dict obj);
    variant(mutable_dict obj);
    variant(variants arr);

    /**
     * @brief 从 std::pair 构造
     */
    template <typename K, typename T>
    variant(const std::pair<K, T>& pair);

    /**
     * @brief 拷贝构造函数
     */
    variant(const variant& other);

    /**
     * @brief 移动构造函数
     */
    variant(variant&& other) noexcept;

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
     * @brief 获取数组类型
     * @return 数组引用
     * @throw std::bad_cast 如果类型不匹配
     */
    const variants& get_array() const {
        if (m_type != type_id::array_type) {
            throw std::bad_cast();
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
            throw std::bad_cast();
        }
        return *static_cast<dict*>(m_object_ptr);
    }

private:
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

} // namespace mc

#endif // MC_VARIANT_H