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
 * @file reflection.h
 * @brief 反射元数据缓存
 */
#ifndef MC_REFLECTION_METADATA_H
#define MC_REFLECTION_METADATA_H

#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>

#include <mc/dict.h>
#include <mc/memory.h>
#include <mc/reflect/metadata.h>
#include <mc/reflect/metadata_info.h>
#include <mc/variant.h>

namespace mc::reflect {

template <typename T>
class reflected_object_impl;

namespace detail {
template <typename T>
void unregister_reflection_type();

using unregister_reflection_t = void (*)();

class MC_API registered_reflection_bridge : public reflection_base {
public:
    explicit registered_reflection_bridge(unregister_reflection_t unregister_type) noexcept;
    ~registered_reflection_bridge() override;

private:
    unregister_reflection_t m_unregister_type;
};

struct struct_reflection_ops {
    reflected_object_ptr (*create_object)();
};

class MC_API struct_reflection_bridge : public registered_reflection_bridge {
public:
    struct_reflection_bridge(mc::string_view type_name, type_id_type type_id, const struct_metadata& metadata,
                             const struct_reflection_ops& ops, unregister_reflection_t unregister_type) noexcept;

    reflected_object_ptr         create_object() override;
    mc::string_view              get_type_name() const override;
    type_id_type                 get_type_id() const override;
    std::vector<type_id_type>    get_base_type_ids() const override;
    bool                         is_derived_from(type_id_type base_type_id) const override;
    const property_type_info*    get_property_info(mc::string_view name) const override;
    const method_type_info*      get_method_info(mc::string_view name) const override;
    const base_class_type_info*  get_base_class_info(mc::string_view name) const override;
    const member_info_base*      get_custom_info(mc::string_view name, size_t reflect_type) const override;

    const property_type_info*   get_property_info(mc::quark name) const override;
    const method_type_info*     get_method_info(mc::quark name) const override;
    const base_class_type_info* get_base_class_info(mc::quark name) const override;
    const member_info_base*     get_custom_info(mc::quark name, size_t reflect_type) const override;

    std::vector<mc::string_view> get_property_names() const override;
    std::vector<mc::string_view> get_method_names() const override;

protected:
    const struct_metadata& get_metadata_ref() const noexcept;

private:
    mc::string_view              m_type_name;
    type_id_type                 m_type_id;
    const struct_metadata*       m_metadata;
    const struct_reflection_ops* m_ops;
};

struct reflected_object_ops {
    const reflection_base& (*get_reflection)();
    const void* (*get_const_object)(const reflected_object*) noexcept;
    void* (*get_object)(reflected_object*) noexcept;
};

class MC_API reflected_object_bridge : public reflected_object {
public:
    explicit reflected_object_bridge(const reflected_object_ops& ops) noexcept;

    type_id_type get_type_id() const override;

    variant get_property(mc::string_view name) const override;
    void    set_property(mc::string_view name, const variant& value) override;

    variant      invoke_method(mc::string_view name, const std::vector<variant>& args) override;
    async_result async_invoke_method(mc::string_view name, const std::vector<variant>& args) override;

    std::vector<mc::string_view> get_property_names() const override;
    std::vector<mc::string_view> get_method_names() const override;

protected:
    const reflected_object_ops& get_ops() const noexcept;

private:
    const reflected_object_ops* m_ops;
};

template <typename T>
const reflected_object_ops& get_reflected_object_ops();

template <typename T>
reflected_object_ptr create_struct_reflected_object()
{
    if constexpr (std::is_abstract_v<T>) {
        MC_THROW(mc::bad_type_exception, "抽象类型不能创建对象: ${type}", ("type", reflector<T>::get_name()));
    } else {
        return reflected_object_ptr{new reflected_object_impl<T>(std::make_shared<T>())};
    }
}

template <typename T>
const struct_reflection_ops& get_struct_reflection_ops()
{
    static const struct_reflection_ops ops = {
        &create_struct_reflected_object<T>,
    };
    return ops;
}
} // namespace detail

/**
 * @brief 反射对象包装器实现
 *
 * 包装实际对象，提供动态访问接口
 *
 * @tparam T 被包装的对象类型
 */
template <typename T>
class reflected_object_impl : public detail::reflected_object_bridge {
public:
    explicit reflected_object_impl(std::shared_ptr<T> obj)
        : detail::reflected_object_bridge(detail::get_reflected_object_ops<T>()), m_obj(std::move(obj))
    {}

    const std::shared_ptr<T>& get_object() const;

private:
    std::shared_ptr<T> m_obj;
};

template <typename T>
class reflection<T, std::enable_if_t<is_reflectable<T>() && !std::is_enum<T>()>>
    : public detail::struct_reflection_bridge {
public:
    using reflection_ptr = reflection_metadata_ptr;

    ~reflection() override = default;

    const property_info_base<T>* get_property_info(mc::string_view name) const override
    {
        return static_cast<const property_info_base<T>*>(detail::struct_reflection_bridge::get_property_info(name));
    }

    // quark 重载
    const property_info_base<T>* get_property_info(mc::quark name) const override
    {
        return static_cast<const property_info_base<T>*>(detail::struct_reflection_bridge::get_property_info(name));
    }

    const method_info_base<T>* get_method_info(mc::string_view name) const override
    {
        return static_cast<const method_info_base<T>*>(detail::struct_reflection_bridge::get_method_info(name));
    }

    const method_info_base<T>* get_method_info(mc::quark name) const override
    {
        return static_cast<const method_info_base<T>*>(detail::struct_reflection_bridge::get_method_info(name));
    }

    template <typename M>
    const method_info_base<T>* get_method_info(M T::* member_ptr) const
    {
        auto offset = get_function_offset(member_ptr);
        return static_cast<const method_info_base<T>*>(get_metadata_ref().get_method_info(offset));
    }

    const property_info_base<T>* get_property_info(size_t offset) const
    {
        return static_cast<const property_info_base<T>*>(get_metadata_ref().get_property_info(offset));
    }

    template <typename M, typename BaseT,
              typename = std::enable_if_t<std::is_same_v<T, BaseT> || std::is_base_of_v<BaseT, T>>>
    const property_info_base<T>* get_property_info(M BaseT::* member) const
    {
        return get_property_info(MC_MEMBER_OFFSETOF(T, member));
    }

    mc::variant get_property(const T& obj, mc::string_view key) const
    {
        const property_info_base<T>* property = get_property_info(key);
        if (property) {
            return property->get_value(obj);
        }
        return mc::variant();
    }

    mc::variant get_property(const T& obj, mc::quark key) const
    {
        const property_info_base<T>* property = get_property_info(key);
        if (property) {
            return property->get_value(obj);
        }
        return mc::variant();
    }

    const base_class_info_base<T>* get_base_class_info(mc::string_view name) const override
    {
        return static_cast<const base_class_info_base<T>*>(detail::struct_reflection_bridge::get_base_class_info(name));
    }

    const base_class_info_base<T>* get_base_class_info(mc::quark name) const override
    {
        return static_cast<const base_class_info_base<T>*>(detail::struct_reflection_bridge::get_base_class_info(name));
    }

    mc::variant get_property(const T& obj, mc::string_view key, mc::string_view base_class_name) const
    {
        const auto* base_class_info = get_base_class_info(base_class_name);
        if (base_class_info) {
            return base_class_info->get_value(obj, key);
        }

        return mc::variant();
    }

    mc::dict get_all_properties(const T& obj) const
    {
        mc::dict result;
        get_metadata_ref().visit_properties([&](const property_type_info* property) {
            const auto* p   = static_cast<const property_info_base<T>*>(property);
            result[p->name] = p->get_value(obj);
            return visit_status::VS_CONTINUE;
        });
        return result;
    }

    mc::variant invoke_method(T& obj, mc::string_view method_name, const mc::variants& args = {}) const
    {
        const method_info_base<T>* method = get_method_info(method_name);
        if (method) {
            return method->invoke(obj, args);
        }

        throw_method_not_exist(method_name);
    }

    mc::variant invoke_method(mc::string_view method_name, const mc::variants& args = {}) const
    {
        const method_info_base<T>* method = get_method_info(method_name);
        if (method && method->is_static()) {
            return method->invoke(args);
        }

        throw_method_not_exist(method_name);
    }

    async_result async_invoke_method(T& obj, mc::string_view method_name, const mc::variants& args = {}) const
    {
        const method_info_base<T>* method = get_method_info(method_name);
        if (method) {
            return method->async_invoke(obj, args);
        }

        throw_method_not_exist(method_name);
    }

    async_result async_invoke_method(mc::string_view method_name, const mc::variants& args = {}) const
    {
        const method_info_base<T>* method = get_method_info(method_name);
        if (method && method->is_static()) {
            return method->async_invoke(args);
        }

        throw_method_not_exist(method_name);
    }

    bool set_property(T& obj, mc::string_view key, const mc::variant& value) const
    {
        const property_info_base<T>* property = get_property_info(key);
        if (property) {
            property->set_value(obj, value);
            return true;
        }
        return false;
    }

    bool set_property(T& obj, mc::quark key, const mc::variant& value) const
    {
        const property_info_base<T>* property = get_property_info(key);
        if (property) {
            property->set_value(obj, value);
            return true;
        }
        return false;
    }

    bool set_property(T& obj, mc::string_view key, mc::string_view base_class_name, const mc::variant& value) const
    {
        const auto* base_class_info = get_base_class_info(base_class_name);
        if (base_class_info) {
            base_class_info->set_value(obj, key, value);
            return true;
        }
        return false;
    }

    mc::string_view get_property_name(size_t offset) const
    {
        const property_info_base<T>* property = get_property_info(offset);
        if (property) {
            return property->name;
        }
        return {};
    }

    template <typename M, typename BaseT,
              typename = std::enable_if_t<std::is_same_v<T, BaseT> || std::is_base_of_v<BaseT, T>>>
    mc::string_view get_property_name(M BaseT::* member)
    {
        return get_property_name(MC_MEMBER_OFFSETOF(T, member));
    }

    reflection_ptr shared_from_this()
    {
        return reflection_ptr(this);
    }

private:
    template <typename, typename>
    friend struct reflector; // 需要访问 instance

    static reflection<T>& instance()
    {
        return *static_cast<reflection<T>*>(instance_ptr().get());
    }

    static reflection_ptr* create_instance_holder()
    {
        return new reflection_ptr(new reflection<T>());
    }

    static reflection_ptr& instance_ptr()
    {
        return detail::reflection_singleton<T>::instance_with_creator(&create_instance_holder);
    }

    static bool has_instance()
    {
        return detail::reflection_singleton<T>::try_get() != nullptr;
    }

    static void reset_instance_for_test()
    {
        detail::reflection_singleton<T>::reset_for_test();
    }

    reflection()
        : detail::struct_reflection_bridge(reflector<T>::get_name(), reflector<T>::get_type_id(),
                                           reflector<T>::get_metadata(), detail::get_struct_reflection_ops<T>(),
                                           &detail::unregister_reflection_type<T>)
    {}
};

template <typename T>
auto& get_reflection()
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(is_reflectable<clean_type>(), "类型必须支持反射才能使用");
    return reflector<clean_type>::get_reflection();
}

namespace detail {
template <typename T>
const reflection_base& reflected_object_get_reflection()
{
    return get_reflection<T>();
}

template <typename T>
const void* reflected_object_get_const_object(const reflected_object* obj) noexcept
{
    return static_cast<const reflected_object_impl<T>*>(obj)->get_object().get();
}

template <typename T>
void* reflected_object_get_object(reflected_object* obj) noexcept
{
    return static_cast<reflected_object_impl<T>*>(obj)->get_object().get();
}

template <typename T>
const reflected_object_ops& get_reflected_object_ops()
{
    static const reflected_object_ops ops = {
        &reflected_object_get_reflection<T>,
        &reflected_object_get_const_object<T>,
        &reflected_object_get_object<T>,
    };
    return ops;
}
} // namespace detail

template <typename C, typename BaseT>
mc::variant base_class_info<C, BaseT>::get_value(const C& obj, mc::string_view name) const
{
    return get_reflection<BaseT>().get_property(get_object(obj), name);
}

template <typename C, typename BaseT>
void base_class_info<C, BaseT>::set_value(C& obj, mc::string_view name, const mc::variant& value) const
{
    get_reflection<BaseT>().set_property(get_object(obj), name, value);
}

template <typename C, typename BaseT>
mc::variant base_class_info<C, BaseT>::invoke(C& obj, mc::string_view name, const mc::variants& args) const
{
    return get_reflection<BaseT>().invoke_method(get_object(obj), name, args);
}

template <typename C, typename BaseT>
async_result base_class_info<C, BaseT>::async_invoke(C& obj, mc::string_view name, const mc::variants& args) const
{
    return get_reflection<BaseT>().async_invoke_method(get_object(obj), name, args);
}

template <typename T>
const std::shared_ptr<T>& reflected_object_impl<T>::get_object() const
{
    return m_obj;
}

} // namespace mc::reflect

#endif // MC_REFLECT_METADATA_H