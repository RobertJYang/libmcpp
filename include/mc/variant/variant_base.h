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

#include <mc/array.h>
#include <mc/common.h>
#include <mc/dict/dict.h>
#include <mc/json.h>
#include <mc/memory/allocator.h>
#include <mc/pretty_name.h>
#include <mc/string.h>
#include <mc/traits.h>
#include <mc/variant/copy_context.h>
#include <mc/variant/variant_common.h>
#include <mc/variant/variant_extension.h>

namespace mc {
namespace detail {

template <typename F, typename Arg>
static auto invoke_visitor(F&& func, Arg&& v) {
    if constexpr (std::is_invocable_v<F>) {
        MC_UNUSED(v);
        return func();
    } else if constexpr (std::is_invocable_v<F, Arg>) {
        return func(std::forward<Arg>(v));
    } else {
        using function_traits = mc::traits::function_traits<F>;
        using return_type     = mc::traits::remove_cvref_t<typename function_traits::return_type>;
        if constexpr (!std::is_same_v<return_type, void>) {
            return return_type{};
        }
    }
}
} // namespace detail

/**
 * @brief variant_base 类，用于表示任意类型的数据
 */
class MC_API variant_base {
public:
    using Config     = variant_config<>;
    using is_variant = std::true_type;

    using allocator_type     = typename Config::allocator_type;
    using string_type        = typename Config::string_type;
    using object_type        = typename Config::object_type;
    using array_type         = typename Config::array_type;
    using blob_type          = typename Config::blob_type;
    using extension_type     = typename Config::extension_type;
    using string_ptr_type    = typename Config::string_ptr_type;
    using array_ptr_type     = typename Config::array_ptr_type;
    using blob_ptr_type      = typename Config::blob_ptr_type;
    using extension_ptr_type = typename Config::extension_ptr_type;

    variant_base()
        : m_uint64(0), m_type(type_id::null_type), m_is_fixed(false) {
        static_assert(sizeof(uint64_t) >= sizeof(void*) && sizeof(uint64_t) >= sizeof(double),
                      "uint64_t 不是联合体中最大的成员");
    }
    variant_base(std::nullptr_t)
        : variant_base() {
    }
    variant_base(type_id type);

    template <typename T, std::enable_if_t<std::is_fundamental_v<T>, int> = 0>
    variant_base(type_id type, T val) {
        if constexpr (std::is_same_v<T, bool>) {
            m_bool = static_cast<bool>(val);
        } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
            m_int64 = static_cast<int64_t>(val);
        } else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
            m_uint64 = static_cast<uint64_t>(val);
        } else if constexpr (std::is_floating_point_v<T>) {
            m_double = static_cast<double>(val);
        }
        m_type     = type;
        m_is_fixed = false;
    }

    /**
     * @brief 从字符串构造 variant_base
     */
    variant_base(const char* str)
        : variant_base(std::string_view(str)) {
    }
    variant_base(const std::string& str)
        : variant_base(std::string_view(str)) {
    }
    variant_base(std::string_view str)
        : m_uint64(0), m_type(type_id::string_type), m_is_fixed(false) {
        m_string_ptr = mc::allocate_ptr<string_type>(allocator_type(), str.data(), str.size());
    }

    /*
     * 从各种基础类型构造 variant_base
     */
    variant_base(bool val)
        : variant_base(type_id::bool_type, val) {
    }
    template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    variant_base(T val)
        : variant_base(mc::detail::fixed_integer_type<T>(), val) {
    }
    template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    variant_base(T val)
        : variant_base(type_id::double_type, val) {
    }
    variant_base(const blob_type& val)
        : variant_base(type_id::blob_type) {
        m_blob_ptr =
            mc::allocate_ptr<blob_type>(allocator_type(), val.data.data(), val.data.size());
        m_is_fixed = false;
    }

    // 从字典构造 variant_base
    variant_base(const dict& obj)
        : variant_base(type_id::object_type) {
        new (&m_object) object_type(obj);
        m_is_fixed = false;
    }

    // 从 array_type 构造 variant_base
    variant_base(const array_type& arr)
        : m_type(type_id::array_type), m_is_fixed(false) {
        new (&m_array) array_type(arr);
    }

    template <typename T, std::enable_if_t<std::is_base_of_v<variant_extension_base, T>, int> = 0>
    variant_base(mc::shared_ptr<T> ext)
        : m_type(type_id::extension_type), m_is_fixed(false) {
        new (&m_extension) extension_ptr_type(mc::static_pointer_cast<variant_extension_base>(ext));
    }

    template <typename T, std::enable_if_t<
                              !is_variant_v<T> &&
                                  !std::is_pointer_v<T> &&
                                  !mc::is_variant_fundamental_v<T> &&
                                  !std::is_same_v<std::decay_t<T>, object_type> &&
                                  !std::is_same_v<std::decay_t<T>, blob_type> &&
                                  !std::is_same_v<std::decay_t<T>, array_type> &&
                                  mc::is_variant_constructible_v<T>,
                              int> = 0>
    variant_base(const T& obj)
        : variant_base() {
        to_variant(obj, *this);
    }

    /**
     * @brief 拷贝构造函数
     */
    variant_base(const variant_base& other);

    /**
     * @brief 移动构造函数
     */
    variant_base(variant_base&& other) noexcept;

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
    void visit(const visitor& v) const;

    /**
     * @brief 使用访问者模式访问 variant_base 中的数据，并返回访问者的结果
     *
     * 支持精确类型分发：visitor 只需要能够处理当前类型，类型不匹配时静默跳过
     *
     * @note 如果 visitor 不能处理当前类型，函数会静默返回（不抛出异常）
     */
    template <typename Visitor>
    auto visit_with(Visitor&& visitor) const {
        switch (m_type) {
        case type_id::null_type:
            return detail::invoke_visitor<Visitor>(std::forward<Visitor>(visitor), nullptr);
        case type_id::int8_type:
        case type_id::int16_type:
        case type_id::int32_type:
        case type_id::int64_type:
            return detail::invoke_visitor<Visitor>(std::forward<Visitor>(visitor), m_int64);
        case type_id::uint8_type:
        case type_id::uint16_type:
        case type_id::uint32_type:
        case type_id::uint64_type:
            return detail::invoke_visitor<Visitor>(std::forward<Visitor>(visitor), m_uint64);
        case type_id::double_type:
            return detail::invoke_visitor<Visitor>(std::forward<Visitor>(visitor), m_double);
        case type_id::bool_type:
            return detail::invoke_visitor<Visitor>(std::forward<Visitor>(visitor), m_bool);
        case type_id::string_type:
            return detail::invoke_visitor<Visitor>(std::forward<Visitor>(visitor), *m_string_ptr);
        case type_id::object_type:
            return detail::invoke_visitor<Visitor>(std::forward<Visitor>(visitor), m_object);
        case type_id::array_type:
            return detail::invoke_visitor<Visitor>(std::forward<Visitor>(visitor), m_array);
        case type_id::blob_type:
            return detail::invoke_visitor<Visitor>(std::forward<Visitor>(visitor), *m_blob_ptr);
        case type_id::extension_type:
            if (m_extension) {
                return detail::invoke_visitor<Visitor>(std::forward<Visitor>(visitor), *m_extension);
            }
        default:
            break;
        }
        throw_unknow_type_error(get_type());
    }

    /**
     * @brief 获取 variant_base 的类型
     */
    type_id get_type() const {
        return static_cast<type_id>(m_type);
    }

    /**
     * @brief 判断 variant_base 是否为固定类型
     */
    bool is_fixed_type() const {
        return m_is_fixed;
    }

    /**
     * @brief 设置 variant_base 为固定类型，锁定当前类型
     * @param fixed 是否设置为固定类型，默认为 true
     */
    void set_fixed_type(bool fixed = true) {
        m_is_fixed = fixed;
    }

    /**
     * @brief 判断 variant_base 是否为空
     */
    bool is_null() const {
        return get_type() == type_id::null_type;
    }

    /**
     * @brief 判断 variant_base 是否为字符串
     */
    bool is_string() const {
        return get_type() == type_id::string_type;
    }

    /**
     * @brief 判断 variant_base 是否为布尔值
     */
    bool is_bool() const {
        return get_type() == type_id::bool_type;
    }

    /**
     * @brief 判断 variant_base 是否为有符号8位整数
     */
    bool is_int8() const {
        return get_type() == type_id::int8_type;
    }

    /**
     * @brief 判断 variant_base 是否为无符号8位整数
     */
    bool is_uint8() const {
        return get_type() == type_id::uint8_type;
    }

    /**
     * @brief 判断 variant_base 是否为有符号16位整数
     */
    bool is_int16() const {
        return get_type() == type_id::int16_type;
    }

    /**
     * @brief 判断 variant_base 是否为无符号16位整数
     */
    bool is_uint16() const {
        return get_type() == type_id::uint16_type;
    }

    /**
     * @brief 判断 variant_base 是否为有符号32位整数
     */
    bool is_int32() const {
        return get_type() == type_id::int32_type;
    }

    /**
     * @brief 判断 variant_base 是否为无符号32位整数
     */
    bool is_uint32() const {
        return get_type() == type_id::uint32_type;
    }

    /**
     * @brief 判断 variant_base 是否为有符号64位整数
     */
    bool is_int64() const {
        return get_type() == type_id::int64_type;
    }

    /**
     * @brief 判断 variant_base 是否为无符号64位整数
     */
    bool is_uint64() const {
        return get_type() == type_id::uint64_type;
    }

    /**
     * @brief 判断 variant_base 是否为双精度浮点数
     */
    bool is_double() const {
        return get_type() == type_id::double_type;
    }

    /**
     * @brief 判断 variant_base 是否为对象
     */
    bool is_object() const {
        return get_type() == type_id::object_type;
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
        return get_type() == type_id::array_type;
    }

    /**
     * @brief 判断 variant_base 是否为二进制数据
     */
    bool is_blob() const {
        return get_type() == type_id::blob_type;
    }

    /**
     * @brief 判断 variant_base 是否为扩展类型
     */
    bool is_extension() const {
        return get_type() == type_id::extension_type;
    }

    /**
     * @brief 判断 variant_base 是否为数值类型（整数或浮点数）
     */
    bool is_numeric() const;

    /**
     * @brief 判断 variant_base 是否为整数类型
     */
    bool is_integer() const;

    /**
     * @brief 判断是否为有符号整数类型
     */
    bool is_signed_integer() const;

    /**
     * @brief 判断是否为无符号整数类型
     */
    bool is_unsigned_integer() const;

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
    int64_t as_int64() const;

    /**
     * @brief 将 variant_base 转换为无符号64位整数
     */
    uint64_t as_uint64() const;

    /**
     * @brief 将 variant_base 转换为布尔值
     */
    bool as_bool(bool strict = false) const;

    /**
     * @brief 将 variant_base 转换为双精度浮点数
     */
    double as_double() const;

    /**
     * @brief 将 variant_base 转换为二进制数据
     */
    blob_type as_blob() const;

    /**
     * @brief 获取扩展类型对象
     */
    extension_ptr_type as_extension() const;

    /**
     * @brief 将 variant_base 转换为字符串
     */
    std::string as_string() const;

    /**
     * @brief 将 variant_base 转换为 dict
     */
    dict as_dict() const;

    /**
     * @brief 将 variant_base 转换为 mutable_dict（为了向后兼容，实际返回 dict）
     * @note mutable_dict 现在是 dict 的别名
     */
    dict as_mutable_dict() const {
        return as_dict();
    }

    dict as_object() const;

    /**
     * @brief 将 variant_base 转换为 array
     */
    array_type as_array() const;

    /**
     * @brief 获取数组中指定位置的元素
     * @note 支持链式修改，返回代理对象
     */
    variant_reference operator[](std::size_t pos);

    /**
     * @brief 获取数组中指定位置的元素（只读）
     */
    variant_reference operator[](std::size_t pos) const;

    /**
     * @brief 获取对象中指定键的值（当variant包含dict对象时）
     * @param key 要查找的键
     * @return 返回代理对象
     * @throw mc::invalid_arg_exception 如果variant不是对象类型
     */
    variant_reference operator[](std::string_view key);

    /**
     * @brief 获取对象中指定键的值（当variant包含dict对象时）- 只读版本
     * @param key 要查找的键
     * @return 返回代理对象
     * @throw mc::invalid_arg_exception 如果variant不是对象类型
     * @throw mc::out_of_range_exception 如果键不存在
     */
    variant_reference operator[](std::string_view key) const;

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
        if constexpr (std::is_same_v<t_type, variant_base>) {
            return *this;
        } else {
            t_type v;
            from_variant(*this, v);
            return v;
        }
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
        using t_type = mc::traits::remove_cvref_t<T>;
        if constexpr (std::is_same_v<t_type, variant_base>) {
            v = *this;
        } else {
            from_variant(*this, v);
        }
    }

    /**
     * @brief 赋值运算符
     */
    variant_base& operator=(const variant_base& other);
    variant_base& operator=(variant_base&& other);
    // 字符串赋值优化
    variant_base& operator=(const char* s);
    variant_base& operator=(const std::string& s) {
        return operator=(std::string_view(s));
    }
    variant_base& operator=(std::string_view s);
    // 二进制赋值优化
    variant_base& operator=(const blob& b);

    variant_base& set_value(const variant_base& other);

    variant_base& set_value(variant_base&& other);

    /**
     * @brief 浅拷贝 variant
     * @return 浅拷贝后的 variant
     * @note 对于基础类型、字符串直接拷贝值
     * @note 对于 array、dict 使用浅拷贝（共享内部元素）
     * @note 对于 blob 拷贝数据
     * @note 对于 extension 调用其 copy() 方法
     */
    variant_base copy() const;

    /**
     * @brief 深拷贝 variant
     * @param ctx 可选的深拷贝上下文，用于检测循环引用并记录已拷贝对象
     * @return 深度拷贝后的 variant
     *
     * @note 如果传入 ctx 参数，则使用该上下文进行循环引用检测
     * @note 如果不传入 ctx，则创建局部上下文（用于顶层调用）
     * @note 遇到循环引用时，返回已拷贝的对象，保持引用关系
     */
    variant_base deep_copy(mc::detail::copy_context* ctx = nullptr) const;

    /**
     * @brief 清空 variant_base
     */
    void clear();

    void swap(variant_base& other) noexcept;

    explicit operator bool() const {
        return as_bool();
    }

    /**
     * @brief 获取字符串类型
     * @return 字符串引用
     * @throw mc::invalid_arg_exception 如果类型不匹配
     */
    const std::string& get_string() const;

    /**
     * @brief 获取blob类型
     * @return blob引用
     * @throw mc::invalid_arg_exception 如果类型不匹配
     */
    const blob_type& get_blob() const;

    /**
     * @brief 获取数组类型
     * @return 数组引用
     * @throw mc::invalid_arg_exception 如果类型不匹配
     */
    const array_type& get_array() const;

    /**
     * @brief 获取对象类型
     * @return 对象引用
     * @throw mc::invalid_arg_exception 如果类型不匹配
     */
    const dict& get_object() const;

    /**
     * @brief 获取类型名称
     * @param type 类型ID
     * @return 类型名称
     */
    static const char* get_type_name(type_id type) {
        return get_type_name_internal(type);
    }

    const char* get_type_name() const;

    /**
     * @brief 获取variant的哈希值，用于支持在容器中使用
     * @return 哈希值
     */
    size_t hash() const;

    // ======== 算术运算操作符 ========

    variant_base operator+(const variant_base& other) const;

    variant_base operator-(const variant_base& other) const;

    variant_base operator*(const variant_base& other) const;

    variant_base operator/(const variant_base& other) const;

    variant_base operator%(const variant_base& other) const;

    // ======== 位运算操作符 ========

    variant_base operator&(const variant_base& other) const;

    variant_base operator|(const variant_base& other) const;

    variant_base operator^(const variant_base& other) const;

    variant_base operator~() const;

    variant_base operator<<(const variant_base& other) const;

    variant_base operator>>(const variant_base& other) const;

    // ======== 算术运算与基本类型 ========
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator+(T other) const {
        return operator+(detail::numeric_t{other});
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator-(T other) const {
        return operator-(detail::numeric_t{other});
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator*(T other) const {
        return operator*(detail::numeric_t{other});
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator/(T other) const {
        return operator/(detail::numeric_t{other});
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator%(T other) const {
        return operator%(detail::numeric_t{other});
    }

    // ======== 位运算与基本类型 ========
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator&(T other) const {
        return operator&(detail::numeric_t{other});
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator|(T other) const {
        return operator|(detail::numeric_t{other});
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator^(T other) const {
        return operator^(detail::numeric_t{other});
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator<<(T other) const {
        return operator<<(detail::numeric_t{other});
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base operator>>(T other) const {
        return operator>>(detail::numeric_t{other});
    }

    // ======== 字符串操作 ========
    variant_base operator+(std::string_view other) const;
    variant_base operator+(const char* other) const {
        return operator+(std::string_view(other));
    }
    variant_base operator+(const std::string& other) const {
        return operator+(std::string_view(other));
    }

    // ======== 复合赋值操作符 ========

    variant_base& operator+=(const variant_base& other);

    variant_base& operator-=(const variant_base& other);

    variant_base& operator*=(const variant_base& other);

    variant_base& operator/=(const variant_base& other);

    variant_base& operator%=(const variant_base& other);

    variant_base& operator&=(const variant_base& other);

    variant_base& operator|=(const variant_base& other);

    variant_base& operator^=(const variant_base& other);

    variant_base& operator<<=(const variant_base& other);

    variant_base& operator>>=(const variant_base& other);

    // ======== 复合赋值操作与基本类型 ========
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator+=(const T& other) {
        return set_value(*this + other);
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator-=(const T& other) {
        return set_value(*this - other);
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator*=(const T& other) {
        return set_value(*this * other);
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator/=(const T& other) {
        return set_value(*this / other);
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator%=(const T& other) {
        return set_value(*this % other);
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator&=(const T& other) {
        return set_value(*this & other);
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator|=(const T& other) {
        return set_value(*this | other);
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator^=(const T& other) {
        return set_value(*this ^ other);
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator<<=(const T& other) {
        return set_value(*this << other);
    }

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    variant_base& operator>>=(const T& other) {
        return set_value(*this >> other);
    }

    // 字符串的复合赋值
    variant_base& operator+=(const char* other) {
        return operator+=(std::string_view(other));
    }
    variant_base& operator+=(std::string_view other) {
        return set_value(*this + other);
    }
    variant_base& operator+=(const std::string& other) {
        return operator+=(std::string_view(other));
    }

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

    bool operator==(const variant_base& other) const;

    bool operator!=(const variant_base& other) const {
        return !(*this == other);
    }

    bool operator<(const variant_base& other) const;

    bool operator>(const variant_base& other) const {
        return other < *this;
    }

    bool operator<=(const variant_base& other) const;

    bool operator>=(const variant_base& other) const;

    /*
     * @brief 添加用于与算术类型的比较
     */
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    bool operator==(const T& other) const {
        return operator==(detail::numeric_t{other});
    }
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    bool operator!=(const T& other) const {
        return !(*this == other);
    }

    bool operator<(detail::numeric_t rhs) const;
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    bool operator<(const T& other) const {
        return *this < detail::make_numeric(other);
    }

    bool         operator>(detail::numeric_t rhs) const;
    bool         operator<=(detail::numeric_t rhs) const;
    bool         operator>=(detail::numeric_t rhs) const;
    bool         operator==(detail::numeric_t rhs) const;
    variant_base operator+(detail::numeric_t rhs) const;
    variant_base operator-(detail::numeric_t rhs) const;
    variant_base operator*(detail::numeric_t rhs) const;
    variant_base operator/(detail::numeric_t rhs) const;
    variant_base operator%(detail::numeric_t rhs) const;
    variant_base operator<<(detail::numeric_t rhs) const;
    variant_base operator>>(detail::numeric_t rhs) const;
    variant_base operator&(detail::numeric_t rhs) const;
    variant_base operator|(detail::numeric_t rhs) const;
    variant_base operator^(detail::numeric_t rhs) const;
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    bool operator>(const T& other) const {
        return operator>(detail::numeric_t{other});
    }
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    bool operator<=(const T& other) const {
        return operator<=(detail::numeric_t{other});
    }
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    bool operator>=(const T& other) const {
        return operator>=(detail::numeric_t{other});
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
     * @brief 添加用于与 std::string 的比较
     */
    bool operator==(const std::string& other) const {
        return this->operator==(std::string_view(other));
    }
    bool operator!=(const std::string& other) const {
        return !(*this == other);
    }
    bool operator<(const std::string& other) const {
        return this->operator<(std::string_view(other));
    }
    bool operator>(const std::string& other) const {
        return this->operator>(std::string_view(other));
    }
    bool operator<=(const std::string& other) const {
        return !(*this > other);
    }
    bool operator>=(const std::string& other) const {
        return !(*this < other);
    }

    /*
     * @brief 添加用于与 const char* 的比较
     */
    bool operator==(const char* other) const {
        return this->operator==(std::string_view(other));
    }
    bool operator!=(const char* other) const {
        return !(*this == other);
    }
    bool operator<(const char* other) const {
        return this->operator<(std::string_view(other));
    }
    bool operator>(const char* other) const {
        return this->operator>(std::string_view(other));
    }
    bool operator<=(const char* other) const {
        return !(*this > other);
    }
    bool operator>=(const char* other) const {
        return !(*this < other);
    }
    friend bool operator==(const char* val, const variant_base& var) {
        return var.operator==(val);
    }
    friend bool operator!=(const char* val, const variant_base& var) {
        return !(var.operator==(val));
    }
    friend bool operator<(const char* val, const variant_base& var) {
        return var.operator>(val);
    }
    friend bool operator>(const char* val, const variant_base& var) {
        return var.operator<(val);
    }
    friend bool operator<=(const char* val, const variant_base& var) {
        return var.operator>=(val);
    }
    friend bool operator>=(const char* val, const variant_base& var) {
        return var.operator<=(val);
    }

    friend bool operator==(const std::string& val, const variant_base& var) {
        return var.operator==(val);
    }
    friend bool operator!=(const std::string& val, const variant_base& var) {
        return !(var.operator==(val));
    }
    friend bool operator<(const std::string& val, const variant_base& var) {
        return var.operator>(val);
    }
    friend bool operator>(const std::string& val, const variant_base& var) {
        return var.operator<(val);
    }
    friend bool operator<=(const std::string& val, const variant_base& var) {
        return var.operator>=(val);
    }
    friend bool operator>=(const std::string& val, const variant_base& var) {
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
     * @brief 与 mc::variants 的比较运算符
     */
    bool operator==(const variants& other) const;
    bool operator!=(const variants& other) const {
        return !(*this == other);
    }
    bool operator<(const variants& other) const;
    bool operator>(const variants& other) const;
    bool operator<=(const variants& other) const {
        return !(*this > other);
    }
    bool operator>=(const variants& other) const {
        return !(*this < other);
    }
    friend bool operator==(const variants& val, const variant_base& var) {
        return var.operator==(val);
    }
    friend bool operator!=(const variants& val, const variant_base& var) {
        return !(var.operator==(val));
    }
    friend bool operator<(const variants& val, const variant_base& var) {
        return var.operator>(val);
    }
    friend bool operator>(const variants& val, const variant_base& var) {
        return var.operator<(val);
    }
    friend bool operator<=(const variants& val, const variant_base& var) {
        return var.operator>=(val);
    }
    friend bool operator>=(const variants& val, const variant_base& var) {
        return var.operator<=(val);
    }

    /*
     * @brief 与 std::vector<T> 的比较运算符
     */
    template <typename T, std::enable_if_t<
                              mc::is_variant_constructible_v<T> || mc::is_variant_v<T>, int> = 0>
    bool operator==(const std::vector<T>& other) const {
        if (get_type() != type_id::array_type) {
            return false;
        }
        return std::equal(m_array.begin(), m_array.end(), other.begin(), other.end());
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T> || mc::is_variant_v<T>, int> = 0>
    bool operator!=(const std::vector<T>& other) const {
        return !(*this == other);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T> || mc::is_variant_v<T>, int> = 0>
    bool operator<(const std::vector<T>& other) const {
        if (is_array()) {
            throw_type_error("array", get_type());
        }
        return std::lexicographical_compare(m_array.begin(), m_array.end(),
                                            other.begin(), other.end());
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T> || mc::is_variant_v<T>, int> = 0>
    bool operator>(const std::vector<T>& other) const {
        if (!is_array()) {
            throw_type_error("array", get_type());
        }
        return std::lexicographical_compare(other.begin(), other.end(),
                                            m_array.begin(), m_array.end());
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T> || mc::is_variant_v<T>, int> = 0>
    bool operator<=(const std::vector<T>& other) const {
        return !(*this > other);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T> || mc::is_variant_v<T>, int> = 0>
    bool operator>=(const std::vector<T>& other) const {
        return !(*this < other);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T> || mc::is_variant_v<T>, int> = 0>
    friend bool operator==(const std::vector<T>& val, const variant_base& var) {
        return var.operator==(val);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T> || mc::is_variant_v<T>, int> = 0>
    friend bool operator!=(const std::vector<T>& val, const variant_base& var) {
        return !(var.operator==(val));
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T> || mc::is_variant_v<T>, int> = 0>
    friend bool operator<(const std::vector<T>& val, const variant_base& var) {
        return var.operator>(val);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T> || mc::is_variant_v<T>, int> = 0>
    friend bool operator>(const std::vector<T>& val, const variant_base& var) {
        return var.operator<(val);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T> || mc::is_variant_v<T>, int> = 0>
    friend bool operator<=(const std::vector<T>& val, const variant_base& var) {
        return var.operator>=(val);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T> || mc::is_variant_v<T>, int> = 0>
    friend bool operator>=(const std::vector<T>& val, const variant_base& var) {
        return var.operator<=(val);
    }

    /*
     * @brief 与 mc::array<T> 的比较运算符
     */
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
    bool operator==(const mc::array<T>& other) const {
        if (!is_array()) {
            return false;
        }
        return m_array == other;
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
    bool operator!=(const mc::array<T>& other) const {
        return !(*this == other);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
    bool operator<(const mc::array<T>& other) const {
        if (!is_array()) {
            throw_type_error("array", get_type());
        }
        return m_array < other;
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
    bool operator>(const mc::array<T>& other) const {
        if (!is_array()) {
            throw_type_error("array", get_type());
        }
        return m_array > other;
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
    bool operator<=(const mc::array<T>& other) const {
        return !(*this > other);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
    bool operator>=(const mc::array<T>& other) const {
        return !(*this < other);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
    friend bool operator==(const mc::array<T>& val, const variant_base& var) {
        return var.operator==(val);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
    friend bool operator!=(const mc::array<T>& val, const variant_base& var) {
        return !(var.operator==(val));
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
    friend bool operator<(const mc::array<T>& val, const variant_base& var) {
        return var.operator>(val);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
    friend bool operator>(const mc::array<T>& val, const variant_base& var) {
        return var.operator<(val);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
    friend bool operator<=(const mc::array<T>& val, const variant_base& var) {
        return var.operator>=(val);
    }
    template <typename T, std::enable_if_t<mc::is_variant_constructible_v<T>, int> = 0>
    friend bool operator>=(const mc::array<T>& val, const variant_base& var) {
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
    bool same_type_equal(const variant_base& other) const;
    bool other_type_equal(const variant_base& other) const;
    bool same_type_less(const variant_base& other) const;
    bool other_type_less(const variant_base& other) const;

protected:
    union {
        double             m_double;
        int64_t            m_int64;
        uint64_t           m_uint64;
        bool               m_bool;
        string_ptr_type    m_string_ptr;
        blob_ptr_type      m_blob_ptr;
        array_type         m_array; // array 本身就是引用语义，不需要指针
        object_type        m_object;
        extension_ptr_type m_extension;
    };
    struct {
        type_id m_type     : 5; // 5 bits for type_id (0-31, supports up to 32 enum values)
        bool    m_is_fixed : 1; // 1 bit for is_fixed_type flag
        uint8_t m_reserved : 2; // 2 bits reserved for future use
    };
};

// 浮点数
template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
void to_variant(const T& var, variant_base& vo) {
    variant_base(static_cast<double>(var)).swap(vo);
}
template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
void from_variant(const variant_base& var, T& vo) {
    vo = static_cast<T>(var.as_double());
}

// 整数
template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
void to_variant(const T& var, variant_base& vo) {
    variant_base(detail::fixed_integer_type<T>(), var).swap(vo);
}
template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
void from_variant(const variant_base& var, T& vo) {
    if constexpr (std::is_signed_v<T>) {
        vo = static_cast<T>(var.as_int64());
    } else if constexpr (std::is_unsigned_v<T>) {
        vo = static_cast<T>(var.as_uint64());
    }
}

// variant_base
template <typename T, std::enable_if_t<mc::is_variant_v<T>, int> = 0>
void to_variant(const T& var, variant_base& vo) {
    vo = var;
}
inline void from_variant(const variant_base& var, variant_base& vo) {
    vo = var;
}

// bool
MC_API void to_variant(bool var, variant_base& vo);
MC_API void from_variant(const variant_base& var, bool& vo);

// string
template <typename Traits, typename Alloc>
void to_variant(const std::basic_string<char, Traits, Alloc>& var, variant_base& vo) {
    variant_base(var).swap(vo);
}
inline void to_variant(std::string_view var, variant_base& vo) {
    vo = var;
}
template <typename Traits, typename Alloc>
void from_variant(const variant_base& var, std::basic_string<char, Traits, Alloc>& vo) {
    if (var.is_string()) {
        vo = var.get_string();
    } else {
        vo = var.as_string();
    }
}

// const char*
MC_API void to_variant(const char* var, variant_base& vo);
MC_API void from_variant(const variant_base& var, const char*& vo);

// char *
MC_API void to_variant(char* var, variant_base& vo);
MC_API void from_variant(const variant_base& var, char*& vo);

// blob_base
template <typename Alloc>
void to_variant(const blob_base<Alloc>& var, variant_base& vo) {
    variant_base(var).swap(vo);
}
template <typename Alloc>
void from_variant(const variant_base& var, blob_base<Alloc>& vo) {
    vo = var.as_blob();
}

// array_type
MC_API void to_variant(const variants& var, variant_base& vo);
MC_API void from_variant(const variant_base& var, variants& vo);

// dict
MC_API void to_variant(const dict& var, variant_base& vo);
MC_API void from_variant(const variant_base& var, dict& vo);

// 为继承自 variant_extension 的类型提供 to_variant 转换
template <typename T,
          std::enable_if_t<std::is_base_of_v<variant_extension_base, T>, int> = 0>
void to_variant(const T& var, variant_base& vo) {
    variant_base(var.clone()).swap(vo);
}
template <typename T,
          std::enable_if_t<std::is_base_of_v<variant_extension_base, T>, int> = 0>
void from_variant(const variant_base& var, T& vo) {
    if (var.is_extension()) {
        auto ext = mc::dynamic_pointer_cast<T>(var.as_extension());
        if (ext) {
            vo = *ext;
            return;
        }
    }
    throw_type_error("extension", var.get_type());
}

// ======== 算术运算与基本类型的友元运算符 ========

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base operator+(T lhs, const variant_base& rhs) {
    return variant_base(lhs) + rhs;
}

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base operator-(T lhs, const variant_base& rhs) {
    return variant_base(lhs) - rhs;
}

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base operator*(T lhs, const variant_base& rhs) {
    return rhs * lhs;
}

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base operator/(T lhs, const variant_base& rhs) {
    return variant_base(lhs) / rhs;
}

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base operator%(T lhs, const variant_base& rhs) {
    return variant_base(lhs) % rhs;
}

// ======== 位运算与基本类型的友元运算符 ========

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base operator&(T lhs, const variant_base& rhs) {
    return rhs & lhs;
}

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base operator|(T lhs, const variant_base& rhs) {
    return rhs | lhs;
}

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base operator^(T lhs, const variant_base& rhs) {
    return rhs ^ lhs;
}

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base operator<<(T lhs, const variant_base& rhs) {
    return variant_base(lhs) << rhs;
}

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base operator>>(T lhs, const variant_base& rhs) {
    return variant_base(lhs) >> rhs;
}

// ======== 字符串操作的友元运算符 ========
MC_API variant_base operator+(std::string_view lhs, const variant_base& rhs);
inline variant_base operator+(const char* lhs, const variant_base& rhs) {
    return std::string_view(lhs) + rhs;
}
inline variant_base operator+(const std::string& lhs, const variant_base& rhs) {
    return std::string_view(lhs) + rhs;
}

MC_API std::ostream& operator<<(std::ostream& os, const variant_base& v);

} // namespace mc

namespace std {

template <>
struct hash<mc::variant_base> {
    size_t operator()(const mc::variant_base& v) const {
        return v.hash();
    }
};
} // namespace std
#endif // MC_VARIANT_VARIANT_BASE_H