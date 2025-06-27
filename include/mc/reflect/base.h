/*
 * Copyright (c) 2023 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MC_REFLECT_BASE_H
#define MC_REFLECT_BASE_H

#include <mc/variant.h>

#include <memory>
#include <string_view>
#include <vector>

namespace mc::engine {
template <typename T>
class result;
}

namespace mc::reflect {

using async_result = mc::engine::result<mc::variant>;
struct property_type_info;
struct method_type_info;

// 异常抛出辅助函数
[[noreturn]] void throw_method_arg_not_enough(std::string_view method_name, size_t expect_count,
                                              size_t actual_count);
[[noreturn]] void throw_method_not_exist(std::string_view method_name);
[[noreturn]] void throw_not_enum_type(std::string_view type_name);
[[noreturn]] void throw_enum_value_not_found(std::string_view type_name, std::string_view value_name);
[[noreturn]] void throw_enum_value_not_found(std::string_view type_name, uint64_t value);
[[noreturn]] void throw_bad_enum_cast(int64_t i, const char* e);
[[noreturn]] void throw_bad_enum_cast(const char* k, const char* e);
[[noreturn]] void throw_variant_cast(const char* k, const char* e);
[[noreturn]] void throw_enum_not_support_create_object(std::string_view type_name);

/**
 * @brief 反射器模板类
 *
 * @tparam T 要反射的类型
 */
template <typename T>
struct reflector {
    using is_defined = std::false_type;
    using is_enum    = std::false_type;
};

/**
 * 检查类型是否可反射
 * @tparam T 要检查的类型
 * @return 如果类型可反射则返回true，否则返回false
 */
template <typename T>
constexpr bool is_reflectable() {
    return reflector<std::decay_t<T>>::is_defined::value;
}

/**
 * 检查类型是否为枚举
 * @tparam T 要检查的类型
 * @return 如果类型是枚举则返回true，否则返回false
 */
template <typename T>
constexpr bool is_enum() {
    return reflector<T>::is_enum::value;
}

/**
 * 检查类型是否为非反射的普通枚举
 * @tparam T 要检查的类型
 * @return 如果类型是普通的枚举则返回true，否则返回false
 */
template <typename T>
constexpr bool is_normal_enum() {
    return std::is_enum_v<T> && !is_reflectable<T>();
}

/**
 * @brief 反射对象基类
 *
 * 提供对反射对象的动态访问接口，无需知道具体类型
 */
class reflected_object {
public:
    virtual ~reflected_object() = default;

    /**
     * @brief 获取类型ID
     * @return int 类型ID
     */
    virtual int get_type_id() const = 0;

    /**
     * @brief 获取属性值
     * @param name 属性名
     * @return variant 属性值
     */
    virtual variant get_property(std::string_view name) const = 0;

    /**
     * @brief 设置属性值
     * @param name 属性名
     * @param value 属性值
     */
    virtual void set_property(std::string_view name, const variant& value) = 0;

    /**
     * @brief 调用方法
     * @param name 方法名
     * @param args 参数列表
     * @return variant 返回值
     */
    virtual variant      invoke_method(std::string_view name, const std::vector<variant>& args)       = 0;
    virtual async_result async_invoke_method(std::string_view name, const std::vector<variant>& args) = 0;

    /**
     * @brief 获取所有属性名
     * @return std::vector<std::string_view> 属性名列表
     */
    virtual std::vector<std::string_view> get_property_names() const = 0;

    /**
     * @brief 获取所有方法名
     * @return std::vector<std::string_view> 方法名列表
     */
    virtual std::vector<std::string_view> get_method_names() const = 0;
};

/**
 * @brief 反射元数据基类
 *
 * 提供类型信息和对象创建功能
 */
class reflection_metadata_base {
public:
    virtual ~reflection_metadata_base() = default;

    /**
     * @brief 创建反射对象
     * @return std::shared_ptr<reflected_object> 反射对象实例
     */
    virtual std::shared_ptr<reflected_object> create_object() = 0;

    /**
     * @brief 获取类型名
     * @return std::string_view 类型名
     */
    virtual std::string_view get_type_name() const = 0;

    /**
     * @brief 获取基类类型ID列表
     * @return std::vector<int> 基类类型ID列表
     */
    virtual std::vector<int> get_base_type_ids() const = 0;

    /**
     * @brief 检查是否是某个类型的子类
     * @param base_type_id 基类类型ID
     * @return bool 是否为子类
     */
    virtual bool is_derived_from(int base_type_id) const = 0;

    /**
     * @brief 获取属性信息
     * @param name 属性名
     * @return const property_type_info* 属性信息指针，需要转换为具体类型
     */
    virtual const property_type_info* get_property_info(std::string_view name) const = 0;

    /**
     * @brief 获取方法信息
     * @param name 方法名
     * @return const method_type_info* 方法信息指针，需要转换为具体类型
     */
    virtual const method_type_info* get_method_info(std::string_view name) const = 0;

    /**
     * @brief 获取所有属性名
     * @return std::vector<std::string_view> 属性名列表
     */
    virtual std::vector<std::string_view> get_property_names() const = 0;

    /**
     * @brief 获取所有方法名
     * @return std::vector<std::string_view> 方法名列表
     */
    virtual std::vector<std::string_view> get_method_names() const = 0;

    /**
     * @brief 获取是否为枚举类型
     * @return bool 是否为枚举类型
     */
    bool is_enum() const {
        return m_is_enum;
    }

    virtual uint64_t get_enum_value(std::string_view name) const {
        throw_not_enum_type(get_type_name());
    }
    virtual std::string_view get_enum_name(uint64_t value) const {
        throw_not_enum_type(get_type_name());
    }
    virtual std::vector<std::string_view> get_enum_names() const {
        throw_not_enum_type(get_type_name());
    }
    virtual bool has_enum_value(std::string_view name) const {
        throw_not_enum_type(get_type_name());
    }

protected:
    bool m_is_enum = false;
};

using reflected_object_ptr     = std::shared_ptr<reflected_object>;
using reflection_metadata_ptr  = std::shared_ptr<reflection_metadata_base>;
using reflection_metadata_wptr = std::weak_ptr<reflection_metadata_base>;

} // namespace mc::reflect

#endif // MC_REFLECT_BASE_H