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
 * @file reflection_factory.h
 * @brief 反射工厂类定义
 */
#ifndef MC_REFLECT_REFLECTION_FACTORY_H
#define MC_REFLECT_REFLECTION_FACTORY_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>

#include <mc/reflect/base.h>
#include <mc/reflect/reflection.h>
#include <mc/reflect/reflection_enum.h>
#include <mc/signal/signal.h>

namespace mc::reflect {

class reflection_factory;
using factory_ptr  = mc::shared_ptr<reflection_factory>;
using factory_wptr = mc::weak_ptr<reflection_factory>;

constexpr inline bool is_valid_namespace(mc::string_view name)
{
    return is_valid_type_name(name);
}

struct global_namespace {
    constexpr static mc::string_view factory_name = mc::string_view{};
};

namespace detail {

MC_API std::mutex& reflection_factory_singleton_creation_mutex() noexcept;
using reflection_factory_singleton_storage = std::atomic<factory_ptr*>;

MC_API factory_ptr& reflection_factory_singleton_instance_with_creator(reflection_factory_singleton_storage& storage,
                                                                       factory_ptr* (*creator)());
MC_API factory_ptr& reflection_factory_singleton_instance_with_names(reflection_factory_singleton_storage& storage,
                                                                     mc::string_view                       factory_name,
                                                                     mc::string_view namespace_type_name);
MC_API factory_ptr* reflection_factory_singleton_try_get(reflection_factory_singleton_storage& storage) noexcept;
MC_API void         reflection_factory_singleton_reset(reflection_factory_singleton_storage& storage) noexcept;

MC_API void reflection_factory_unregister_type(factory_ptr* factory_ptr_raw, mc::string_view type_name) noexcept;

MC_API factory_ptr* create_factory_holder(mc::string_view factory_name, mc::string_view namespace_type_name);

template <typename NameSpace>
factory_ptr* create_reflection_factory_holder();

template <typename T>
void unregister_reflection_type();

template <typename NameSpace>
class reflection_factory_singleton {
public:
    using creator_t = factory_ptr* (*)();

    static factory_ptr& instance_with_creator(creator_t creator)
    {
        return reflection_factory_singleton_instance_with_creator(instance_ptr(), creator);
    }

    static factory_ptr& instance_with_names(mc::string_view factory_name, mc::string_view namespace_type_name)
    {
        return reflection_factory_singleton_instance_with_names(instance_ptr(), factory_name, namespace_type_name);
    }

    static factory_ptr* try_get()
    {
        return reflection_factory_singleton_try_get(instance_ptr());
    }

    static void reset_for_test()
    {
        reflection_factory_singleton_reset(instance_ptr());
    }

private:
    static reflection_factory_singleton_storage& instance_ptr()
    {
        static reflection_factory_singleton_storage ptr{nullptr};
        return ptr;
    }
};

template <typename T>
reflection_metadata_ptr create_reflection_metadata();

} // namespace detail

/**
 * @brief 反射工厂类
 */
class MC_API reflection_factory : public mc::enable_shared_from_this<reflection_factory> {
public:
    static reflection_factory& global();
    static factory_ptr&        global_ptr();
    static factory_ptr         try_global_ptr();
    static void                reset_global();

    template <typename NameSpace>
    static reflection_factory& instance()
    {
        if constexpr (std::is_same_v<NameSpace, global_namespace>) {
            return global();
        } else {
            return *instance_ptr<NameSpace>();
        }
    }

    template <typename NameSpace>
    static factory_ptr& instance_ptr()
    {
        if constexpr (std::is_same_v<NameSpace, global_namespace>) {
            return global_ptr();
        } else {
            static_assert(is_valid_namespace(NameSpace::factory_name), "factory_name is not valid");
            return detail::reflection_factory_singleton<NameSpace>::instance_with_names(NameSpace::factory_name,
                                                                                        mc::pretty_name<NameSpace>());
        }
    }

    template <typename NameSpace>
    static factory_ptr try_instance_ptr()
    {
        if constexpr (std::is_same_v<NameSpace, global_namespace>) {
            return try_global_ptr();
        } else {
            auto p = detail::reflection_factory_singleton<NameSpace>::try_get();
            return p ? *p : factory_ptr();
        }
    }

    ~reflection_factory();

    reflection_metadata_ptr get_metadata(type_id_type type_id);

    reflection_metadata_ptr get_metadata(mc::string_view type_name);

    reflected_object_ptr try_create_object(type_id_type type_id);

    reflected_object_ptr try_create_object(mc::string_view type_name);

    reflected_object_ptr create_object(type_id_type type_id);

    reflected_object_ptr create_object(mc::string_view type_name);

    type_id_type get_type_id(mc::string_view type_name) const;

    std::vector<mc::string> get_registered_types() const;

    std::vector<mc::string> get_module_types(mc::string_view module_path = mc::string_view{}) const;

    std::vector<mc::string> get_module_paths() const;

    bool has_module(mc::string_view module_path) const;

    factory_id_type register_factory(factory_ptr factory);

    void unregister_factory(mc::string_view factory_name);

    factory_ptr get_factory(mc::string_view factory_name) const;

    factory_ptr get_factory_by_id(factory_id_type factory_id) const;

    factory_ptr get_parent_factory() const;

    std::vector<mc::string> get_factory_names() const;

    mc::string_view get_factory_name() const;

    mc::string_view get_namespace_type_name() const;

    factory_id_type get_factory_id() const;

    template <typename T>
    type_id_type register_type(type_id_type type_id = INVALID_TYPE_ID)
    {
        return register_type_impl(reflector<T>::get_name(), type_id, &detail::create_reflection_metadata<T>);
    }

    mc::signal<void(type_id_type)>    on_type_unregister;
    mc::signal<void(factory_id_type)> on_factory_unregister;

private:
    reflection_factory(mc::string_view factory_name, mc::string_view factory_type_name, bool is_global);

    template <typename, typename>
    friend struct reflector; // 反射器需要访问 unregister_type_impl

    template <typename, typename>
    friend class reflection; // 反射元数据需要访问 unregister_type_impl

    template <typename NameSpace>
    friend factory_ptr* detail::create_reflection_factory_holder();

    template <typename T>
    friend void detail::unregister_reflection_type();

    friend factory_ptr* detail::create_factory_holder(mc::string_view, mc::string_view);
    friend void         detail::reflection_factory_unregister_type(factory_ptr*, mc::string_view) noexcept;

    friend struct module_node;

    type_id_type register_type_impl(mc::string_view type_name, type_id_type type_id,
                                    reflection_metadata_ptr (*creator)());
    void         unregister_type_impl(mc::string_view type_name);

    class impl;
    std::unique_ptr<impl> m_impl;
};

MC_API reflected_object_ptr try_create_object(type_id_type type_id);

MC_API reflected_object_ptr try_create_object(mc::string_view type_name);

MC_API reflected_object_ptr create_object(type_id_type type_id);

MC_API reflected_object_ptr create_object(mc::string_view type_name);

inline std::vector<mc::string> get_registered_types()
{
    return reflection_factory::global().get_registered_types();
}

inline type_id_type get_type_id(mc::string_view type_name)
{
    return reflection_factory::global().get_type_id(type_name);
}

template <typename T>
reflected_object_ptr wrap_object(std::shared_ptr<T> obj)
{
    return std::make_shared<reflected_object_impl<T>>(obj);
}

template <typename T>
reflected_object_ptr wrap_object(const T& obj)
{
    auto copy = std::make_shared<T>(obj);
    return wrap_object(copy);
}

template <typename T>
reflection_factory& get_reflect_factory()
{
    if constexpr (detail::has_reflect_namespace<T>::value) {
        return reflection_factory::instance<typename detail::reflect_namespace<T>::type>();
    } else {
        return reflection_factory::global();
    }
}

template <typename T>
factory_ptr try_get_reflect_factory()
{
    if constexpr (detail::has_reflect_namespace<T>::value) {
        return reflection_factory::try_instance_ptr<typename detail::reflect_namespace<T>::type>();
    } else {
        return reflection_factory::try_global_ptr();
    }
}

namespace detail {

template <typename NameSpace>
factory_ptr* create_reflection_factory_holder()
{
    return create_factory_holder(NameSpace::factory_name, mc::pretty_name<NameSpace>());
}

template <typename T>
reflection_metadata_ptr create_reflection_metadata()
{
    return reflector<T>::get_reflection().shared_from_this();
}

template <typename T>
void unregister_reflection_type()
{
    factory_ptr* ptr = nullptr;
    if constexpr (has_reflect_namespace<T>::value) {
        using ns_type = typename reflect_namespace<T>::type;
        ptr           = reflection_factory_singleton<ns_type>::try_get();
    }
    if (ptr == nullptr) {
        ptr = reflection_factory_singleton<global_namespace>::try_get();
    }
    reflection_factory_unregister_type(ptr, reflector<T>::get_name());
}

} // namespace detail

} // namespace mc::reflect

#endif // MC_REFLECT_REFLECTION_FACTORY_H