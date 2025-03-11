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
 * @file variant.cpp
 * @brief 实现 variant.h 中声明的方法
 */
#include <cstring>
#include <limits>
#include <mc/dict.h>
#include <mc/mutable_dict.h>
#include <mc/variant.h>
#include <stdexcept>

namespace mc {

// 辅助函数，用于抛出类型错误异常
// 辅助函数：获取类型的整数值
static std::string get_type_value(variant::type_id type) {
    return std::to_string(static_cast<int>(type)) + " (" + [type]() {
        switch (type) {
        case variant::type_id::null_type:
            return "null_type";
        case variant::type_id::int8_type:
            return "int8_type";
        case variant::type_id::uint8_type:
            return "uint8_type";
        case variant::type_id::int16_type:
            return "int16_type";
        case variant::type_id::uint16_type:
            return "uint16_type";
        case variant::type_id::int32_type:
            return "int32_type";
        case variant::type_id::uint32_type:
            return "uint32_type";
        case variant::type_id::int64_type:
            return "int64_type";
        case variant::type_id::uint64_type:
            return "uint64_type";
        case variant::type_id::double_type:
            return "double_type";
        case variant::type_id::bool_type:
            return "bool_type";
        case variant::type_id::string_type:
            return "string_type";
        case variant::type_id::array_type:
            return "array_type";
        case variant::type_id::object_type:
            return "object_type";
        case variant::type_id::blob_type:
            return "blob_type";
        default:
            return "未知类型";
        }
    }() + ")";
}

static void throw_type_error(const char* expected_type, variant::type_id actual_type) {
    std::string error = "类型错误：期望 ";
    error += expected_type;
    error += "，实际为 " + get_type_value(actual_type);
    throw std::runtime_error(error);
}

static void throw_unknow_type_error(variant::type_id actual_type) {
    throw std::runtime_error("类型错误：" + get_type_value(actual_type));
}

// 从 type_id 构造
variant::variant(type_id type) : variant() {
    m_type = type;
    switch (type) {
    case type_id::string_type:
        m_string_ptr = new std::string();
        break;
    case type_id::array_type:
        m_array_ptr = new variants();
        break;
    case type_id::object_type:
        m_object_ptr = new dict();
        break;
    case type_id::blob_type:
        m_blob_ptr = new blob();
        break;
    default:
        break;
    }
}

// 默认构造函数
variant::variant() : m_uint64(0), m_type(type_id::null_type) {
    static_assert(sizeof(uint64_t) >= sizeof(void*) && sizeof(uint64_t) >= sizeof(double),
                  "uint64_t 不是联合体中最大的成员");
}

// 从 nullptr 构造
variant::variant(std::nullptr_t) : variant() {
}

// 从字符串构造
variant::variant(const char* str) : variant() {
    if (str) {
        m_type = type_id::string_type;
        m_string_ptr = new std::string(str);
    }
}

variant::variant(char* str) : variant(static_cast<const char*>(str)) {
}

// 从基本类型构造
variant::variant(float val) : m_double(val), m_type(type_id::double_type) {
}
variant::variant(uint8_t val) : m_uint64(val), m_type(type_id::uint8_type) {
}
variant::variant(int8_t val) : m_int64(val), m_type(type_id::int8_type) {
}
variant::variant(uint16_t val) : m_uint64(val), m_type(type_id::uint16_type) {
}
variant::variant(int16_t val) : m_int64(val), m_type(type_id::int16_type) {
}
variant::variant(uint32_t val) : m_uint64(val), m_type(type_id::uint32_type) {
}
variant::variant(int32_t val) : m_int64(val), m_type(type_id::int32_type) {
}
variant::variant(uint64_t val) : m_uint64(val), m_type(type_id::uint64_type) {
}
variant::variant(int64_t val) : m_int64(val), m_type(type_id::int64_type) {
}
variant::variant(double val) : m_double(val), m_type(type_id::double_type) {
}
variant::variant(bool val) : m_bool(val), m_type(type_id::bool_type) {
}

// 从 blob 构造
variant::variant(blob val) : variant() {
    m_type = type_id::blob_type;
    m_blob_ptr = new blob(std::move(val));
}

// 从 std::string 构造
variant::variant(std::string val) : variant() {
    m_type = type_id::string_type;
    m_string_ptr = new std::string(std::move(val));
}

// 从 dict 构造
variant::variant(dict obj) : variant() {
    m_type = type_id::object_type;
    m_object_ptr = new dict(std::move(obj));
}

// 从 mutable_dict 构造
variant::variant(mutable_dict obj) : variant() {
    m_type = type_id::object_type;
    m_object_ptr = new dict(std::move(obj));
}

// 从 variants 构造
variant::variant(variants arr) : variant() {
    m_type = type_id::array_type;
    m_array_ptr = new variants(std::move(arr));
}

// 重置 variant 内容
void variant::clear() {
    switch (m_type) {
    case type_id::null_type:
        break;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        m_int64 = 0;
        break;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        m_uint64 = 0;
        break;
    case type_id::double_type:
        m_double = 0.0;
        break;
    case type_id::bool_type:
        m_bool = false;
        break;
    case type_id::string_type:
        delete static_cast<std::string*>(m_string_ptr);
        m_string_ptr = nullptr;
        break;
    case type_id::array_type:
        delete static_cast<variants*>(m_array_ptr);
        m_array_ptr = nullptr;
        break;
    case type_id::object_type:
        delete static_cast<dict*>(m_object_ptr);
        m_object_ptr = nullptr;
        break;
    case type_id::blob_type:
        delete static_cast<blob*>(m_blob_ptr);
        m_blob_ptr = nullptr;
        break;
    default:
        throw_unknow_type_error(m_type);
        break;
    }
    m_type = type_id::null_type;
}

// 拷贝构造函数
variant::variant(const variant& other) : variant() {
    switch (other.m_type) {
    case type_id::null_type:
        break;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        m_int64 = other.m_int64;
        break;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        m_uint64 = other.m_uint64;
        break;
    case type_id::double_type:
        m_double = other.m_double;
        break;
    case type_id::bool_type:
        m_bool = other.m_bool;
        break;
    case type_id::string_type:
        m_string_ptr = new std::string(*static_cast<std::string*>(other.m_string_ptr));
        break;
    case type_id::array_type:
        m_array_ptr = new variants(*static_cast<variants*>(other.m_array_ptr));
        break;
    case type_id::object_type:
        m_object_ptr = new dict(*static_cast<dict*>(other.m_object_ptr));
        break;
    case type_id::blob_type:
        m_blob_ptr = new blob(*static_cast<blob*>(other.m_blob_ptr));
        break;
    default:
        break;
    }
    m_type = other.m_type;
}

// 移动构造函数
variant::variant(variant&& other) noexcept : variant() {
    m_type = other.m_type;

    switch (m_type) {
    case type_id::null_type:
        break;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        m_int64 = other.m_int64;
        break;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        m_uint64 = other.m_uint64;
        break;
    case type_id::double_type:
        m_double = other.m_double;
        break;
    case type_id::bool_type:
        m_bool = other.m_bool;
        break;
    case type_id::string_type:
        m_string_ptr = other.m_string_ptr;
        other.m_string_ptr = nullptr;
        break;
    case type_id::array_type:
        m_array_ptr = other.m_array_ptr;
        other.m_array_ptr = nullptr;
        break;
    case type_id::object_type:
        m_object_ptr = other.m_object_ptr;
        other.m_object_ptr = nullptr;
        break;
    case type_id::blob_type:
        m_blob_ptr = other.m_blob_ptr;
        other.m_blob_ptr = nullptr;
        break;
    default:
        break;
    }

    other.m_type = type_id::null_type;
}

// 移动赋值运算符
variant& variant::operator=(variant&& other) noexcept {
    if (this != &other) {
        clear();

        switch (other.m_type) {
        case type_id::null_type:
            break;
        case type_id::int8_type:
        case type_id::int16_type:
        case type_id::int32_type:
        case type_id::int64_type:
            m_int64 = other.m_int64;
            break;
        case type_id::uint8_type:
        case type_id::uint16_type:
        case type_id::uint32_type:
        case type_id::uint64_type:
            m_uint64 = other.m_uint64;
            break;
        case type_id::double_type:
            m_double = other.m_double;
            break;
        case type_id::bool_type:
            m_bool = other.m_bool;
            break;
        case type_id::string_type:
            m_string_ptr = other.m_string_ptr;
            other.m_string_ptr = nullptr;
            break;
        case type_id::array_type:
            m_array_ptr = other.m_array_ptr;
            other.m_array_ptr = nullptr;
            break;
        case type_id::object_type:
            m_object_ptr = other.m_object_ptr;
            other.m_object_ptr = nullptr;
            break;
        case type_id::blob_type:
            m_blob_ptr = other.m_blob_ptr;
            other.m_blob_ptr = nullptr;
            break;
        default:
            break;
        }
        m_type = other.m_type;
        other.m_type = type_id::null_type;
    }
    return *this;
}

// 访问 variant 中的数据
void variant::visit(const visitor& v) const {
    switch (m_type) {
    case type_id::null_type:
        v.handle();
        break;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        v.handle(m_int64);
        break;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        v.handle(m_uint64);
        break;
    case type_id::double_type:
        v.handle(m_double);
        break;
    case type_id::bool_type:
        v.handle(m_bool);
        break;
    case type_id::string_type:
        v.handle(*static_cast<std::string*>(m_string_ptr));
        break;
    case type_id::object_type:
        v.handle(*static_cast<dict*>(m_object_ptr));
        break;
    case type_id::array_type:
        v.handle(*static_cast<variants*>(m_array_ptr));
        break;
    case type_id::blob_type:
        v.handle(*static_cast<blob*>(m_blob_ptr));
        break;
    default:
        throw_unknow_type_error(m_type);
        break;
    }
}

// 获取 variant 的类型
variant::type_id variant::get_type() const {
    return m_type;
}

// 判断 variant 是否为空
bool variant::is_null() const {
    return m_type == type_id::null_type;
}

// 判断 variant 是否为字符串
bool variant::is_string() const {
    return m_type == type_id::string_type;
}

// 判断 variant 是否为布尔值
bool variant::is_bool() const {
    return m_type == type_id::bool_type;
}

// 判断 variant 是否为有符号64位整数
bool variant::is_int64() const {
    return m_type == type_id::int64_type;
}

// 判断 variant 是否为无符号64位整数
bool variant::is_uint64() const {
    return m_type == type_id::uint64_type;
}

// 判断 variant 是否为双精度浮点数
bool variant::is_double() const {
    return m_type == type_id::double_type;
}

// 判断 variant 是否为对象
bool variant::is_object() const {
    return m_type == type_id::object_type;
}

// 判断 variant 是否为数组
bool variant::is_array() const {
    return m_type == type_id::array_type;
}

// 判断 variant 是否为二进制数据
bool variant::is_blob() const {
    return m_type == type_id::blob_type;
}

// 判断 variant 是否为数值类型（整数或浮点数）
bool variant::is_numeric() const {
    return is_int8() || is_uint8() || is_int16() || is_uint16() || is_int32() || is_uint32() ||
           is_int64() || is_uint64() || is_double();
}

// 判断 variant 是否为整数类型
bool variant::is_integer() const {
    return is_int8() || is_uint8() || is_int16() || is_uint16() || is_int32() || is_uint32() ||
           is_int64() || is_uint64();
}

// 将 variant 转换为有符号64位整数
int64_t variant::as_int64() const {
    switch (m_type) {
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return m_int64;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        if (m_uint64 > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            throw std::overflow_error("uint64_t值超出int64_t范围");
        }
        return static_cast<int64_t>(m_uint64);
    case type_id::double_type:
        return static_cast<int64_t>(m_double);
    case type_id::bool_type:
        return m_bool ? 1 : 0;
    case type_id::string_type:
        try {
            return std::stoll(*static_cast<std::string*>(m_string_ptr));
        } catch (const std::exception& e) {
            throw_type_error("int64", m_type);
        }
    default:
        throw_type_error("int64", m_type);
    }
    return 0;
}

// 将 variant 转换为无符号64位整数
uint64_t variant::as_uint64() const {
    switch (m_type) {
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return m_uint64;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        if (m_int64 < 0) {
            throw std::underflow_error("int64_t值为负数，无法转换为uint64_t");
        }
        return static_cast<uint64_t>(m_int64);
    case type_id::double_type:
        if (m_double < 0) {
            throw std::underflow_error("double值为负数，无法转换为uint64_t");
        }
        return static_cast<uint64_t>(m_double);
    case type_id::bool_type:
        return m_bool ? 1 : 0;
    case type_id::string_type:
        try {
            return std::stoull(*static_cast<std::string*>(m_string_ptr));
        } catch (const std::exception& e) {
            throw_type_error("uint64", m_type);
        }
    default:
        throw_type_error("uint64", m_type);
    }
    return 0;
}

// 将 variant 转换为布尔值
bool variant::as_bool() const {
    switch (m_type) {
    case type_id::bool_type:
        return m_bool;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return m_int64 != 0;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return m_uint64 != 0;
    case type_id::double_type:
        return m_double != 0;
    case type_id::string_type: {
        const std::string& s = *static_cast<std::string*>(m_string_ptr);
        if (s == "true" || s == "1") {
            return true;
        } else if (s == "false" || s == "0") {
            return false;
        } else {
            throw std::runtime_error("无法将字符串转换为 bool：" + s);
        }
    }
    case type_id::null_type:
        return false;
    default:
        throw_type_error("bool", m_type);
        return false; // 不会执行到这里
    }
}

// 将 variant 转换为双精度浮点数
double variant::as_double() const {
    switch (m_type) {
    case type_id::double_type:
        return m_double;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return static_cast<double>(m_int64);
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return static_cast<double>(m_uint64);
    case type_id::bool_type:
        return m_bool ? 1.0 : 0.0;
    case type_id::string_type: {
        try {
            return std::stod(*static_cast<std::string*>(m_string_ptr));
        } catch (const std::exception& e) {
            throw std::runtime_error("无法将字符串转换为 double：" + std::string(e.what()));
        }
    }
    default:
        throw_type_error("double", m_type);
        return 0.0; // 不会执行到这里
    }
}

// 将 variant 转换为二进制数据
blob variant::as_blob() const {
    if (m_type == type_id::blob_type) {
        return *static_cast<blob*>(m_blob_ptr);
    } else if (m_type == type_id::string_type) {
        const std::string& s = *static_cast<std::string*>(m_string_ptr);
        blob b;
        b.data.assign(s.begin(), s.end());
        return b;
    } else {
        throw_type_error("blob", m_type);
        return blob(); // 不会执行到这里
    }
}

// 将 variant 转换为字符串
std::string variant::as_string() const {
    switch (m_type) {
    case type_id::string_type:
        return *static_cast<std::string*>(m_string_ptr);
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return std::to_string(m_int64);
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return std::to_string(m_uint64);
    case type_id::double_type:
        return std::to_string(m_double);
    case type_id::bool_type:
        return m_bool ? "true" : "false";
    case type_id::null_type:
        return "null";
    case type_id::blob_type: {
        const blob& b = *static_cast<blob*>(m_blob_ptr);
        return std::string(b.data.begin(), b.data.end());
    }
    default:
        throw_type_error("string", m_type);
        return std::string(); // 不会执行到这里
    }
}

// 获取数组中指定位置的元素
const variant& variant::operator[](size_t pos) const {
    if (m_type != type_id::array_type) {
        throw_type_error("array", m_type);
    }

    const variants& arr = *static_cast<variants*>(m_array_ptr);
    if (pos >= arr.size()) {
        throw std::out_of_range("数组索引越界");
    }

    return arr[pos];
}

// 获取数组的大小
size_t variant::size() const {
    switch (m_type) {
    case type_id::array_type:
        return static_cast<variants*>(m_array_ptr)->size();
    case type_id::object_type:
        return static_cast<dict*>(m_object_ptr)->size();
    case type_id::string_type:
        return static_cast<std::string*>(m_string_ptr)->size();
    case type_id::blob_type:
        return static_cast<blob*>(m_blob_ptr)->data.size();
    default:
        return 0;
    }
}

// 基本类型转换函数实现
void to_variant(const blob& var, mc::variant& vo) {
    vo = variant(var);
}

void from_variant(const mc::variant& var, blob& vo) {
    vo = var.as_blob();
}

/**
 * @brief 整数类型 to_variant 的通用实现
 */
template <typename IntType>
void int_to_variant(const IntType& var, mc::variant& vo) {
    vo = variant(var);
}

/**
 * @brief 有符号整数类型 from_variant 的通用实现
 */
template <typename IntType>
void signed_int_from_variant(const mc::variant& var, IntType& vo, variant::type_id specific_type) {
    if (var.get_type() == specific_type) {
        vo = static_cast<IntType>(var.as_int64());
    } else {
        int64_t value = var.as_int64();
        if (value < std::numeric_limits<IntType>::min() ||
            value > std::numeric_limits<IntType>::max()) {
            throw std::overflow_error("值超出范围");
        }
        vo = static_cast<IntType>(value);
    }
}

/**
 * @brief 无符号整数类型 from_variant 的通用实现
 */
template <typename IntType>
void unsigned_int_from_variant(const mc::variant& var, IntType& vo,
                               variant::type_id specific_type) {
    if (var.get_type() == specific_type) {
        vo = static_cast<IntType>(var.as_uint64());
    } else {
        uint64_t value = var.as_uint64();
        if (value > std::numeric_limits<IntType>::max()) {
            throw std::overflow_error("值超出范围");
        }
        vo = static_cast<IntType>(value);
    }
}

// uint8_t 实现
void to_variant(const uint8_t& var, mc::variant& vo) {
    int_to_variant(var, vo);
}

void from_variant(const mc::variant& var, uint8_t& vo) {
    unsigned_int_from_variant(var, vo, variant::type_id::uint8_type);
}

// int8_t 实现
void to_variant(const int8_t& var, mc::variant& vo) {
    int_to_variant(var, vo);
}

void from_variant(const mc::variant& var, int8_t& vo) {
    signed_int_from_variant(var, vo, variant::type_id::int8_type);
}

// uint16_t 实现
void to_variant(const uint16_t& var, mc::variant& vo) {
    int_to_variant(var, vo);
}

void from_variant(const mc::variant& var, uint16_t& vo) {
    unsigned_int_from_variant(var, vo, variant::type_id::uint16_type);
}

// int16_t 实现
void to_variant(const int16_t& var, mc::variant& vo) {
    int_to_variant(var, vo);
}

void from_variant(const mc::variant& var, int16_t& vo) {
    signed_int_from_variant(var, vo, variant::type_id::int16_type);
}

// uint32_t 实现
void to_variant(const uint32_t& var, mc::variant& vo) {
    int_to_variant(var, vo);
}

void from_variant(const mc::variant& var, uint32_t& vo) {
    unsigned_int_from_variant(var, vo, variant::type_id::uint32_type);
}

// int32_t 实现
void to_variant(const int32_t& var, mc::variant& vo) {
    int_to_variant(var, vo);
}

void from_variant(const mc::variant& var, int32_t& vo) {
    signed_int_from_variant(var, vo, variant::type_id::int32_type);
}

// uint64_t 实现
void to_variant(const uint64_t& var, mc::variant& vo) {
    int_to_variant(var, vo);
}

void from_variant(const mc::variant& var, uint64_t& vo) {
    unsigned_int_from_variant(var, vo, variant::type_id::uint64_type);
}

// int64_t 实现
void to_variant(const int64_t& var, mc::variant& vo) {
    int_to_variant(var, vo);
}

void from_variant(const mc::variant& var, int64_t& vo) {
    signed_int_from_variant(var, vo, variant::type_id::int64_type);
}

void to_variant(const double& var, mc::variant& vo) {
    vo = variant(var);
}

void from_variant(const mc::variant& var, double& vo) {
    vo = var.as_double();
}

void to_variant(const float& var, mc::variant& vo) {
    vo = variant(static_cast<double>(var));
}

void from_variant(const mc::variant& var, float& vo) {
    vo = static_cast<float>(var.as_double());
}

void to_variant(const bool& var, mc::variant& vo) {
    vo = variant(var);
}

void from_variant(const mc::variant& var, bool& vo) {
    vo = var.as_bool();
}

void to_variant(const std::string& var, mc::variant& vo) {
    vo = variant(var);
}

void from_variant(const mc::variant& var, std::string& vo) {
    vo = var.as_string();
}

void to_variant(const dict& var, mc::variant& vo) {
    vo = variant(var);
}

void from_variant(const mc::variant& var, mc::dict& vo) {
    if (!var.is_object()) {
        throw std::bad_cast();
    }
    vo = var.get_object();
}

void to_variant(const mutable_dict& var, mc::variant& vo) {
    vo = variant(var);
}

void from_variant(const mc::variant& var, mc::mutable_dict& vo) {
    if (!var.is_object()) {
        throw std::bad_cast();
    }
    vo = var.get_object();
}

void to_variant(const variants& var, mc::variant& vo) {
    vo = variant(var);
}

void from_variant(const mc::variant& var, mc::variants& vo) {
    if (!var.is_array()) {
        throw std::bad_cast();
    }
    vo = var.get_array();
}

void to_variant(const std::vector<char>& var, mc::variant& vo) {
    blob b;
    b.data = var;
    vo = variant(std::move(b));
}

void from_variant(const mc::variant& var, std::vector<char>& vo) {
    blob b = var.as_blob();
    vo = std::move(b.data);
}

// 类型检查实现
bool variant::is_int8() const {
    return m_type == type_id::int8_type;
}

bool variant::is_uint8() const {
    return m_type == type_id::uint8_type;
}

bool variant::is_int16() const {
    return m_type == type_id::int16_type;
}

bool variant::is_uint16() const {
    return m_type == type_id::uint16_type;
}

bool variant::is_int32() const {
    return m_type == type_id::int32_type;
}

bool variant::is_uint32() const {
    return m_type == type_id::uint32_type;
}

// 添加析构函数
variant::~variant() {
    clear();
}

// 拷贝赋值运算符
variant& variant::operator=(const variant& other) {
    if (this != &other) {
        clear();

        switch (other.m_type) {
        case type_id::null_type:
            break;
        case type_id::int8_type:
        case type_id::int16_type:
        case type_id::int32_type:
        case type_id::int64_type:
            m_int64 = other.m_int64;
            break;
        case type_id::uint8_type:
        case type_id::uint16_type:
        case type_id::uint32_type:
        case type_id::uint64_type:
            m_uint64 = other.m_uint64;
            break;
        case type_id::double_type:
            m_double = other.m_double;
            break;
        case type_id::bool_type:
            m_bool = other.m_bool;
            break;
        case type_id::string_type:
            m_string_ptr = new std::string(*static_cast<std::string*>(other.m_string_ptr));
            break;
        case type_id::array_type:
            m_array_ptr = new variants(*static_cast<variants*>(other.m_array_ptr));
            break;
        case type_id::object_type:
            m_object_ptr = new dict(*static_cast<dict*>(other.m_object_ptr));
            break;
        case type_id::blob_type:
            m_blob_ptr = new blob(*static_cast<blob*>(other.m_blob_ptr));
            break;
        default:
            throw_unknow_type_error(other.m_type);
            break;
        }
        m_type = other.m_type;
    }
    return *this;
}

// typed_variant 实现

typed_variant::typed_variant() : variant() {
}

typed_variant::typed_variant(const variant& other) : variant(other) {
}

typed_variant::typed_variant(variant&& other) noexcept : variant(std::move(other)) {
}

typed_variant::typed_variant(type_id type) : variant(type) {
}

typed_variant& typed_variant::operator=(const variant& other) {
    if (this == &other) {
        return *this;
    }

    if (m_type == other.get_type() || m_type == type_id::null_type) {
        variant::operator=(other);
    } else {
        switch (m_type) {
        case type_id::int8_type: {
            int8_t val;
            from_variant(other, val);
            m_int64 = val;
            break;
        }
        case type_id::uint8_type: {
            uint8_t val;
            from_variant(other, val);
            m_uint64 = val;
            break;
        }
        case type_id::int16_type: {
            int16_t val;
            from_variant(other, val);
            m_int64 = val;
            break;
        }
        case type_id::uint16_type: {
            uint16_t val;
            from_variant(other, val);
            m_uint64 = val;
            break;
        }
        case type_id::int32_type: {
            int32_t val;
            from_variant(other, val);
            m_int64 = val;
            break;
        }
        case type_id::uint32_type: {
            uint32_t val;
            from_variant(other, val);
            m_uint64 = val;
            break;
        }
        case type_id::int64_type: {
            from_variant(other, m_int64);
            break;
        }
        case type_id::uint64_type: {
            from_variant(other, m_uint64);
            break;
        }
        case type_id::double_type: {
            from_variant(other, m_double);
            break;
        }
        case type_id::bool_type: {
            from_variant(other, m_bool);
            break;
        }
        case type_id::string_type: {
            from_variant(other, *static_cast<std::string*>(m_string_ptr));
            break;
        }
        case type_id::array_type:
            from_variant(other, *static_cast<variants*>(m_array_ptr));
            break;
        case type_id::object_type:
            from_variant(other, *static_cast<dict*>(m_object_ptr));
            break;
        case type_id::blob_type:
            from_variant(other, *static_cast<blob*>(m_blob_ptr));
            break;
        default:
            throw_unknow_type_error(m_type);
            break;
        }
    }
    return *this;
}

typed_variant& typed_variant::operator=(variant&& other) {
    if (this != &other) {
        if (get_type() == other.get_type()) {
            variant::operator=(std::move(other));
        } else {
            *this = other;
        }
    }
    return *this;
}

bool variant::operator==(const variant& other) const {
    // 如果类型不同，则不相等
    if (m_type != other.m_type) {
        return false;
    }

    // 根据类型比较值
    switch (m_type) {
    case type_id::null_type:
        return true; // 两个 null 类型总是相等的
    case type_id::bool_type:
        return m_bool == other.m_bool;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return m_int64 == other.m_int64;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return m_uint64 == other.m_uint64;
    case type_id::double_type:
        return m_double == other.m_double;
    case type_id::string_type:
        return *static_cast<std::string*>(m_string_ptr) ==
               *static_cast<std::string*>(other.m_string_ptr);
    case type_id::array_type:
        return *static_cast<variants*>(m_array_ptr) == *static_cast<variants*>(other.m_array_ptr);
    case type_id::object_type:
        return *static_cast<dict*>(m_object_ptr) == *static_cast<dict*>(other.m_object_ptr);
    case type_id::blob_type:
        return *static_cast<blob*>(m_blob_ptr) == *static_cast<blob*>(other.m_blob_ptr);
    default:
        return false;
    }
}

} // namespace mc