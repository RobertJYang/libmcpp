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

#include <mc/memory.h>
#include <mc/string_utils.h>
#include <mc/variant.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/comparison/greater.hpp>
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/preprocessor/facilities/is_empty.hpp>
#include <boost/preprocessor/punctuation/is_begin_parens.hpp>
#include <boost/preprocessor/punctuation/remove_parens.hpp>
#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/pop_front.hpp>
#include <boost/preprocessor/seq/transform.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/variadic/size.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>

namespace mc {
template <typename T>
class result;
}

namespace mc::reflect {

namespace detail {
// 反射名称来自宏参数中的字符串字面量；经函数模板推导为数组长度，避免退化为 const char* 导致失去 constexpr。
template <std::size_t N>
constexpr mc::string_view reflect_name_from_literal(const char (&s)[N]) noexcept
{
    return mc::string_view(s, N - 1);
}

constexpr mc::string_view reflect_name_from_literal(mc::string_view s) noexcept
{
    return s;
}

constexpr mc::string_view reflect_name_from_literal(std::string_view s) noexcept
{
    return reflect_name_from_literal(mc::string_view(s));
}
} // namespace detail

using async_result = mc::result<mc::variant>;
struct property_type_info;
struct method_type_info;
struct base_class_type_info;
struct member_info_base;

// 异常抛出辅助函数
[[noreturn]] MC_API void throw_method_arg_not_enough(mc::string_view method_name, size_t expect_count,
                                                     size_t actual_count);
[[noreturn]] MC_API void throw_method_not_exist(mc::string_view method_name);
[[noreturn]] MC_API void throw_not_enum_type(mc::string_view type_name);
[[noreturn]] MC_API void throw_enum_value_not_found(mc::string_view type_name, mc::string_view value_name);
[[noreturn]] MC_API void throw_enum_value_not_found(mc::string_view type_name, uint64_t value);
[[noreturn]] MC_API void throw_bad_enum_cast(int64_t i, mc::string_view e);
[[noreturn]] MC_API void throw_bad_enum_cast(mc::string_view k, mc::string_view e);
[[noreturn]] MC_API void throw_variant_cast(mc::string_view k, mc::string_view e);
[[noreturn]] MC_API void throw_enum_not_support_create_object(mc::string_view type_name);

using type_id_type       = int64_t;
using local_type_id_type = int32_t;
using factory_id_type    = int32_t;

// 无效的类型ID和工厂ID常量
constexpr type_id_type    INVALID_TYPE_ID    = -1;
constexpr factory_id_type INVALID_FACTORY_ID = -1;

/**
 * @brief 反射器模板类
 *
 * @tparam T 要反射的类型
 */
template <typename T>
struct reflectable {
    using is_defined = std::false_type;
    using is_enum    = std::false_type;
};

template <typename T, typename = void>
struct reflector;

template <typename T>
struct static_metadata;

namespace detail {
template <typename T, typename = void>
struct has_reflectable : std::false_type {};

template <typename T>
struct has_reflectable<T, std::enable_if_t<std::is_same_v<typename T::is_reflectable, std::true_type>, void>>
    : std::true_type {};

} // namespace detail

/**
 * 检查类型是否可反射
 * @tparam T 要检查的类型
 * @return 如果类型可反射则返回true，否则返回false
 */
template <typename T>
constexpr bool is_reflectable()
{
    return reflectable<std::decay_t<T>>::is_defined::value || detail::has_reflectable<T>::value;
}

/**
 * 检查类型是否为枚举
 * @tparam T 要检查的类型
 * @return 如果类型是枚举则返回true，否则返回false
 */
template <typename T>
constexpr bool is_enum()
{
    return reflectable<T>::is_enum::value;
}

/**
 * 检查类型是否为非反射的普通枚举
 * @tparam T 要检查的类型
 * @return 如果类型是普通的枚举则返回true，否则返回false
 */
template <typename T>
constexpr bool is_normal_enum()
{
    return std::is_enum_v<T> && !is_reflectable<T>();
}

template <typename T>
constexpr mc::string_view get_type_name()
{
    if constexpr (detail::has_reflectable<T>::value) {
        return T::reflect_name;
    } else {
        return reflectable<T>::reflect_name;
    }
}

/*
 * 检查类型名称是否有效
 *
 * 由于类型名称由命名空间加类型名组成，以 . 或 :: 分隔，支持多级命名空间：
 * 1. 类型名称不能为空
 * 2. 类型名称按点号分隔，每段名称只能包含字母、数字、下划线，每段不能以数字开头
 * 3. 不能有连续的点号，也不能以点号开头或结尾
 * 例如：
 * - mc.devices.Sensor：表示 mc.devices 命名空间下的 Sensor 类型
 * - mc::devices::Sensor：表示 mc.devices 命名空间下的 Sensor 类型
 * - mc::devices.Sensor：表示 mc.devices 命名空间下的 Sensor 类型
 * - mc::devices.sensors.TemperatureSensor：表示 mc.devices.sensors 命名空间下的 TemperatureSensor 类型
 */
constexpr std::size_t max_type_name_length = 255;
constexpr inline bool is_valid_type_name(mc::string_view name)
{
    // 类型名称不能为空
    if (name.empty() || name.size() > max_type_name_length) {
        return false;
    }

    // 不能以分隔符开头或结尾
    if (name[0] == '.' || name[0] == ':' || name[name.size() - 1] == '.' || name[name.size() - 1] == ':') {
        return false;
    }

    bool segment_start = true; // 标记当前是否是段的开始
    for (size_t pos = 0; pos < name.size(); pos++) {
        char c = name[pos];

        if (c == '.') {
            // 不允许连续的分隔符
            if (segment_start) {
                return false;
            }
            segment_start = true;
        } else if (c == ':') {
            // 检查是否是 ::
            if (pos + 1 >= name.size() || name[pos + 1] != ':') {
                return false; // 单独的 : 不合法
            }
            // 不允许连续的分隔符
            if (segment_start) {
                return false;
            }
            segment_start = true;
            pos++; // 跳过第二个 :
        } else if (segment_start) {
            // 段的第一个字符必须是字母或下划线
            if (!mc::is_first_identifier_char(c)) {
                return false;
            }
            segment_start = false;
        } else {
            // 其他字符必须是合法的标识符字符
            if (!mc::is_identifier_char(c)) {
                return false;
            }
        }
    }

    return true;
}

inline mc::string_view longest_common_prefix(mc::string_view s1, mc::string_view s2)
{
    size_t i = 0;
    while (i < s1.size() && i < s2.size() && s1[i] == s2[i]) {
        ++i;
    }
    return s1.substr(0, i);
}

inline mc::string_view remove_common_namespace(mc::string_view s1, mc::string_view s2)
{
    auto prefix = longest_common_prefix(s1, s2);
    if (prefix.empty()) {
        return s1;
    }

    auto pos = prefix.find_last_of(':');
    if (pos == mc::string_view::npos) {
        return s1;
    }

    return s1.substr(pos + 1);
}

namespace detail {

// 类型命名空间映射特化
// 用户可以通过特化此模板来为任何类型（包括枚举）指定命名空间
template <typename T, typename = void>
struct reflect_namespace {
    using type = void; // 默认无命名空间，使用全局工厂
};

// 检查类型是否有全局命名空间映射
template <typename T, typename = void>
struct has_reflect_namespace : std::false_type {};

template <typename T>
struct has_reflect_namespace<T, std::enable_if_t<!std::is_same_v<typename reflect_namespace<T>::type, void>>>
    : std::true_type {};

/**
 * 检查类型是否在命名空间中，为了兼容C++宏自动填充的类型名，类型名可以用 :: 或 . 分隔，但命名空间只能用 . 分隔
 * @param type_name 类型名，格式为：mc.devices.TemperatureSensor 或 mc::devices::TemperatureSensor
 * @param namespace_name 命名空间名，格式为：mc.devices
 * @return 如果类型在命名空间中则返回true，否则返回false
 */
inline bool type_in_namespace(mc::string_view type_name, mc::string_view namespace_name)
{
    if (!is_valid_type_name(type_name) || !is_valid_type_name(namespace_name)) {
        return false;
    }

    std::size_t segment_count     = 0;
    auto        type_name_it      = mc::strings::split_iterator(type_name, "::.");
    auto        namespace_name_it = mc::strings::split_iterator(namespace_name, ".");
    while (!type_name_it.is_end() && !namespace_name_it.is_end()) {
        if (*type_name_it != *namespace_name_it) {
            // 如果第一段就不匹配，那整个类型会放到命名空间之下，这也是允许的
            // 但如果是部分匹配就不允许，说明类型名不包含命名空间
            return segment_count == 0 ? true : false;
        }

        ++segment_count;
        ++type_name_it;
        ++namespace_name_it;
    }

    // 命名空间名未遍历完，说明类型名不包含命名空间
    if (!namespace_name_it.is_end()) {
        return false;
    }

    // 类型名遍历完，说明类型名包含命名空间
    if (type_name_it.is_end()) {
        return false;
    }

    return true;
}

template <typename T>
inline bool check_type_namespace(mc::string_view type_name)
{
    if constexpr (has_reflect_namespace<T>::value) {
        using namespace_type = typename reflect_namespace<T>::type;
        return type_in_namespace(type_name, namespace_type::factory_name);
    } else {
        MC_UNUSED(type_name);
        return true;
    }
}
} // namespace detail

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
    virtual type_id_type get_type_id() const = 0;

    /**
     * @brief 获取属性值
     * @param name 属性名
     * @return variant 属性值
     */
    virtual variant get_property(mc::string_view name) const = 0;

    /**
     * @brief 设置属性值
     * @param name 属性名
     * @param value 属性值
     */
    virtual void set_property(mc::string_view name, const variant& value) = 0;

    /**
     * @brief 调用方法
     * @param name 方法名
     * @param args 参数列表
     * @return variant 返回值
     */
    virtual variant      invoke_method(mc::string_view name, const std::vector<variant>& args)       = 0;
    virtual async_result async_invoke_method(mc::string_view name, const std::vector<variant>& args) = 0;

    /**
     * @brief 获取所有属性名
     * @return std::vector<mc::string_view> 属性名列表
     */
    virtual std::vector<mc::string_view> get_property_names() const = 0;

    /**
     * @brief 获取所有方法名
     * @return std::vector<mc::string_view> 方法名列表
     */
    virtual std::vector<mc::string_view> get_method_names() const = 0;
};

/**
 * @brief 反射元数据基类
 *
 * 提供类型信息和对象创建功能
 */
class reflection_base : public mc::shared_base {
public:
    virtual ~reflection_base() = default;

    /**
     * @brief 创建反射对象
     * @return std::shared_ptr<reflected_object> 反射对象实例
     */
    virtual std::shared_ptr<reflected_object> create_object() = 0;

    /**
     * @brief 获取类型名
     * @return mc::string_view 类型名
     */
    virtual mc::string_view get_type_name() const = 0;

    /**
     * @brief 获取类型ID
     * @return type_id_type 类型ID
     */
    virtual type_id_type get_type_id() const = 0;

    /**
     * @brief 获取基类类型ID列表
     * @return std::vector<int> 基类类型ID列表
     */
    virtual std::vector<type_id_type> get_base_type_ids() const = 0;

    /**
     * @brief 检查是否是某个类型的子类
     * @param base_type_id 基类类型ID
     * @return bool 是否为子类
     */
    virtual bool is_derived_from(type_id_type base_type_id) const = 0;

    /**
     * @brief 获取属性信息
     * @param name 属性名
     * @return const property_type_info* 属性信息指针，需要转换为具体类型
     */
    virtual const property_type_info* get_property_info(mc::string_view name) const = 0;

    /**
     * @brief 获取方法信息
     * @param name 方法名
     * @return const method_type_info* 方法信息指针，需要转换为具体类型
     */
    virtual const method_type_info* get_method_info(mc::string_view name) const = 0;

    /**
     * @brief 获取基类信息
     * @param name 基类名
     * @return const base_class_type_info* 基类信息指针，需要转换为具体类型
     */
    virtual const base_class_type_info* get_base_class_info(mc::string_view name) const = 0;

    /**
     * @brief 获取自定义信息
     * @param name 自定义信息名
     * @param reflect_type 反射类型
     * @return const member_info_base* 自定义信息指针，需要转换为具体类型
     */
    virtual const member_info_base* get_custom_info(mc::string_view name, size_t reflect_type) const = 0;

    /**
     * @brief 获取所有属性名
     * @return std::vector<mc::string_view> 属性名列表
     */
    virtual std::vector<mc::string_view> get_property_names() const = 0;

    /**
     * @brief 获取所有方法名
     * @return std::vector<mc::string_view> 方法名列表
     */
    virtual std::vector<mc::string_view> get_method_names() const = 0;

    /**
     * @brief 获取是否为枚举类型
     * @return bool 是否为枚举类型
     */
    bool is_enum() const
    {
        return m_is_enum;
    }

    virtual uint64_t get_enum_value(mc::string_view name) const
    {
        MC_UNUSED(name);
        throw_not_enum_type(get_type_name());
    }
    virtual mc::string_view get_enum_name(uint64_t value) const
    {
        MC_UNUSED(value);
        throw_not_enum_type(get_type_name());
    }
    virtual std::vector<mc::string_view> get_enum_names() const
    {
        throw_not_enum_type(get_type_name());
    }
    virtual bool has_enum_value(mc::string_view name) const
    {
        MC_UNUSED(name);
        throw_not_enum_type(get_type_name());
    }
    virtual bool has_enum_value(uint64_t value) const
    {
        MC_UNUSED(value);
        throw_not_enum_type(get_type_name());
    }

protected:
    bool m_is_enum = false;
};

using reflected_object_ptr = std::shared_ptr<reflected_object>;

using reflection_metadata_ptr  = mc::shared_ptr<reflection_base>;
using reflection_metadata_wptr = mc::weak_ptr<reflection_base>;

namespace detail {

MC_API std::mutex& reflection_singleton_creation_mutex() noexcept;
using reflection_singleton_storage = std::atomic<reflection_metadata_ptr*>;

MC_API reflection_metadata_ptr& reflection_singleton_instance_with_creator(reflection_singleton_storage& storage,
                                                                           reflection_metadata_ptr* (*creator)());
MC_API reflection_metadata_ptr* reflection_singleton_try_get(reflection_singleton_storage& storage) noexcept;
MC_API void                     reflection_singleton_reset(reflection_singleton_storage& storage) noexcept;

template <typename Tag>
class reflection_singleton {
public:
    using reflection_ptr = reflection_metadata_ptr;
    using creator_t      = reflection_ptr* (*)();

    static reflection_ptr& instance_with_creator(creator_t creator)
    {
        return reflection_singleton_instance_with_creator(instance_ptr(), creator);
    }

    static reflection_ptr* try_get()
    {
        return reflection_singleton_try_get(instance_ptr());
    }

    static void reset_for_test()
    {
        reflection_singleton_reset(instance_ptr());
    }

private:
    static reflection_singleton_storage& instance_ptr()
    {
        static reflection_singleton_storage ptr{nullptr};
        return ptr;
    }
};

} // namespace detail

/**
 * @brief 定义类的反射信息
 */
#define MC_GLOBAL_REFLECTABLE(name, TYPE)                                                                              \
    template <>                                                                                                        \
    struct mc::reflect::reflectable<TYPE> {                                                                            \
        using is_defined = std::true_type;                                                                             \
        using is_enum    = std::conditional_t<std::is_enum_v<TYPE>, std::true_type, std::false_type>;                  \
        constexpr static mc::string_view reflect_name = mc::reflect::detail::reflect_name_from_literal(name);          \
    };

#define MC_CLASS_REFLECTABLE(name)                                                                                     \
    using is_reflectable = std::true_type;                                                                             \
    template <typename, typename>                                                                                      \
    friend struct mc::reflect::reflector;                                                                              \
    template <typename>                                                                                                \
    friend struct mc::reflect::static_metadata;                                                                        \
    constexpr static mc::string_view reflect_name = mc::reflect::detail::reflect_name_from_literal(name);

// 声明类型或者枚举是可反射的
// @param name 反射名称，用于在反射系统中标识类型或者枚举，支持多级命名空间，以 . 或 ::
// 分隔，例如：mc.devices.TemperatureSensor
// @param ... 枚举类型
// @note 为了使用方便，我们支持在类内部添加 MC_REFLECTABLE 或在全局命名空间 MC_REFLECTABLE 来标记该类支持反射。
// 例如：
// namespace mc::devices {
// struct TemperatureSensor {
//     MC_REFLECTABLE("mc.devices.TemperatureSensor"); // 在类声明内部声明反射
// };
// } // namespace mc::devices
// 或者
// namespace {
//     MC_REFLECTABLE("mc.devices.TemperatureSensor", mc::devices::TemperatureSensor); // 在全局命名空间声明反射
// }
// 优先建议使用内部声明的方式声明反射，但如过想对一些第三方类声明反射，则必须使用全局声明的方式。
// 另外对于枚举类型，也只能使用全局的方式声明反射。
// 我们通过识别 MC_REFLECTABLE 的参数个数来决定是全局反射还是类内反射，这样可以避免再增加一个宏名称，
// 如果 ... 可变参数大于一个参数，则表示是全局反射，否则表示是类内反射。
// 建议在反射系统中的名称保持与C++中的命名空间一致，避免混淆，为此反射系统中也支持 :: 分隔的命名空间。
#define MC_REFLECTABLE(...)                                                                                            \
    BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), MC_GLOBAL_REFLECTABLE,                      \
                 MC_CLASS_REFLECTABLE)                                                                                 \
    (__VA_ARGS__)

} // namespace mc::reflect

#endif // MC_REFLECT_BASE_H