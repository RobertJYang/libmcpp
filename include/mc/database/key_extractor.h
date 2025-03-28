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

#ifndef MC_DATABASE_KEY_EXTRACTOR_H
#define MC_DATABASE_KEY_EXTRACTOR_H

#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <mc/database/key.h>

namespace mc::database {

/**
 * 标签类型，用于SFINAE
 */
struct tag_member {};
struct tag_member_function {};
struct tag_functor {};
struct tag_composite {};
/**
 * 键提取器特性萃取类基类
 */
template <typename Extractor>
struct key_extractor_traits;

/**
 * 成员变量键提取器
 * @tparam Class 类类型
 * @tparam KeyType 键类型
 * @tparam Member 成员变量指针
 */
template <typename Class, typename KeyType, KeyType Class::* Member>
class member_key {
public:
    using object_type = Class;
    using key_type    = KeyType;
    using tag         = tag_member;

    /**
     * 提取键值
     * @param obj 对象
     * @return 键值
     */
    key_type operator()(const object_type& obj) const {
        return obj.*Member;
    }

    /**
     * 将键值添加到键缓冲区
     * @param key 键缓冲区
     * @param obj 对象
     * @return 成功返回true，失败返回false
     */
    void extract_key(mdb_key& key, const object_type& obj) const {
        key.append_value(obj.*Member);
    }

    void append_key(mdb_key& key, const key_type& value) const {
        key.append_value(value);
    }
};

/**
 * 成员函数键提取器
 * @tparam Class 类类型
 * @tparam KeyType 键类型
 * @tparam MemberFn 成员函数指针
 */
template <typename Class, typename KeyType, KeyType (Class::*MemberFn)() const>
class member_function_key {
public:
    using object_type = Class;
    using key_type    = KeyType;
    using tag         = tag_member_function;

    /**
     * 提取键值
     * @param obj 对象
     * @return 键值
     */
    key_type operator()(const object_type& obj) const {
        return (obj.*MemberFn)();
    }

    /**
     * 将键值添加到键缓冲区
     * @param key 键缓冲区
     * @param obj 对象
     * @return 成功返回true，失败返回false
     */
    void extract_key(mdb_key& key, const object_type& obj) const {
        key.append_value((obj.*MemberFn)());
    }

    void append_key(mdb_key& key, const key_type& value) const {
        key.append_value(value);
    }
};

/**
 * 函数对象键提取器
 * @tparam Class 类类型
 * @tparam Functor 函数对象类型
 */
template <typename Class, typename Functor>
class functor_key {
public:
    using object_type  = Class;
    using functor_type = Functor;
    using key_type     = decltype(std::declval<functor_type>()(std::declval<const object_type&>()));
    using tag          = tag_functor;

    /**
     * 构造函数
     * @param f 函数对象
     */
    explicit functor_key(const functor_type& f = functor_type()) : m_functor(f) {
    }

    /**
     * 提取键值
     * @param obj 对象
     * @return 键值
     */
    key_type operator()(const object_type& obj) const {
        return m_functor(obj);
    }

    /**
     * 将键值添加到键缓冲区
     * @param key 键缓冲区
     * @param obj 对象
     * @return 成功返回true，失败返回false
     */
    void extract_key(mdb_key& key, const object_type& obj) const {
        key.append_value(m_functor(obj));
    }

    void append_key(mdb_key& key, const key_type& value) const {
        key.append_value(value);
    }

private:
    functor_type m_functor;
};

/**
 * 复合键提取器
 * @tparam Extractors 键提取器类型列表
 */
template <typename... Extractors>
class composite_key {
public:
    // 获取第一个提取器的对象类型
    using object_type =
        typename std::tuple_element<0, std::tuple<Extractors...>>::type::object_type;
    using key_type = typename std::tuple_element<0, std::tuple<Extractors...>>::type::key_type;
    using tag      = tag_composite;

    /**
     * 构造函数
     * @param extractors 键提取器实例
     */
    explicit composite_key(const Extractors&... extractors) : m_extractors(extractors...) {
    }

    /**
     * 提取组合键并添加到键缓冲区
     * @param key 键缓冲区
     * @param obj 对象
     */
    void extract_key(mdb_key& key, const object_type& obj) const {
        extract_key_impl(key, obj, std::index_sequence_for<Extractors...>());
    }

    template <typename... KeyTypes>
    void append_key(mdb_key& key, const KeyTypes&... keys) const {
        extract_keys_impl(key, std::index_sequence_for<KeyTypes...>(), keys...);
    }

private:
    std::tuple<Extractors...> m_extractors;

    /**
     * 实现提取组合键并添加到键缓冲区
     * @param key 键缓冲区
     * @param obj 对象
     * @param indices 编译期索引序列
     * @return 成功返回true，失败返回false
     */
    template <size_t... Indices>
    void extract_key_impl(mdb_key& key, const object_type& obj,
                          std::index_sequence<Indices...>) const {
        (std::get<Indices>(m_extractors).extract_key(key, obj), ...);
    }

    template <typename... KeyTypes, size_t... Indices>
    void extract_keys_impl(mdb_key& key, std::index_sequence<Indices...>,
                           const KeyTypes&... keys) const {
        (std::get<Indices>(m_extractors).append_key(key, keys), ...);
    }
};

/**
 * 键提取器特性萃取类，成员变量版本
 */
template <typename Class, typename KeyType, KeyType Class::* Member>
struct key_extractor_traits<member_key<Class, KeyType, Member>> {
    using object_type    = Class;
    using key_type       = KeyType;
    using extractor_type = member_key<Class, KeyType, Member>;
    using tag            = tag_member;
};

/**
 * 键提取器特性萃取类，成员函数版本
 */
template <typename Class, typename KeyType, KeyType (Class::*MemberFn)() const>
struct key_extractor_traits<member_function_key<Class, KeyType, MemberFn>> {
    using object_type    = Class;
    using key_type       = KeyType;
    using extractor_type = member_function_key<Class, KeyType, MemberFn>;
    using tag            = tag_member_function;
};

/**
 * 键提取器特性萃取类，函数对象版本
 */
template <typename Class, typename Functor>
struct key_extractor_traits<functor_key<Class, Functor>> {
    using object_type  = Class;
    using functor_type = Functor;
    using key_type     = decltype(std::declval<functor_type>()(std::declval<const object_type&>()));
    using extractor_type = functor_key<Class, Functor>;
    using tag            = tag_functor;
};

/**
 * 键提取器特性萃取类，复合键版本
 */
template <typename... Extractors>
struct key_extractor_traits<composite_key<Extractors...>> {
    using object_type =
        typename std::tuple_element<0, std::tuple<Extractors...>>::type::object_type;
    using extractors_tuple = std::tuple<Extractors...>;
    using extractor_type   = composite_key<Extractors...>;
};

/**
 * 键提取器工厂函数，成员变量版本
 * @tparam Class 类类型
 * @tparam KeyType 键类型
 * @tparam Member 成员变量指针
 * @return 键提取器
 */
template <typename Class, typename KeyType, KeyType Class::* Member>
member_key<Class, KeyType, Member> make_key() {
    return member_key<Class, KeyType, Member>();
}

/**
 * 键提取器工厂函数，成员函数版本
 * @tparam Class 类类型
 * @tparam KeyType 键类型
 * @tparam MemberFn 成员函数指针
 * @return 键提取器
 */
template <typename Class, typename KeyType, KeyType (Class::*MemberFn)() const>
member_function_key<Class, KeyType, MemberFn> make_key() {
    return member_function_key<Class, KeyType, MemberFn>();
}

/**
 * 键提取器工厂函数，函数对象版本
 * @tparam Class 类类型
 * @tparam Functor 函数对象类型
 * @param f 函数对象
 * @return 键提取器
 */
template <typename Class, typename Functor>
functor_key<Class, Functor> make_key(const Functor& f = Functor()) {
    return functor_key<Class, Functor>(f);
}

/**
 * 键提取器工厂函数，复合键版本
 * @tparam Extractors 键提取器类型列表
 * @param extractors 键提取器实例
 * @return 复合键提取器
 */
template <typename... Extractors>
composite_key<Extractors...> make_key(const Extractors&... extractors) {
    return composite_key<Extractors...>(extractors...);
}

} // namespace mc::database

#endif // MC_DATABASE_KEY_EXTRACTOR_H