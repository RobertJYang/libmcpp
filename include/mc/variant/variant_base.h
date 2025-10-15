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
 * @file variant_base.h
 * @brief 定义了 mc 命名空间下的 variant_base 模板类，用于表示任意类型的数据
 */
#ifndef MC_VARIANT_VARIANT_BASE_H
#define MC_VARIANT_VARIANT_BASE_H

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <mc/common.h>
#include <mc/dict/dict.h>
#include <mc/json.h>
#include <mc/memory/allocator.h>
#include <mc/pretty_name.h>
#include <mc/string.h>
#include <mc/traits.h>
#include <mc/variant/variant_common.h>
#include <mc/variant/variant_extension.h>

namespace mc {

/**
 * @brief variant_base 类，用于表示任意类型的数据
 */
template <typename Config = variant_config<>>
class variant_base {
public:
    using is_variant = std::true_type;

    using allocator_type                = typename Config::allocator_type;
    using string_type                   = typename Config::string_type;
    using object_type                   = typename Config::object_type;
    using array_type                    = typename Config::array_type;
    using blob_type                     = typename Config::blob_type;
    using extension_type                = typename Config::extension_type;
    static constexpr bool is_fixed_type = Config::is_fixed_type;
    using string_ptr_type               = typename Config::string_ptr_type;
    using array_ptr_type                = typename Config::array_ptr_type;
    using blob_ptr_type                 = typename Config::blob_ptr_type;
    using extension_ptr_type            = typename Config::extension_ptr_type;

    template <typename OtherConfig>
    friend class variant_base;

    variant_base() : m_uint64(0), m_type(type_id::null_type) {
        static_assert(sizeof(uint64_t) >= sizeof(void*) && sizeof(uint64_t) >= sizeof(double),
                      "uint64_t 不是联合体中最大的成员");
    }
    variant_base(std::nullptr_t) : variant_base() {
    }
    variant_base(type_id type) : m_uint64(0), m_type(type) {
        switch (type) {
        case type_id::string_type:
            m_string_ptr = mc::allocate_ptr<string_type>(m_alloc);
            break;
        case type_id::array_type:
            m_array_ptr = mc::allocate_ptr<array_type>(m_alloc);
            break;
        case type_id::object_type:
            new (&m_object) object_type();
            break;
        case type_id::blob_type:
            m_blob_ptr = mc::allocate_ptr<blob_type>(m_alloc);
            break;
        case type_id::extension_type:
            new (&m_extension) extension_ptr_type();
            break;
        default:
            break;
        }
    }

    template <typename T, std::enable_if_t<std::is_fundamental_v<T>, int> = 0>
    variant_base(type_id type, T val) {
        if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
            m_int64 = static_cast<int64_t>(val);
        } else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
            m_uint64 = static_cast<uint64_t>(val);
        } else if constexpr (std::is_floating_point_v<T>) {
            m_double = static_cast<double>(val);
        } else if constexpr (std::is_same_v<T, bool>) {
            m_bool = static_cast<bool>(val);
        }
        m_type = type;
    }

    /**
     * @brief 从字符串构造 variant_base
     */
    variant_base(const char* str, const allocator_type& alloc = allocator_type())
        : variant_base(std::string_view(str), alloc) {
    }
    variant_base(char* str, const allocator_type& alloc = allocator_type())
        : variant_base(std::string_view(str), alloc) {
    }
    variant_base(const std::string& str, const allocator_type& alloc = allocator_type())
        : variant_base(std::string_view(str), alloc) {
    }
    variant_base(std::string_view str, const allocator_type& alloc = allocator_type())
        : m_uint64(0), m_type(type_id::string_type), m_alloc(alloc) {
        m_string_ptr = mc::allocate_ptr<string_type>(m_alloc, str.data(), str.size(), m_alloc);
    }

    /*
     * 从各种基础类型构造 variant_base
     */
    variant_base(float val) : variant_base(type_id::double_type, val) {
    }
    variant_base(uint8_t val) : variant_base(type_id::uint8_type, val) {
    }
    variant_base(int8_t val) : variant_base(type_id::int8_type, val) {
    }
    variant_base(uint16_t val) : variant_base(type_id::uint16_type, val) {
    }
    variant_base(int16_t val) : variant_base(type_id::int16_type, val) {
    }
    variant_base(uint32_t val) : variant_base(type_id::uint32_type, val) {
    }
    variant_base(int32_t val) : variant_base(type_id::int32_type, val) {
    }
    variant_base(uint64_t val) : variant_base(type_id::uint64_type, val) {
    }
    variant_base(int64_t val) : variant_base(type_id::int64_type, val) {
    }
    variant_base(double val) : variant_base(type_id::double_type, val) {
    }
    variant_base(bool val) : variant_base(type_id::bool_type, val) {
    }
    template <typename OtherAllocator>
    variant_base(const blob_base<OtherAllocator>& val) : variant_base(type_id::blob_type) {
        m_blob_ptr =
            mc::allocate_ptr<blob_type>(m_alloc, val.data.data(), val.data.size(), m_alloc);
    }

    // 从字典构造 variant_base
    variant_base(const dict& obj) : variant_base(type_id::object_type) {
        new (&m_object) object_type(obj);
    }
    variant_base(mutable_dict& obj) : variant_base(type_id::object_type) {
        new (&m_object) object_type(obj);
    }

    // 从 array_type 构造 variant_base
    variant_base(const array_type& arr, const allocator_type& alloc = allocator_type())
        : m_type(type_id::array_type), m_alloc(alloc) {
        m_array_ptr = mc::allocate_ptr<array_type>(m_alloc, arr, alloc);
    }
    template <typename OtherConfig>
    variant_base(const variants_base<OtherConfig>& arr,
                 const allocator_type&             alloc = allocator_type())
        : m_type(type_id::array_type), m_alloc(alloc) {
        m_array_ptr = mc::allocate_ptr<array_type>(m_alloc, alloc);
        for (const auto& item : arr) {
            m_array_ptr->emplace_back(item, alloc);
        }
    }

    /**
     * @brief 从 std::optional 构造 variant_base
     */
    template <typename T>
    variant_base(const std::optional<T>& v) : variant_base() {
        if (v.has_value()) {
            variant_base(v.value()).swap(*this);
        }
    }

    template <typename T, std::enable_if_t<std::is_base_of_v<variant_extension_base, T>, int> = 0>
    variant_base(mc::shared_ptr<T> ext, const allocator_type& alloc = allocator_type())
        : m_type(type_id::extension_type), m_alloc(alloc) {
        new (&m_extension) extension_ptr_type(mc::static_pointer_cast<variant_extension_base>(ext));
    }

    template <typename T, std::enable_if_t<!is_variant_v<T> && !std::is_pointer_v<T>, int> = 0>
    variant_base(const T& obj) : variant_base() {
        to_variant(obj, *this);
    }

    /**
     * @brief 拷贝构造函数
     */
    variant_base(const variant_base& other);

    /**
     * @brief 移动构造函数
     */
    variant_base(variant_base&& other) noexcept {
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
            break;
        case type_id::array_type:
            m_array_ptr = other.m_array_ptr;
            break;
        case type_id::object_type:
            new (&m_object) object_type(std::move(other.m_object));
            break;
        case type_id::blob_type:
            m_blob_ptr = other.m_blob_ptr;
            break;
        case type_id::extension_type:
            new (&m_extension) extension_ptr_type(std::move(other.m_extension));
            break;
        default:
            break;
        }

        other.m_type   = type_id::null_type;
        other.m_uint64 = 0;
    }

    // 从其他配置的 variant_base 构造
    template <typename OtherConfig>
    variant_base(const variant_base<OtherConfig>& other,
                 const allocator_type&            alloc = allocator_type());

    /**
     * @brief 析构函数
     */
    ~variant_base() {
        clear();
    }

    /**
     * @brief 访问者接口，用于访问 variant_base 中的数据
     */
    class visitor {
    public:
        virtual ~visitor() {
        }

        virtual void handle() const                                = 0;
        virtual void handle(const int64_t& v) const                = 0;
        virtual void handle(const uint64_t& v) const               = 0;
        virtual void handle(const double& v) const                 = 0;
        virtual void handle(const bool& v) const                   = 0;
        virtual void handle(const std::string& v) const            = 0;
        virtual void handle(const object_type& v) const            = 0;
        virtual void handle(const array_type& v) const             = 0;
        virtual void handle(const blob_type& v) const              = 0;
        virtual void handle(const variant_extension_base& v) const = 0;
    };

    /**
     * @brief 访问 variant_base 中的数据
     */
    void visit(const visitor& v) const {
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
            v.handle(*m_string_ptr);
            break;
        case type_id::object_type:
            v.handle(m_object);
            break;
        case type_id::array_type:
            v.handle(*m_array_ptr);
            break;
        case type_id::blob_type:
            v.handle(*m_blob_ptr);
            break;
        case type_id::extension_type:
            if (m_extension) {
                v.handle(*m_extension);
            }
            break;
        default:
            throw_unknow_type_error(m_type);
        }
    }

    /**
     * @brief 使用访问者模式访问 variant_base 中的数据，并返回访问者的结果
     */
    template <typename Visitor>
    auto visit_with(Visitor&& visitor) const {
        switch (m_type) {
        case type_id::null_type:
            return visitor(nullptr);
        case type_id::int8_type:
        case type_id::int16_type:
        case type_id::int32_type:
        case type_id::int64_type:
            return visitor(m_int64);
        case type_id::uint8_type:
        case type_id::uint16_type:
        case type_id::uint32_type:
        case type_id::uint64_type:
            return visitor(m_uint64);
        case type_id::double_type:
            return visitor(m_double);
        case type_id::bool_type:
            return visitor(m_bool);
        case type_id::string_type:
            return visitor(*m_string_ptr);
        case type_id::object_type:
            return visitor(m_object);
        case type_id::array_type:
            return visitor(*m_array_ptr);
        case type_id::blob_type:
            return visitor(*m_blob_ptr);
        case type_id::extension_type: {
            if (m_extension) {
                return visitor(*m_extension);
            }
            throw_unknow_type_error(m_type);
            break;
        }
        default:
            throw_unknow_type_error(m_type);
        }
    }

    /**
     * @brief 获取 variant_base 的类型
     */
    type_id get_type() const {
        return m_type;
    }

    /**
     * @brief 判断 variant_base 是否为空
     */
    bool is_null() const {
        return m_type == type_id::null_type;
    }

    /**
     * @brief 判断 variant_base 是否为字符串
     */
    bool is_string() const {
        return m_type == type_id::string_type;
    }

    /**
     * @brief 判断 variant_base 是否为布尔值
     */
    bool is_bool() const {
        return m_type == type_id::bool_type;
    }

    /**
     * @brief 判断 variant_base 是否为有符号8位整数
     */
    bool is_int8() const {
        return m_type == type_id::int8_type;
    }

    /**
     * @brief 判断 variant_base 是否为无符号8位整数
     */
    bool is_uint8() const {
        return m_type == type_id::uint8_type;
    }

    /**
     * @brief 判断 variant_base 是否为有符号16位整数
     */
    bool is_int16() const {
        return m_type == type_id::int16_type;
    }

    /**
     * @brief 判断 variant_base 是否为无符号16位整数
     */
    bool is_uint16() const {
        return m_type == type_id::uint16_type;
    }

    /**
     * @brief 判断 variant_base 是否为有符号32位整数
     */
    bool is_int32() const {
        return m_type == type_id::int32_type;
    }

    /**
     * @brief 判断 variant_base 是否为无符号32位整数
     */
    bool is_uint32() const {
        return m_type == type_id::uint32_type;
    }

    /**
     * @brief 判断 variant_base 是否为有符号64位整数
     */
    bool is_int64() const {
        return m_type == type_id::int64_type;
    }

    /**
     * @brief 判断 variant_base 是否为无符号64位整数
     */
    bool is_uint64() const {
        return m_type == type_id::uint64_type;
    }

    /**
     * @brief 判断 variant_base 是否为双精度浮点数
     */
    bool is_double() const {
        return m_type == type_id::double_type;
    }

    /**
     * @brief 判断 variant_base 是否为对象
     */
    bool is_object() const {
        return m_type == type_id::object_type;
    }

    /**
     * @brief 判断 variant_base 是否为字典（dict），是 is_object 的别名
     */
    bool is_dict() const {
        return is_object();
    }

    /**
     * @brief 判断 variant_base 是否为数组
     */
    bool is_array() const {
        return m_type == type_id::array_type;
    }

    /**
     * @brief 判断 variant_base 是否为二进制数据
     */
    bool is_blob() const {
        return m_type == type_id::blob_type;
    }

    /**
     * @brief 判断 variant_base 是否为扩展类型
     */
    bool is_extension() const {
        return m_type == type_id::extension_type;
    }

    /**
     * @brief 判断 variant_base 是否为数值类型（整数或浮点数）
     */
    bool is_numeric() const {
        switch (m_type) {
        case type_id::bool_type: // 与 std::is_arithmetic_v 一样，bool 类型也属于数值类型
        case type_id::int8_type:
        case type_id::int16_type:
        case type_id::int32_type:
        case type_id::int64_type:
        case type_id::uint8_type:
        case type_id::uint16_type:
        case type_id::uint32_type:
        case type_id::uint64_type:
        case type_id::double_type:
            return true;
        default:
            return false;
        }
    }

    /**
     * @brief 判断 variant_base 是否为整数类型
     */
    bool is_integer() const {
        switch (m_type) {
        case type_id::bool_type: // 与 std::is_integer_v 一样，bool 类型也属于整数类型
        case type_id::int8_type:
        case type_id::int16_type:
        case type_id::int32_type:
        case type_id::int64_type:
        case type_id::uint8_type:
        case type_id::uint16_type:
        case type_id::uint32_type:
        case type_id::uint64_type:
            return true;
        default:
            return false;
        }
    }

    /**
     * @brief 判断是否为有符号整数类型
     */
    bool is_signed_integer() const {
        return m_type == type_id::int8_type || m_type == type_id::int16_type ||
               m_type == type_id::int32_type || m_type == type_id::int64_type;
    }

    /**
     * @brief 判断是否为无符号整数类型
     */
    bool is_unsigned_integer() const {
        return m_type == type_id::uint8_type || m_type == type_id::uint16_type ||
               m_type == type_id::uint32_type || m_type == type_id::uint64_type;
    }

    /**
     * @brief 将 variant_base 转换为有符号8位整数
     */
    int8_t as_int8() const {
        return static_cast<int8_t>(as_int64());
    }

    /**
     * @brief 将 variant_base 转换为无符号8位整数
     */
    uint8_t as_uint8() const {
        return static_cast<uint8_t>(as_uint64());
    }

    /**
     * @brief 将 variant_base 转换为有符号16位整数
     */
    int16_t as_int16() const {
        return static_cast<int16_t>(as_int64());
    }

    /**
     * @brief 将 variant_base 转换为无符号16位整数
     */
    uint16_t as_uint16() const {
        return static_cast<uint16_t>(as_uint64());
    }

    /**
     * @brief 将 variant_base 转换为有符号32位整数
     */
    int32_t as_int32() const {
        return static_cast<int32_t>(as_int64());
    }

    /**
     * @brief 将 variant_base 转换为无符号32位整数
     */
    uint32_t as_uint32() const {
        return static_cast<uint32_t>(as_uint64());
    }

    /**
     * @brief 将 variant_base 转换为有符号64位整数
     */
    int64_t as_int64() const {
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
            return static_cast<int64_t>(m_uint64);
        case type_id::double_type:
            return static_cast<int64_t>(m_double);
        case type_id::bool_type:
            return m_bool ? 1 : 0;
        case type_id::string_type:
            return mc::string::to_number<int64_t>(*m_string_ptr);
        case type_id::blob_type:
            return mc::string::to_number<int64_t>(m_blob_ptr->as_string_view());
        case type_id::extension_type:
            return m_extension ? m_extension->as_int64() : 0;
        default:
            throw_type_error("int64", m_type);
        }
    }

    /**
     * @brief 将 variant_base 转换为无符号64位整数
     */
    uint64_t as_uint64() const {
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
            return static_cast<uint64_t>(m_int64);
        case type_id::double_type:
            return static_cast<uint64_t>(m_double);
        case type_id::bool_type:
            return m_bool ? 1 : 0;
        case type_id::string_type:
            return mc::string::to_number<uint64_t>(*m_string_ptr);
        case type_id::blob_type:
            return mc::string::to_number<uint64_t>(m_blob_ptr->as_string_view());
        case type_id::extension_type:
            return m_extension ? m_extension->as_uint64() : 0;
        default:
            throw_type_error("uint64", m_type);
            return 0;
        }
    }

    /**
     * @brief 将 variant_base 转换为布尔值
     */
    bool as_bool(bool strict = false) const {
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
            return mc::string::to_bool_with_default(*m_string_ptr, !strict);
        }
        case type_id::blob_type: {
            return mc::string::to_bool_with_default(m_blob_ptr->as_string_view(), !strict);
        }
        case type_id::extension_type:
            return m_extension ? m_extension->as_bool() : false;
        case type_id::null_type:
            return false;
        case type_id::array_type:
            return !m_array_ptr->empty();
        case type_id::object_type:
            return !m_object.empty();
        default:
            return false;
        }
    }

    /**
     * @brief 将 variant_base 转换为双精度浮点数
     */
    double as_double() const {
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
            return mc::string::to_number<double>(*m_string_ptr);
        }
        case type_id::blob_type: {
            return mc::string::to_number<double>(m_blob_ptr->as_string_view());
        }
        case type_id::extension_type:
            return m_extension ? m_extension->as_double() : 0.0;
        default:
            throw_type_error("double", m_type);
            return 0.0;
        }
    }

    /**
     * @brief 将 variant_base 转换为二进制数据
     */
    blob_base<> as_blob() const {
        switch (m_type) {
        case type_id::blob_type:
            return *m_blob_ptr;
        case type_id::string_type: {
            blob_base<> b;
            b.data.assign(m_string_ptr->begin(), m_string_ptr->end());
            return b;
        }
        case type_id::extension_type: {
            if (!m_extension) {
                return {};
            }

            blob_base<> b;
            auto        s = m_extension->as_string();
            b.data.assign(s.begin(), s.end());
            return b;
        }
        default:
            throw_type_error("blob_base", m_type);
        }
    }

    /**
     * @brief 获取扩展类型对象
     */
    extension_ptr_type as_extension() const {
        if (m_type != type_id::extension_type) {
            throw_type_error("extension", m_type);
        }
        return m_extension;
    }

    /**
     * @brief 将 variant_base 转换为字符串
     */
    std::string as_string() const {
        switch (m_type) {
        case type_id::string_type:
            return *m_string_ptr;
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
        case type_id::double_type: {
            return mc::to_string(m_double);
        }
        case type_id::bool_type:
            return m_bool ? "true" : "false";
        case type_id::null_type:
            return "null";
        case type_id::blob_type: {
            return std::string(m_blob_ptr->as_string_view());
        }
        case type_id::extension_type:
            return m_extension ? m_extension->as_string() : std::string();
        default:
            return to_string();
        }
    }

    /**
     * @brief 将 variant_base 转换为 dict
     */
    dict         as_dict() const;
    mutable_dict as_mutable_dict() const;
    dict         as_object() const;

    array_type as_array() const {
        if (m_type != type_id::array_type) {
            throw_type_error("array", m_type);
        }

        return *m_array_ptr;
    }

    /**
     * @brief 获取数组中指定位置的元素
     * @note 支持链式修改
     */
    variant_base& operator[](std::size_t pos) {
        if (m_type != type_id::array_type) {
            throw_type_error("array", m_type);
        }

        auto& arr = *m_array_ptr;
        if (pos >= arr.size()) {
            throw std::out_of_range("数组索引越界: 索引 " + std::to_string(pos) + " 超出范围 [0, " +
                                    std::to_string(arr.size() - 1) + "]");
        }

        return arr[pos];
    }

    /**
     * @brief 获取数组中指定位置的元素
     */
    const variant_base& operator[](std::size_t pos) const {
        if (m_type != type_id::array_type) {
            throw_type_error("array", m_type);
        }

        const auto& arr = *m_array_ptr;
        if (pos >= arr.size()) {
            throw std::out_of_range("数组索引越界: 索引 " + std::to_string(pos) + " 超出范围 [0, " +
                                    std::to_string(arr.size() - 1) + "]");
        }

        return arr[pos];
    }

    /**
     * @brief 获取对象中指定键的值（当variant包含dict对象时）
     * @param key 要查找的键
     * @return 指定键对应的值的可修改引用
     * @throw std::runtime_error 如果variant不是对象类型
     * @note 此方法通过强制类型转换成为 mc::mutable_dict，支持链式修改
     */
    variant_base& operator[](std::string_view key);

    /**
     * @brief 获取对象中指定键的值（当variant包含dict对象时）- 只读版本
     * @param key 要查找的键
     * @return 指定键对应的值的引用
     * @throw std::runtime_error 如果variant不是对象类型
     * @throw std::out_of_range 如果键不存在
     */
    const variant_base& operator[](std::string_view key) const;

    /**
     * @brief 获取对象中指定键的值，如果不存在或不是对象类型则返回默认值
     * @param key 要查找的键
     * @param default_value 默认值
     * @return 指定键对应的值的引用，或者默认值的引用
     */
    const variant_base& get(const std::string& key, const variant_base& default_value) const {
        return get(std::string_view(key), default_value);
    }

    /**
     * @brief 获取对象中指定键的值，如果不存在或不是对象类型则返回默认值
     * @param key 要查找的键
     * @param default_value 默认值
     * @return 指定键对应的值的引用，或者默认值的引用
     */
    const variant_base& get(std::string_view key, const variant_base& default_value) const;

    /**
     * @brief 获取对象中指定键的值，如果不存在或不是对象类型则返回默认值
     * @param key 要查找的键
     * @param default_value 默认值
     * @return 指定键对应的值的引用，或者默认值的引用
     */
    const variant_base& get(const char* key, const variant_base& default_value) const {
        return get(std::string_view(key), default_value);
    }

    /**
     * @brief 检查对象是否包含指定键（当variant包含dict对象时）
     * @param key 要查找的键
     * @return 如果包含指定键则返回 true，否则返回 false
     */
    bool contains(std::string_view key) const;

    /**
     * @brief 检查对象是否包含指定键（当variant包含dict对象时）
     * @param key 要查找的键
     * @return 如果包含指定键则返回 true，否则返回 false
     */
    bool contains(const std::string& key) const {
        return contains(std::string_view(key));
    }

    /**
     * @brief 检查对象是否包含指定键（当variant包含dict对象时）
     * @param key 要查找的键
     * @return 如果包含指定键则返回 true，否则返回 false
     */
    bool contains(const char* key) const {
        return contains(std::string_view(key));
    }

    /**
     * @brief 获取大小
     */
    size_t size() const;

    /**
     * @brief 将 variant_base 转换为指定类型
     */
    template <typename T>
    T as() const {
        using t_type = mc::traits::remove_cvref_t<T>;

        t_type v;
        from_variant(*this, v);
        return v;
    }

    template <typename T>
    std::optional<T> try_as() const {
        try {
            return as<T>();
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    /**
     * @brief 将 variant_base 转换为指定类型，如果转换失败则返回默认值
     * @param default_value 转换失败时返回的默认值
     * @return 转换后的值或默认值
     */
    template <typename T>
    T as(const T& default_value) const {
        if (auto v = try_as<T>()) {
            return *v;
        }
        return default_value;
    }

    /**
     * @brief 将 variant_base 转换为指定类型并存储到引用中
     */
    template <typename T>
    void as(T& v) const {
        from_variant(*this, v);
    }

    /**
     * @brief 赋值运算符
     */
    variant_base& operator=(const variant_base& other) {
        if (this == &other) {
            return *this;
        }

        if constexpr (!is_fixed_type) {
            variant_base(other).swap(*this);
        } else {
            if (other.m_type == m_type || m_type == type_id::null_type) {
                variant_base(other).swap(*this);
            } else {
                set_value(other);
            }
        }
        return *this;
    }
    variant_base& operator=(variant_base&& other) {
        if (this == &other) {
            return *this;
        }

        if constexpr (!is_fixed_type) {
            variant_base(std::move(other)).swap(*this);
        } else {
            if (other.m_type == m_type || m_type == type_id::null_type) {
                variant_base(std::move(other)).swap(*this);
            } else {
                // 类型不一样那就一定存在转换，相比move会有一定开销
                set_value(other);
                other.clear();
            }
        }
        return *this;
    }
    // 字符串赋值优化
    variant_base& operator=(const char* s) {
        if (s) {
            return operator=(std::string_view(s));
        }

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
            m_double = 0;
            break;
        case type_id::bool_type:
            m_bool = 0;
            break;
        case type_id::string_type:
        case type_id::array_type:
        case type_id::object_type:
        case type_id::blob_type:
        case type_id::extension_type:
        default: {
            if constexpr (!is_fixed_type) {
                variant_base().swap(*this);
            } else if (m_type == type_id::string_type) {
                // 对于固定类型来说，给字符串赋值 0(nullptr) 会清空字符串
                m_string_ptr->clear();
            } else {
                throw_type_error(get_type_name(), type_id::string_type);
            }
            break;
        }
        }
        return *this;
    }
    template <typename Allocator>
    variant_base& operator=(const std::basic_string<char, std::char_traits<char>, Allocator>& s) {
        return operator=(std::string_view(s));
    }
    variant_base& operator=(std::string_view s) {
        if (m_type == type_id::string_type) {
            m_string_ptr->assign(s.begin(), s.end());
        } else if (m_type == type_id::blob_type) {
            // 字符串可以赋值给二进制类型
            m_blob_ptr->data.assign(s.begin(), s.end());
        } else {
            *this = variant_base(s);
        }
        return *this;
    }
    // 二进制赋值优化
    template <typename Allocator>
    variant_base& operator=(const blob_base<Allocator>& b) {
        if (m_type == type_id::blob_type) {
            m_blob_ptr->data.assign(b.data.begin(), b.data.end());
        } else {
            *this = variant_base(b);
        }
        return *this;
    }

    template <typename OtherConfig>
    void set_value(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    void set_value(variant_base<OtherConfig>&& other);

    /**
     * @brief 清空 variant_base
     */
    void clear();

    void swap(variant_base& other) noexcept {
        std::swap(m_type, other.m_type);
        std::swap(m_uint64, other.m_uint64);
    }

    explicit operator bool() const {
        return as_bool();
    }

    /**
     * @brief 获取字符串类型
     * @return 字符串引用
     * @throw std::bad_cast 如果类型不匹配
     */
    const std::string& get_string() const {
        if (m_type != type_id::string_type) {
            throw_type_error("string", m_type);
        }
        return *m_string_ptr;
    }

    /**
     * @brief 获取blob类型
     * @return blob引用
     * @throw std::bad_cast 如果类型不匹配
     */
    const blob_type& get_blob() const {
        if (m_type != type_id::blob_type) {
            throw_type_error("blob", m_type);
        }
        return *m_blob_ptr;
    }

    /**
     * @brief 获取数组类型
     * @return 数组引用
     * @throw std::bad_cast 如果类型不匹配
     */
    const array_type& get_array() const {
        if (m_type != type_id::array_type) {
            throw_type_error("array", m_type);
        }
        return *m_array_ptr;
    }

    /**
     * @brief 获取对象类型
     * @return 对象引用
     * @throw std::bad_cast 如果类型不匹配
     */
    const dict& get_object() const {
        if (m_type != type_id::object_type) {
            throw_type_error("object", m_type);
        }
        return m_object;
    }

    /**
     * @brief 获取类型名称
     * @param type 类型ID
     * @return 类型名称
     */
    static const char* get_type_name(type_id type) {
        return get_type_name_internal(type);
    }

    const char* get_type_name() const {
        if (m_type == type_id::extension_type && m_extension) {
            return m_extension->get_type_name().data();
        }
        return get_type_name_internal(m_type);
    }

    /**
     * @brief 获取variant的哈希值，用于支持在容器中使用
     * @return 哈希值
     */
    size_t hash() const {
        switch (m_type) {
        case type_id::null_type:
            return 0;
        case type_id::bool_type:
            return std::hash<bool>{}(m_bool);
        case type_id::int8_type:
        case type_id::int16_type:
        case type_id::int32_type:
        case type_id::int64_type:
            return std::hash<int64_t>{}(m_int64);
        case type_id::uint8_type:
        case type_id::uint16_type:
        case type_id::uint32_type:
        case type_id::uint64_type:
            return std::hash<uint64_t>{}(m_uint64);
        case type_id::double_type:
            return std::hash<double>{}(m_double);
        case type_id::string_type: {
            const auto& str_data = *m_string_ptr;
            return calculate_str_hash(str_data);
        }
        case type_id::blob_type: {
            const auto& blob_data = *m_blob_ptr;
            return calculate_str_hash(blob_data.as_string_view());
        }
        case type_id::array_type: {
            return calculate_array_hash(*m_array_ptr);
        }
        case type_id::object_type: {
            // 使用dict的hash方法计算哈希值
            return m_object.hash();
        }
        case type_id::extension_type:
            return m_extension ? m_extension->hash() : 0;
        default:
            return 0;
        }
    }

    // ======== 算术运算操作符 ========
    template <typename OtherConfig>
    variant_base operator+(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_base operator-(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_base operator*(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_base operator/(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_base operator%(const variant_base<OtherConfig>& other) const;

    // ======== 位运算操作符 ========
    template <typename OtherConfig>
    variant_base operator&(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_base operator|(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_base operator^(const variant_base<OtherConfig>& other) const;

    variant_base operator~() const;

    template <typename OtherConfig>
    variant_base operator<<(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_base operator>>(const variant_base<OtherConfig>& other) const;

    // ======== 算术运算与基本类型 ========
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator+(T other) const;

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator-(T other) const;

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator*(T other) const;

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator/(T other) const;

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator%(T other) const;

    // ======== 位运算与基本类型 ========
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator&(T other) const;

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator|(T other) const;

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator^(T other) const;

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator<<(T other) const;

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator>>(T other) const;

    // ======== 字符串操作 ========
    variant_base operator+(std::string_view other) const;

    // ======== 复合赋值操作符 ========
    template <typename OtherConfig>
    variant_base& operator+=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_base& operator-=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_base& operator*=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_base& operator/=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_base& operator%=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_base& operator&=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_base& operator|=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_base& operator^=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_base& operator<<=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_base& operator>>=(const variant_base<OtherConfig>& other);

    // ======== 复合赋值操作与基本类型 ========
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator+=(const T& other);

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator-=(const T& other);

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator*=(const T& other);

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator/=(const T& other);

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator%=(const T& other);

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator&=(const T& other);

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator|=(const T& other);

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator^=(const T& other);

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator<<=(const T& other);

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator>>=(const T& other);

    // 字符串的复合赋值
    variant_base& operator+=(std::string_view other);

    variant_base& operator++();
    variant_base& operator--();
    variant_base  operator++(int);
    variant_base  operator--(int);

    // 一元操作符
    variant_base operator-() const;
    variant_base operator!() const;

    /**
     * @brief 比较操作符
     */
    template <typename OtherConfig>
    bool operator==(const variant_base<OtherConfig>& other) const;
    template <typename OtherConfig>
    bool operator!=(const variant_base<OtherConfig>& other) const {
        return !(*this == other);
    }
    template <typename OtherConfig>
    bool operator<(const variant_base<OtherConfig>& other) const;
    template <typename OtherConfig>
    bool operator>(const variant_base<OtherConfig>& other) const {
        return other < *this;
    }
    template <typename OtherConfig>
    bool operator<=(const variant_base<OtherConfig>& other) const {
        if (is_double()) {
            if (std::isnan(m_double)) {
                return false;
            }
        } else if (other.is_double()) {
            if (std::isnan(other.as_double())) {
                return false;
            }
        }
        return !(*this > other);
    }
    template <typename OtherConfig>
    bool operator>=(const variant_base<OtherConfig>& other) const {
        if (is_double()) {
            if (std::isnan(m_double)) {
                return false;
            }
        } else if (other.is_double()) {
            if (std::isnan(other.as_double())) {
                return false;
            }
        }
        return !(*this < other);
    }

    /*
     * @brief 添加用于与算术类型的比较
     */
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    bool operator==(const T& other) const;
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    bool operator!=(const T& other) const {
        return !(*this == other);
    }
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    bool operator<(const T& other) const;
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    bool operator>(const T& other) const;
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    bool operator<=(const T& other) const {
        if constexpr (std::is_floating_point_v<T>) {
            if (std::isnan(other)) {
                return false;
            }
        }

        if (is_double()) {
            if (std::isnan(m_double)) {
                return false;
            }
        }

        return !(*this > other);
    }
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    bool operator>=(const T& other) const {
        if constexpr (std::is_floating_point_v<T>) {
            if (std::isnan(other)) {
                return false;
            }
        }

        if (is_double()) {
            if (std::isnan(m_double)) {
                return false;
            }
        }

        return !(*this < other);
    }
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    friend bool operator==(const T& lhs, const variant_base& rhs) {
        return rhs == lhs;
    }
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    friend bool operator!=(const T& lhs, const variant_base& rhs) {
        return !(rhs == lhs);
    }
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    friend bool operator<(const T& lhs, const variant_base& rhs) {
        return rhs > lhs;
    }
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    friend bool operator>(const T& lhs, const variant_base& rhs) {
        return rhs < lhs;
    }
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    friend bool operator<=(const T& lhs, const variant_base& rhs) {
        return rhs >= lhs;
    }
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    friend bool operator>=(const T& lhs, const variant_base& rhs) {
        return rhs <= lhs;
    }

    /*
     * @brief 添加用于与 std::string_view 的比较
     */
    bool operator==(std::string_view other) const;
    bool operator!=(std::string_view other) const {
        return !(*this == other);
    }
    bool operator<(std::string_view other) const;
    bool operator>(std::string_view other) const;
    bool operator<=(std::string_view other) const {
        return !(*this > other);
    }
    bool operator>=(std::string_view other) const {
        return !(*this < other);
    }
    friend bool operator==(std::string_view val, const variant_base& var) {
        return var.operator==(val);
    }
    friend bool operator!=(std::string_view val, const variant_base& var) {
        return !(var.operator==(val));
    }
    friend bool operator<(std::string_view val, const variant_base& var) {
        return var.operator>(val);
    }
    friend bool operator>(std::string_view val, const variant_base& var) {
        return var.operator<(val);
    }
    friend bool operator<=(std::string_view val, const variant_base& var) {
        return var.operator>=(val);
    }
    friend bool operator>=(std::string_view val, const variant_base& var) {
        return var.operator<=(val);
    }

    /*
     * @brief 添加用于与 mc::blob_base 的比较
     */
    template <typename OtherAllocator>
    bool operator==(const blob_base<OtherAllocator>& other) const {
        return *this == other.as_string_view();
    }
    template <typename OtherAllocator>
    bool operator!=(const blob_base<OtherAllocator>& other) const {
        return !(*this == other);
    }
    template <typename OtherAllocator>
    bool operator<(const blob_base<OtherAllocator>& other) const {
        return *this < other.as_string_view();
    }
    template <typename OtherAllocator>
    bool operator>(const blob_base<OtherAllocator>& other) const {
        return *this > other.as_string_view();
    }
    template <typename OtherAllocator>
    bool operator<=(const blob_base<OtherAllocator>& other) const {
        return !(*this > other);
    }
    template <typename OtherAllocator>
    bool operator>=(const blob_base<OtherAllocator>& other) const {
        return !(*this < other);
    }
    template <typename OtherAllocator>
    friend bool operator==(const blob_base<OtherAllocator>& val, const variant_base& var) {
        return var.operator==(val);
    }
    template <typename OtherAllocator>
    friend bool operator!=(const blob_base<OtherAllocator>& val, const variant_base& var) {
        return !(var.operator==(val));
    }
    template <typename OtherAllocator>
    friend bool operator<(const blob_base<OtherAllocator>& val, const variant_base& var) {
        return var.operator>(val);
    }
    template <typename OtherAllocator>
    friend bool operator>(const blob_base<OtherAllocator>& val, const variant_base& var) {
        return var.operator<(val);
    }
    template <typename OtherAllocator>
    friend bool operator<=(const blob_base<OtherAllocator>& val, const variant_base& var) {
        return var.operator>=(val);
    }
    template <typename OtherAllocator>
    friend bool operator>=(const blob_base<OtherAllocator>& val, const variant_base& var) {
        return var.operator<=(val);
    }

    /*
     * @brief 添加用于与 mc::variants 的比较
     */
    template <typename OtherConfig>
    bool operator==(const std::vector<variant_base<OtherConfig>>& other) const {
        if (m_type != type_id::array_type) {
            return false;
        }
        return std::equal(m_array_ptr->begin(), m_array_ptr->end(), other.begin(), other.end());
    }
    template <typename OtherConfig>
    bool operator!=(const std::vector<variant_base<OtherConfig>>& other) const {
        return !(*this == other);
    }
    template <typename OtherConfig>
    bool operator<(const std::vector<variant_base<OtherConfig>>& other) const {
        if (m_type != type_id::array_type) {
            throw_type_error("array", m_type);
        }
        return std::lexicographical_compare(m_array_ptr->begin(), m_array_ptr->end(), other.begin(),
                                            other.end());
    }
    template <typename OtherConfig>
    bool operator>(const std::vector<variant_base<OtherConfig>>& other) const {
        if (m_type != type_id::array_type) {
            throw_type_error("array", m_type);
        }
        return std::lexicographical_compare(m_array_ptr->begin(), m_array_ptr->end(), other.begin(),
                                            other.end(), std::greater<variant_base<OtherConfig>>());
    }
    template <typename OtherConfig>
    bool operator<=(const std::vector<variant_base<OtherConfig>>& other) const {
        return !(*this > other);
    }
    template <typename OtherConfig>
    bool operator>=(const std::vector<variant_base<OtherConfig>>& other) const {
        return !(*this < other);
    }
    template <typename OtherConfig>
    friend bool operator==(const std::vector<variant_base<OtherConfig>>& val,
                           const variant_base&                           var) {
        return var.operator==(val);
    }
    template <typename OtherConfig>
    friend bool operator!=(const std::vector<variant_base<OtherConfig>>& val,
                           const variant_base&                           var) {
        return !(var.operator==(val));
    }
    template <typename OtherConfig>
    friend bool operator<(const std::vector<variant_base<OtherConfig>>& val,
                          const variant_base&                           var) {
        return var.operator>(val);
    }
    template <typename OtherConfig>
    friend bool operator>(const std::vector<variant_base<OtherConfig>>& val,
                          const variant_base&                           var) {
        return var.operator<(val);
    }
    template <typename OtherConfig>
    friend bool operator<=(const std::vector<variant_base<OtherConfig>>& val,
                           const variant_base&                           var) {
        return var.operator>=(val);
    }
    template <typename OtherConfig>
    friend bool operator>=(const std::vector<variant_base<OtherConfig>>& val,
                           const variant_base&                           var) {
        return var.operator<=(val);
    }

    /*
     * @brief 添加用于与 mc::dict 的比较，dict 不支持大小比较，只支持相等比较
     */
    bool operator==(const dict& other) const;
    bool operator!=(const dict& other) const {
        return !(*this == other);
    }
    friend bool operator==(const dict& val, const variant_base& var) {
        return var.operator==(val);
    }
    friend bool operator!=(const dict& val, const variant_base& var) {
        return !(var.operator==(val));
    }

    /**
     * @brief 将 variant 转换为字符串表示
     *
     * @return std::string variant 的字符串表示
     */
    std::string to_string() const {
        return json::json_encode(*this);
    }

private:
    template <typename OtherConfig>
    bool same_type_equal(const variant_base<OtherConfig>& other) const;
    template <typename OtherConfig>
    bool other_type_equal(const variant_base<OtherConfig>& other) const;
    template <typename OtherConfig>
    bool same_type_less(const variant_base<OtherConfig>& other) const;
    template <typename OtherConfig>
    bool other_type_less(const variant_base<OtherConfig>& other) const;

protected:
    union {
        double             m_double;
        int64_t            m_int64;
        uint64_t           m_uint64;
        bool               m_bool;
        string_ptr_type    m_string_ptr;
        blob_ptr_type      m_blob_ptr;
        array_ptr_type     m_array_ptr;
        object_type        m_object;
        extension_ptr_type m_extension;
    };
    type_id        m_type;
    allocator_type m_alloc = allocator_type();
};

template <typename Config>
inline size_t calculate_array_hash(const variants_base<Config>& array_data) {
    if (array_data.empty()) {
        return 0;
    }

    // 使用黄金比例作为种子
    size_t       h    = 0x9e3779b9 ^ array_data.size();
    const size_t step = (array_data.size() >> 5) + 1;
    for (size_t l1 = array_data.size(); l1 >= step; l1 -= step) {
        h = h ^ ((h << 5) + (h >> 2) + array_data[l1 - 1].hash());
    }

    return h;
};

// 浮点数
template <typename Config, typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
void to_variant(const T& var, variant_base<Config>& vo) {
    variant_base<Config>(static_cast<double>(var)).swap(vo);
}
template <typename Config, typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
void from_variant(const variant_base<Config>& var, T& vo) {
    vo = static_cast<T>(var.as_double());
}

// 整数
template <typename Config, typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
void to_variant(const T& var, variant_base<Config>& vo) {
    variant_base<Config>(detail::fixed_integer_type<T>(), var).swap(vo);
}
template <typename Config, typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
void from_variant(const variant_base<Config>& var, T& vo) {
    if constexpr (std::is_signed_v<T>) {
        vo = static_cast<T>(var.as_int64());
    } else if constexpr (std::is_unsigned_v<T>) {
        vo = static_cast<T>(var.as_uint64());
    }
}

// bool
template <typename Config>
void to_variant(bool var, variant_base<Config>& vo) {
    variant_base<Config>(var).swap(vo);
}
template <typename Config>
void from_variant(const variant_base<Config>& var, bool& vo) {
    vo = var.as_bool();
}

// string
template <typename Config, typename Traits, typename Alloc>
void to_variant(const std::basic_string<char, Traits, Alloc>& var, variant_base<Config>& vo) {
    variant_base<Config>(var).swap(vo);
}
template <typename Config>
void to_variant(std::string_view var, variant_base<Config>& vo) {
    variant_base<Config>(var).swap(vo);
}
template <typename Config, typename Traits, typename Alloc>
void from_variant(const variant_base<Config>& var, std::basic_string<char, Traits, Alloc>& vo) {
    vo = var.as_string();
}

// blob_base
template <typename Config, typename Alloc>
void to_variant(const blob_base<Alloc>& var, variant_base<Config>& vo) {
    variant_base<Config>(var).swap(vo);
}
template <typename Config, typename Alloc>
void from_variant(const variant_base<Config>& var, blob_base<Alloc>& vo) {
    vo = var.as_blob();
}

// array_type
template <typename Config1, typename Config2>
void to_variant(const variants_base<Config1>& var, variant_base<Config2>& vo) {
    variant_base<Config2>(var).swap(vo);
}
template <typename Config1, typename Config2>
void from_variant(const variant_base<Config1>& var, variants_base<Config2>& vo) {
    vo = var.get_array();
}

// 从其他 variant_base 转换为 variant_base
template <typename Config1, typename Config2>
void to_variant(const variant_base<Config1>& var, variant_base<Config2>& vo) {
    variant_base<Config2>(var).swap(vo);
}
template <typename Config1, typename Config2>
void from_variant(const variant_base<Config1>& var, variant_base<Config2>& vo) {
    variant_base<Config2>(var).swap(vo);
}

// 为继承自 variant_extension 的类型提供 to_variant 转换
template <typename Config, typename T,
          std::enable_if_t<std::is_base_of_v<variant_extension_base, T>, int> = 0>
void to_variant(const T& var, variant_base<Config>& vo) {
    variant_base<Config>(var.clone()).swap(vo);
}
template <typename Config, typename T,
          std::enable_if_t<std::is_base_of_v<variant_extension_base, T>, int> = 0>
void from_variant(const variant_base<Config>& var, T& vo) {
    if (var.is_extension()) {
        auto ext_ptr = var.as_extension();
        if (!ext_ptr) {
            return;
        }

        auto ext = mc::dynamic_pointer_cast<T>(ext_ptr);
        if (ext) {
            vo = *ext;
            return;
        }
    }
    throw_type_error("extension", var.get_type());
}

} // namespace mc

#include <mc/variant/variant_base_cmp_op.inl>
#include <mc/variant/variant_base_op.inl>

namespace std {
template <typename Config>
struct hash<mc::variant_base<Config>> {
    size_t operator()(const mc::variant_base<Config>& v) const {
        return v.hash();
    }
};
} // namespace std
#endif // MC_VARIANT_VARIANT_BASE_H