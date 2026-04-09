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
#include <mc/signal_slot.h>

namespace mc::reflect {

class reflection_factory;
using factory_ptr  = mc::shared_ptr<reflection_factory>;
using factory_wptr = mc::weak_ptr<reflection_factory>;

constexpr inline bool is_valid_namespace(std::string_view name)
{
    return is_valid_type_name(name);
}

struct global_namespace {
    constexpr static std::string_view factory_name = std::string_view{};
};

/**
 * @brief 反射工厂类
 *
 * 管理类型注册和反射对象创建，支持模块系统
 */
class MC_API reflection_factory : public mc::enable_shared_from_this<reflection_factory> {
public:
    /**
     * @brief 获取全局反射工厂实例
     * @return reflection_factory& 工厂实例引用
     */
    static reflection_factory& global();
    static factory_ptr&        global_ptr();
    static factory_ptr         try_global_ptr();
    static void                reset_global();

    /**
     * @brief 获取单例实例，指定工厂标签
     * @return reflection_factory& 工厂实例引用
     */
    template <typename NameSpace>
    static reflection_factory& instance()
    {
        if constexpr (std::is_same_v<NameSpace, global_namespace>) {
            return global();
        } else {
            return *instance_ptr<NameSpace>();
        }
    }

    /**
     * @brief 获取单例实例指针
     * @return factory_ptr& 工厂实例指针
     * @note 使用智能指针管理单例实例，确保当前实例注册到其他工厂时，其他工厂可以感知当前实例被销毁
     */
    template <typename NameSpace>
    static factory_ptr& instance_ptr()
    {
        if constexpr (std::is_same_v<NameSpace, global_namespace>) {
            return global_ptr();
        } else {
            static_assert(is_valid_namespace(NameSpace::factory_name), "factory_name is not valid");
            return mc::singleton<factory_ptr, NameSpace>::instance_with_creator([]() {
                return new factory_ptr(
                    new reflection_factory(NameSpace::factory_name, mc::pretty_name<NameSpace>(), false));
            });
        }
    }

    template <typename NameSpace>
    static factory_ptr try_instance_ptr()
    {
        if constexpr (std::is_same_v<NameSpace, global_namespace>) {
            return try_global_ptr();
        } else {
            auto p = mc::singleton<factory_ptr, NameSpace>::try_get();
            return p ? *p : factory_ptr();
        }
    }

    ~reflection_factory();

    /**
     * @brief 获取反射元数据
     * @param type_id 类型ID
     * @return reflection_metadata_ptr 反射元数据
     */
    reflection_metadata_ptr get_metadata(type_id_type type_id);

    /**
     * @brief 通过类型名获取反射元数据（在所有已注册的工厂中查找）
     * @param type_name 类型名
     * @return reflection_metadata_ptr 反射元数据
     */
    reflection_metadata_ptr get_metadata(std::string_view type_name);

    /**
     * @brief 尝试创建反射对象，失败时返回nullptr
     * @param type_id 类型ID
     * @return reflected_object_ptr 反射对象实例，失败时返回nullptr
     */
    reflected_object_ptr try_create_object(type_id_type type_id);

    /**
     * @brief 尝试通过类型名创建反射对象（在所有已注册的工厂中查找），失败时返回nullptr
     * @param type_name 类型名
     * @return reflected_object_ptr 反射对象实例，失败时返回nullptr
     */
    reflected_object_ptr try_create_object(std::string_view type_name);

    /**
     * @brief 创建反射对象
     * @param type_id 类型ID
     * @return reflected_object_ptr 反射对象实例
     */
    reflected_object_ptr create_object(type_id_type type_id);

    /**
     * @brief 通过类型名创建反射对象（在所有已注册的工厂中查找）
     * @param type_name 类型名
     * @return reflected_object_ptr 反射对象实例
     */
    reflected_object_ptr create_object(std::string_view type_name);

    /**
     * @brief 获取类型ID
     * @param type_name 类型名
     * @return int 类型ID，如果不存在返回-1
     */
    type_id_type get_type_id(std::string_view type_name) const;

    /**
     * @brief 获取所有已注册的类型名（包括所有子模块的类型）
     * @return std::vector<std::string> 类型名列表
     */
    std::vector<std::string> get_registered_types() const;

    /**
     * @brief 根据模块路径获取该模块下的所有类型
     * @param module_path 模块路径（如 "mc::devices" 或 "mc.devices"），如果为空，则获取当前工厂下的所有类型
     * @return std::vector<std::pair<std::string_view, type_id_type>> 类型名和类型ID的列表
     */
    std::vector<std::string> get_module_types(std::string_view module_path = std::string_view{}) const;

    /**
     * @brief 获取所有模块路径
     * @return std::vector<std::string> 模块路径列表
     */
    std::vector<std::string> get_module_paths() const;

    /**
     * @brief 检查模块是否存在
     * @param module_path 模块路径
     * @return bool 模块是否存在
     */
    bool has_module(std::string_view module_path) const;

    /**
     * @brief 注册反射工厂
     * @param factory_name 工厂名称
     * @param factory 反射工厂实例
     */
    factory_id_type register_factory(factory_ptr factory);

    /**
     * @brief 注销反射工厂
     * @param factory_name 工厂名称
     */
    void unregister_factory(std::string_view factory_name);

    /**
     * @brief 获取反射工厂
     * @param factory_name 工厂名称
     * @return factory_ptr 反射工厂实例
     */
    factory_ptr get_factory(std::string_view factory_name) const;

    /**
     * @brief 获取反射工厂
     * @param factory_id 工厂ID
     * @return factory_ptr 反射工厂实例
     */
    factory_ptr get_factory_by_id(factory_id_type factory_id) const;

    /**
     * @brief 获取父工厂
     * @return factory_ptr 父工厂实例
     */
    factory_ptr get_parent_factory() const;

    /**
     * @brief 获取所有已注册的工厂名称
     * @return std::vector<std::string> 工厂名称列表
     */
    std::vector<std::string> get_factory_names() const;

    /**
     * @brief 获取工厂名称
     * @return std::string_view 工厂名称
     */
    const std::string& get_factory_name() const;

    /**
     * @brief 获取工厂命名空间类型名
     * @return std::string_view 工厂类型名
     */
    const std::string& get_namespace_type_name() const;

    factory_id_type get_factory_id() const;

    /**
     * @brief 注册类型
     * @tparam T 要注册的类型
     * @return type_id_type 分配的类型ID
     */
    template <typename T>
    type_id_type register_type(type_id_type type_id = INVALID_TYPE_ID)
    {
        return register_type_impl(reflector<T>::get_name(), type_id, []() {
            return reflector<T>::get_reflection().shared_from_this();
        });
    }

    mc::signal<void(type_id_type)>    on_type_unregister;
    mc::signal<void(factory_id_type)> on_factory_unregister;

private:
    reflection_factory(std::string_view factory_name, std::string_view factory_type_name, bool is_global);

    template <typename, typename>
    friend struct reflector; // 反射器需要访问 unregister_type_impl

    template <typename, typename>
    friend class reflection; // 反射元数据需要访问 unregister_type_impl

    friend struct module_node;

    type_id_type register_type_impl(std::string_view type_name, type_id_type type_id,
                                    std::function<reflection_metadata_ptr()>&& creator);
    void         unregister_type_impl(std::string_view type_name);

    class impl;
    std::unique_ptr<impl> m_impl;
};

/**
 * @brief 全局便利函数：尝试通过类型ID创建反射对象，失败时返回nullptr
 * @param type_id 类型ID
 * @return reflected_object_ptr 反射对象实例，失败时返回nullptr
 */
MC_API reflected_object_ptr try_create_object(type_id_type type_id);

/**
 * @brief 全局便利函数：尝试通过类型名创建反射对象，失败时返回nullptr
 * @param type_name 类型名
 * @return reflected_object_ptr 反射对象实例，失败时返回nullptr
 */
MC_API reflected_object_ptr try_create_object(std::string_view type_name);

/**
 * @brief 全局便利函数：通过类型ID创建反射对象
 * @param type_id 类型ID
 * @return reflected_object_ptr 反射对象实例
 */
MC_API reflected_object_ptr create_object(type_id_type type_id);

/**
 * @brief 全局便利函数：通过类型名创建反射对象
 * @param type_name 类型名
 * @return reflected_object_ptr 反射对象实例
 */
MC_API reflected_object_ptr create_object(std::string_view type_name);

/**
 * @brief 获取已注册的所有类型名
 * @return std::vector<std::string> 类型名列表
 */
inline std::vector<std::string> get_registered_types()
{
    return reflection_factory::global().get_registered_types();
}

/**
 * @brief 获取类型ID
 * @param type_name 类型名
 * @return type_id_type 类型ID，如果不存在返回-1
 */
inline type_id_type get_type_id(std::string_view type_name)
{
    return reflection_factory::global().get_type_id(type_name);
}

/**
 * @brief 包装现有对象为反射对象
 * @tparam T 对象类型
 * @param obj 对象实例
 * @return reflected_object_ptr 反射对象包装器
 */
template <typename T>
reflected_object_ptr wrap_object(std::shared_ptr<T> obj)
{
    return std::make_shared<reflected_object_impl<T>>(obj);
}

/**
 * @brief 包装现有对象为反射对象（创建副本）
 * @tparam T 对象类型
 * @param obj 对象实例
 * @return reflected_object_ptr 反射对象包装器
 */
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

template <typename T>
reflection<T, std::enable_if_t<is_reflectable<T>() && std::is_enum<T>()>>::~reflection()
{
    auto factory = try_get_reflect_factory<T>();
    if (factory) {
        factory->unregister_type_impl(reflector<T>::get_name());
    }
}

template <typename T>
reflection<T, std::enable_if_t<is_reflectable<T>() && !std::is_enum<T>()>>::~reflection()
{
    auto factory = try_get_reflect_factory<T>();
    if (factory) {
        factory->unregister_type_impl(reflector<T>::get_name());
    }
}

} // namespace mc::reflect

#endif // MC_REFLECT_REFLECTION_FACTORY_H