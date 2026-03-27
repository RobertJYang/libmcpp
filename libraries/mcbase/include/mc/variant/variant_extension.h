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
 * @file variant_extension.h
 * @brief 定义了 variant 扩展类型的基类，用于管理复杂类型
 */
#ifndef MC_VARIANT_EXTENSION_H
#define MC_VARIANT_EXTENSION_H

#include <functional>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include <mc/memory.h>
#include <mc/traits.h>
#include <mc/variant/copy_context.h>
#include <mc/variant/extension_type_id.h>
#include <mc/variant/variant_common.h>

namespace mc {

struct variant_payload_type {
    mc::string_view                    name;
    const variant_payload_type* const* bases;
    std::size_t                        base_count;

    constexpr variant_payload_type()
        : name("unknown", 7), bases(nullptr), base_count(0)
    {}

    constexpr variant_payload_type(mc::string_view name_, const variant_payload_type* const* bases_ = nullptr,
                                   std::size_t base_count_ = 0)
        : name(name_), bases(bases_), base_count(base_count_)
    {}
};

template <typename T>
struct variant_payload_type_traits {
    static const variant_payload_type* get()
    {
        return nullptr;
    }
};

class variant_extension_base;

namespace detail {

inline bool payload_type_is_a(const variant_payload_type* source, const variant_payload_type* target)
{
    if (!source || !target) {
        return false;
    }
    if (source == target) {
        return true;
    }
    for (std::size_t i = 0; i < source->base_count; ++i) {
        if (payload_type_is_a(source->bases[i], target)) {
            return true;
        }
    }
    return false;
}

template <typename Payload, typename = void>
struct has_bind_extension_owner : std::false_type {};

template <typename Payload>
struct has_bind_extension_owner<Payload,
                                std::void_t<decltype(std::declval<Payload&>().__mc_bind_extension_owner(
                                    std::declval<variant_extension_base*>()))>> : std::true_type {};

template <typename Traits, typename Payload, typename = void>
struct has_traits_bind_owner : std::false_type {};

template <typename Traits, typename Payload>
struct has_traits_bind_owner<Traits, Payload,
                             std::void_t<decltype(Traits::bind_owner(std::declval<Payload&>(),
                                                                     std::declval<variant_extension_base*>()))>>
    : std::true_type {};

template <typename Traits, typename = void>
struct has_traits_payload_type : std::false_type {};

template <typename Traits>
struct has_traits_payload_type<Traits, std::void_t<decltype(Traits::payload_type())>> : std::true_type {};

template <typename Traits, typename Payload, typename = void>
struct has_traits_upcast : std::false_type {};

template <typename Traits, typename Payload>
struct has_traits_upcast<
    Traits, Payload,
    std::void_t<decltype(Traits::upcast(std::declval<Payload*>(), std::declval<const variant_payload_type&>()))>>
    : std::true_type {};

template <typename Traits, typename Payload, typename = void>
struct has_traits_const_upcast : std::false_type {};

template <typename Traits, typename Payload>
struct has_traits_const_upcast<Traits, Payload,
                               std::void_t<decltype(Traits::upcast(std::declval<const Payload*>(),
                                                                   std::declval<const variant_payload_type&>()))>>
    : std::true_type {};

template <typename Traits, typename = void>
struct has_traits_type_info : std::false_type {};

template <typename Traits>
struct has_traits_type_info<Traits, std::void_t<decltype(Traits::get_type_info())>> : std::true_type {};

template <typename Traits, typename Payload, typename = void>
struct has_traits_deep_copy : std::false_type {};

template <typename Traits, typename Payload>
struct has_traits_deep_copy<
    Traits, Payload,
    std::void_t<decltype(Traits::deep_copy(std::declval<const Payload&>(), std::declval<detail::copy_context*>()))>>
    : std::true_type {};

template <typename Payload, typename = void>
struct has_std_hash : std::false_type {};

template <typename Payload>
struct has_std_hash<Payload, std::void_t<decltype(std::hash<Payload>{}(std::declval<const Payload&>()))>>
    : std::true_type {};

template <typename Traits, typename Payload>
void bind_extension_owner(Payload& payload, variant_extension_base* owner)
{
    if constexpr (has_traits_bind_owner<Traits, Payload>::value) {
        Traits::bind_owner(payload, owner);
    } else if constexpr (has_bind_extension_owner<Payload>::value) {
        payload.__mc_bind_extension_owner(owner);
    } else {
        MC_UNUSED(payload);
        MC_UNUSED(owner);
    }
}

template <typename Traits, typename Payload>
const variant_payload_type* get_payload_type()
{
    if constexpr (has_traits_payload_type<Traits>::value) {
        return Traits::payload_type();
    } else {
        return variant_payload_type_traits<Payload>::get();
    }
}

template <typename Traits, typename Payload>
void* upcast_payload(Payload* payload, const variant_payload_type& target)
{
    if (!payload) {
        return nullptr;
    }
    if constexpr (has_traits_upcast<Traits, Payload>::value) {
        return Traits::upcast(payload, target);
    } else {
        auto* source = get_payload_type<Traits, Payload>();
        if (source && source == &target) {
            return payload;
        }
        return nullptr;
    }
}

template <typename Traits, typename Payload>
const void* upcast_payload(const Payload* payload, const variant_payload_type& target)
{
    if (!payload) {
        return nullptr;
    }
    if constexpr (has_traits_const_upcast<Traits, Payload>::value) {
        return Traits::upcast(payload, target);
    } else {
        return upcast_payload<Traits>(const_cast<Payload*>(payload), target);
    }
}

template <typename Traits, typename Payload>
extension_type_info get_extension_type_info()
{
    if constexpr (has_traits_type_info<Traits>::value) {
        return Traits::get_type_info();
    } else {
        return extension_type_info_traits<Payload>::get();
    }
}

template <typename Extension, typename Payload>
mc::shared_ptr<variant_extension_base> deep_copy_extension(const Payload& payload, detail::copy_context* ctx)
{
    using traits_type = typename Extension::traits_type;
    if constexpr (has_traits_deep_copy<traits_type, Payload>::value) {
        return traits_type::deep_copy(payload, ctx);
    } else {
        MC_UNUSED(ctx);
        return mc::make_shared<Extension>(std::in_place, payload);
    }
}

template <typename T>
T* cast_extension_payload(variant_extension_base* ext);

template <typename T>
const T* cast_extension_payload(const variant_extension_base* ext);

} // namespace detail

/**
 * @brief variant 扩展类型基类
 *
 * 所有需要在 variant 中管理的复杂类型都应该继承此类，
 * 提供统一的接口用于复制、克隆、类型信息等操作
 */
class MC_API variant_extension_base : public mc::enable_shared_from_this<variant_extension_base> {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~variant_extension_base();

    /**
     * @brief 创建当前对象的浅拷贝
     * @return 返回当前对象的浅拷贝
     */
    virtual mc::shared_ptr<variant_extension_base> copy() const = 0;

    /**
     * @brief 创建当前对象的深拷贝
     * @param ctx 可选的深拷贝上下文，用于检测循环引用并记录已拷贝对象
     * @return 返回当前对象的深拷贝
     * @note 默认实现委托给 copy()，子类可以重写提供真正的深拷贝
     */
    virtual mc::shared_ptr<variant_extension_base> deep_copy(detail::copy_context* ctx = nullptr) const
    {
        (void)ctx;
        // 深拷贝默认实现为浅拷贝
        return copy();
    }

    /**
     * @brief 克隆对象（向后兼容的非虚函数）
     * @return 返回当前对象的拷贝
     * @note 内部调用 copy()，保持向后兼容
     */
    mc::shared_ptr<variant_extension_base> clone() const
    {
        return copy();
    }

    /**
     * @brief 获取 extension 类型信息
     * @return 返回类型信息（包含类型ID、名称、特征等）
     *
     * 默认实现：返回未知类型
     * 子类应该重写此方法以提供正确的类型信息
     */
    virtual extension_type_info get_type_info() const
    {
        return extension_type_info(); // 返回默认的未知类型
    }

    /**
     * @brief 获取 extension 类型ID
     * @return 返回类型ID
     */
    extension_type_id_t get_type_id() const
    {
        return get_type_info().id;
    }

    /**
     * @brief 获取类型名称
     * @return 返回类型名称字符串
     */
    virtual mc::string_view get_type_name() const
    {
        return get_type_info().name;
    }

    /**
     * @brief 比较两个扩展对象是否相等
     * @param other 要比较的对象
     * @return 如果相等返回 true，否则返回 false
     */
    virtual bool equals(const variant_extension_base& other) const = 0;

    /**
     * @brief 计算对象的哈希值
     * @return 返回哈希值
     */
    virtual std::size_t hash() const
    {
        return reinterpret_cast<std::size_t>(this);
    }

    virtual const variant_payload_type* payload_type() const
    {
        return nullptr;
    }

    virtual void* payload_address()
    {
        return nullptr;
    }

    virtual const void* payload_address() const
    {
        return nullptr;
    }

    virtual void* upcast_payload_to(const variant_payload_type& target)
    {
        MC_UNUSED(target);
        return nullptr;
    }

    virtual const void* upcast_payload_to(const variant_payload_type& target) const
    {
        MC_UNUSED(target);
        return nullptr;
    }

    bool is_payload_type(const variant_payload_type& target) const
    {
        return detail::payload_type_is_a(payload_type(), &target);
    }

    template <typename T>
    T* payload_as()
    {
        return detail::cast_extension_payload<T>(this);
    }

    template <typename T>
    const T* payload_as() const
    {
        return detail::cast_extension_payload<T>(this);
    }

    virtual int64_t as_int64() const
    {
        return 0;
    }
    virtual uint64_t as_uint64() const
    {
        return 0;
    }
    virtual double as_double() const
    {
        return 0;
    }
    virtual bool as_bool() const
    {
        return false;
    }
    virtual mc::string as_string() const
    {
        return mc::string(get_type_name());
    }
    virtual void visit(std::function<void(const mc::variant&)>&& visitor) const
    {
        MC_UNUSED(visitor);
    }

    /**
     * @brief 查询是否支持零开销的引用访问
     * @return true 表示 extension 内部存储就是 variant，支持返回引用；false 需要值拷贝
     *
     * 如果返回 true，operator[] 将调用 get_ptr/get_ref 获取引用，零开销
     * 如果返回 false，operator[] 将调用 get 获取值，有拷贝开销
     */
    virtual bool supports_reference_access() const
    {
        return false; // 默认不支持，保持向后兼容
    }

    /**
     * @brief 通过索引获取元素指针（零开销访问）
     * @param index 索引位置
     * @return 返回指向内部 variant 的指针，如果不支持则返回 nullptr
     * @note 只有 supports_reference_access() 返回 true 时才会被调用
     */
    virtual mc::variant* get_ptr(std::size_t index)
    {
        MC_UNUSED(index);
        return nullptr; // 默认不支持
    }

    /**
     * @brief 通过索引获取元素指针（const 版本）
     */
    virtual const mc::variant* get_ptr(std::size_t index) const
    {
        MC_UNUSED(index);
        return nullptr; // 默认不支持
    }

    /**
     * @brief 通过键获取元素指针（零开销访问）
     * @param key 键名
     * @return 返回指向内部 variant 的指针，如果不支持则返回 nullptr
     * @note 只有 supports_reference_access() 返回 true 时才会被调用
     */
    virtual mc::variant* get_ptr(mc::string_view key)
    {
        MC_UNUSED(key);
        return nullptr; // 默认不支持
    }

    /**
     * @brief 通过键获取元素指针（const 版本）
     */
    virtual const mc::variant* get_ptr(mc::string_view key) const
    {
        MC_UNUSED(key);
        return nullptr; // 默认不支持
    }

    /**
     * @brief 通过索引获取元素（用于支持 operator[](size_t)）
     * @param index 索引位置
     * @return 返回指定位置的元素（值类型）
     * @throw mc::invalid_op_exception 如果不支持索引访问
     */
    virtual mc::variant get(std::size_t index) const;

    /**
     * @brief 通过索引设置元素（用于支持 operator[](size_t) 赋值）
     * @param index 索引位置
     * @param value 要设置的值
     * @throw mc::invalid_op_exception 如果不支持索引访问
     */
    virtual void set(std::size_t index, const mc::variant& value);

    /**
     * @brief 通过键获取元素（用于支持 operator[](string_view)）
     * @param key 键名
     * @return 返回指定键对应的元素（值类型）
     * @throw mc::invalid_op_exception 如果不支持键访问
     */
    virtual mc::variant get(mc::string_view key) const;

    /**
     * @brief 通过键设置元素（用于支持 operator[](string_view) 赋值）
     * @param key 键名
     * @param value 要设置的值
     * @throw mc::invalid_op_exception 如果不支持键访问
     */
    virtual void set(mc::string_view key, const mc::variant& value);

    // ==================== GC 追踪支持 ====================

    /**
     * @brief 获取 Traceable 接口指针
     *
     * 如果扩展类型同时实现了 Traceable 接口，应重写此方法返回 this。
     * 用于 GC 系统在运行时识别可追踪对象，避免 dynamic_cast。
     *
     * @return 如果实现了 Traceable，返回 Traceable 指针；否则返回 nullptr
     */
    virtual void* as_traceable() const {
        return nullptr;
    }
};

namespace detail {

template <typename T>
T* cast_extension_payload(variant_extension_base* ext)
{
    if (!ext) {
        return nullptr;
    }

    if (auto* payload_type = variant_payload_type_traits<T>::get()) {
        if (auto* payload = ext->upcast_payload_to(*payload_type)) {
            return static_cast<T*>(payload);
        }
    }

    if constexpr (std::is_base_of_v<variant_extension_base, T>) {
        return dynamic_cast<T*>(ext);
    } else {
        return nullptr;
    }
}

template <typename T>
const T* cast_extension_payload(const variant_extension_base* ext)
{
    if (!ext) {
        return nullptr;
    }

    if (auto* payload_type = variant_payload_type_traits<T>::get()) {
        if (auto* payload = ext->upcast_payload_to(*payload_type)) {
            return static_cast<const T*>(payload);
        }
    }

    if constexpr (std::is_base_of_v<variant_extension_base, T>) {
        return dynamic_cast<const T*>(ext);
    } else {
        return nullptr;
    }
}

} // namespace detail

template <typename Payload>
struct default_composed_variant_extension_traits {
    static const variant_payload_type* payload_type()
    {
        return variant_payload_type_traits<Payload>::get();
    }

    static void* upcast(Payload* payload, const variant_payload_type& target)
    {
        auto* source = payload_type();
        if (source && source == &target) {
            return payload;
        }
        return nullptr;
    }

    static const void* upcast(const Payload* payload, const variant_payload_type& target)
    {
        return upcast(const_cast<Payload*>(payload), target);
    }
};

template <typename Payload, typename Traits = default_composed_variant_extension_traits<Payload>>
class composed_variant_extension final : public variant_extension_base {
public:
    using value_type  = Payload;
    using traits_type = Traits;

    template <typename... Args>
    explicit composed_variant_extension(std::in_place_t, Args&&... args)
    {
        ::new (static_cast<void*>(m_storage)) Payload(std::forward<Args>(args)...);
        detail::bind_extension_owner<Traits>(*payload(), this);
    }

    ~composed_variant_extension() override
    {
        payload()->~Payload();
    }

    Payload* payload()
    {
        return reinterpret_cast<Payload*>(m_storage);
    }

    const Payload* payload() const
    {
        return reinterpret_cast<const Payload*>(m_storage);
    }

    mc::shared_ptr<variant_extension_base> copy() const override
    {
        if constexpr (std::is_copy_constructible_v<Payload>) {
            return mc::make_shared<composed_variant_extension>(std::in_place, *payload());
        } else {
            throw std::runtime_error("composed_variant_extension payload does not support copy()");
        }
    }

    mc::shared_ptr<variant_extension_base> deep_copy(detail::copy_context* ctx = nullptr) const override
    {
        if constexpr (detail::has_traits_deep_copy<traits_type, Payload>::value) {
            return detail::deep_copy_extension<composed_variant_extension>(*payload(), ctx);
        } else if constexpr (std::is_copy_constructible_v<Payload>) {
            return detail::deep_copy_extension<composed_variant_extension>(*payload(), ctx);
        } else {
            MC_UNUSED(ctx);
            throw std::runtime_error(
                "composed_variant_extension payload does not support deep_copy()");
        }
    }

    extension_type_info get_type_info() const override
    {
        return detail::get_extension_type_info<Traits, Payload>();
    }

    mc::string_view get_type_name() const override
    {
        if (auto* type = payload_type()) {
            return type->name;
        }
        return variant_extension_base::get_type_name();
    }

    bool equals(const variant_extension_base& other) const override
    {
        if (this == &other) {
            return true;
        }

        auto* other_payload = other.payload_as<Payload>();
        if (!other_payload) {
            return false;
        }

        if constexpr (mc::traits::has_operator_equal_v<Payload, Payload>) {
            return *payload() == *other_payload;
        }
        return payload() == other_payload;
    }

    std::size_t hash() const override
    {
        if constexpr (detail::has_std_hash<Payload>::value) {
            return std::hash<Payload>{}(*payload());
        }
        return reinterpret_cast<std::size_t>(payload());
    }

    const variant_payload_type* payload_type() const override
    {
        return detail::get_payload_type<Traits, Payload>();
    }

    void* payload_address() override
    {
        return payload();
    }

    const void* payload_address() const override
    {
        return payload();
    }

    void* upcast_payload_to(const variant_payload_type& target) override
    {
        return detail::upcast_payload<Traits>(payload(), target);
    }

    const void* upcast_payload_to(const variant_payload_type& target) const override
    {
        return detail::upcast_payload<Traits>(payload(), target);
    }

private:
    alignas(Payload) unsigned char m_storage[sizeof(Payload)];
};

template <typename T>
class variant_extension : public variant_extension_base {
public:
    using value_type = T;

    virtual mc::shared_ptr<variant_extension_base> copy() const override
    {
        return mc::make_shared<T>(static_cast<const T&>(*this));
    }

    virtual extension_type_info get_type_info() const override
    {
        return extension_type_info_traits<T>::get();
    }

    virtual bool equals(const variant_extension_base& other) const override
    {
        // 快速类型检查：先比较类型ID
        if (get_type_id() != other.get_type_id()) {
            return false;
        }

        // 类型相同，进行值比较
        if constexpr (mc::traits::has_operator_equal_v<T, T>) {
            // 类型ID相同，可以安全地 static_cast
            return static_cast<const T&>(*this) == static_cast<const T&>(other);
        }
        return false;
    }

    mc::shared_ptr<T> shared_from_this() const
    {
        return mc::shared_ptr<T>(static_cast<const T*>(this));
    }
    mc::shared_ptr<T> shared_from_this()
    {
        return mc::shared_ptr<T>(static_cast<T*>(this));
    }
    mc::weak_ptr<T> weak_from_this() const
    {
        return mc::weak_ptr<T>(static_cast<const T*>(this));
    }
    mc::weak_ptr<T> weak_from_this()
    {
        return mc::weak_ptr<T>(static_cast<T*>(this));
    }
};

} // namespace mc

#endif // MC_VARIANT_EXTENSION_H