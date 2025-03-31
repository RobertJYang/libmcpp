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
 * @tparam ObjectType 类类型
 * @tparam KeyType 键类型
 * @tparam Member 成员变量指针
 */
template <typename ObjectType, typename KeyType, KeyType ObjectType::* Member>
class member_key {
public:
    using object_type                     = ObjectType;
    using key_type                        = KeyType;
    static constexpr int  key_count       = 1;
    static constexpr bool is_compound_key = false;

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
     */
    void extract_key(mdb_key& key, const object_type& obj) const {
        key.append_value(obj.*Member);
    }

    /**
     * 将指定键值添加到键缓冲区
     * @param key 键缓冲区
     * @param value 键值
     */
    template <typename T>
    void append_key(mdb_key& key, const T& value) const {
        key.append_value(static_cast<key_type>(value));
    }

    // 特化字符数组到字符串的转换
    template <size_t N>
    void append_key(mdb_key& key, const char (&value)[N]) const {
        key.append_value(std::string_view(value));
    }
};

/**
 * 成员函数键提取器
 * @tparam ObjectType 类类型
 * @tparam KeyType 键类型
 * @tparam MemberFn 成员函数指针
 */
template <typename ObjectType, typename KeyType, KeyType (ObjectType::*MemberFn)() const>
class member_function_key {
public:
    using object_type                     = ObjectType;
    using key_type                        = KeyType;
    static constexpr int  key_count       = 1;
    static constexpr bool is_compound_key = false;

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
     */
    void extract_key(mdb_key& key, const object_type& obj) const {
        key.append_value((obj.*MemberFn)());
    }

    /**
     * 将指定键值添加到键缓冲区
     * @param key 键缓冲区
     * @param value 键值
     */
    template <typename T>
    void append_key(mdb_key& key, const T& value) const {
        key.append_value(static_cast<key_type>(value));
    }
};

/**
 * 函数对象键提取器
 * @tparam ObjectType 类类型
 * @tparam KeyType 键类型
 * @tparam Functor 函数对象类型
 */
template <typename ObjectType, typename KeyType, typename Functor>
class functor_key {
public:
    using object_type                     = ObjectType;
    using key_type                        = KeyType;
    static constexpr int  key_count       = 1;
    static constexpr bool is_compound_key = false;

    /**
     * 构造函数
     * @param f 函数对象
     */
    explicit functor_key(const Functor& f = Functor()) : m_functor(f) {
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
     */
    void extract_key(mdb_key& key, const object_type& obj) const {
        key.append_value(m_functor(obj));
    }

    /**
     * 将指定键值添加到键缓冲区
     * @param key 键缓冲区
     * @param value 键值
     */
    template <typename T>
    void append_key(mdb_key& key, const T& value) const {
        key.append_value(static_cast<key_type>(value));
    }

private:
    Functor m_functor;
};

/**
 * 复合键提取器
 * @tparam Extractors 键提取器类型列表
 */
template <typename... Extractors>
class composite_key {
public:
    // 从第一个提取器中获取 object_type
    using object_type = typename std::tuple_element_t<0, std::tuple<Extractors...>>::object_type;
    static constexpr int  key_count       = sizeof...(Extractors);
    static constexpr bool is_compound_key = true;

    /**
     * 默认构造函数
     */
    composite_key() = default;

    /**
     * 构造函数
     * @param extractors 键提取器实例
     */
    composite_key(Extractors... extractors) : m_extractors(std::move(extractors)...) {
    }

    /**
     * 提取组合键并添加到键缓冲区
     * @param key 键缓冲区
     * @param obj 对象
     */
    void extract_key(mdb_key& key, const object_type& obj) const {
        extract_key_impl(key, obj, std::index_sequence_for<Extractors...>());
    }

    /**
     * 接收键值并添加到键缓冲区
     * @param key 键缓冲区
     * @param keys 键值列表
     */
    template <typename... KeyTypes>
    void append_key(mdb_key& key, const KeyTypes&... keys) const {
        static_assert(sizeof...(KeyTypes) <= sizeof...(Extractors), "键的数量不能超过提取器数量");

        if constexpr (sizeof...(KeyTypes) == 1) {
            // 如果只有一个键，使用第一个提取器
            std::get<0>(m_extractors).append_key(key, std::get<0>(std::forward_as_tuple(keys...)));
        } else {
            // 使用多个键，数量可以小于提取器数量
            append_key_impl<0>(key, std::forward_as_tuple(keys...));
        }
    }

private:
    std::tuple<Extractors...> m_extractors;

    /**
     * 递归实现添加键值到键缓冲区
     * @param key 键缓冲区
     * @param keys 键值元组
     */
    template <size_t I, typename Tuple>
    void append_key_impl(mdb_key& key, const Tuple& keys) const {
        if constexpr (I < std::tuple_size_v<Tuple> && I < sizeof...(Extractors)) {
            std::get<I>(m_extractors).append_key(key, std::get<I>(keys));
            append_key_impl<I + 1>(key, keys);
        }
    }

    /**
     * 实现提取组合键并添加到键缓冲区
     * @param key 键缓冲区
     * @param obj 对象
     * @param indices 编译期索引序列
     */
    template <size_t... Indices>
    void extract_key_impl(mdb_key& key, const object_type& obj,
                          std::index_sequence<Indices...>) const {
        (std::get<Indices>(m_extractors).extract_key(key, obj), ...);
    }
};

/**
 * 键提取器特性萃取类，成员变量版本
 */
template <typename ObjectType, typename KeyType, KeyType ObjectType::* Member>
struct key_extractor_traits<member_key<ObjectType, KeyType, Member>> {
    using object_type    = ObjectType;
    using key_type       = KeyType;
    using extractor_type = member_key<ObjectType, KeyType, Member>;
    using tag            = tag_member;
};

/**
 * 键提取器特性萃取类，成员函数版本
 */
template <typename ObjectType, typename KeyType, KeyType (ObjectType::*MemberFn)() const>
struct key_extractor_traits<member_function_key<ObjectType, KeyType, MemberFn>> {
    using object_type    = ObjectType;
    using key_type       = KeyType;
    using extractor_type = member_function_key<ObjectType, KeyType, MemberFn>;
    using tag            = tag_member_function;
};

/**
 * 键提取器特性萃取类，函数对象版本
 */
template <typename ObjectType, typename KeyType, typename Functor>
struct key_extractor_traits<functor_key<ObjectType, KeyType, Functor>> {
    using object_type    = ObjectType;
    using key_type       = KeyType;
    using extractor_type = functor_key<ObjectType, KeyType, Functor>;
    using tag            = tag_functor;
};

/**
 * 键提取器特性萃取类，复合键版本
 */
template <typename... Extractors>
struct key_extractor_traits<composite_key<Extractors...>> {
    using object_type    = typename composite_key<Extractors...>::object_type;
    using extractor_type = composite_key<Extractors...>;
    using tag            = tag_composite;
};

/**
 * 推导 lambda 函数的返回类型
 */
template <typename ObjectType, typename Functor>
struct lambda_return_type {
    using type = decltype(std::declval<Functor>()(std::declval<const ObjectType&>()));
};

/**
 * 键提取器工厂函数，成员变量版本
 * @tparam ObjectType 类类型
 * @tparam KeyType 键类型
 * @tparam Member 成员变量指针
 * @return 键提取器
 */
template <typename ObjectType, typename KeyType, KeyType ObjectType::* Member>
member_key<ObjectType, KeyType, Member> make_key() {
    return member_key<ObjectType, KeyType, Member>();
}

/**
 * 键提取器工厂函数，成员函数版本
 * @tparam ObjectType 类类型
 * @tparam KeyType 键类型
 * @tparam MemberFn 成员函数指针
 * @return 键提取器
 */
template <typename ObjectType, typename KeyType, KeyType (ObjectType::*MemberFn)() const>
member_function_key<ObjectType, KeyType, MemberFn> make_key() {
    return member_function_key<ObjectType, KeyType, MemberFn>();
}

/**
 * 键提取器工厂函数，函数对象版本
 * @tparam ObjectType 类类型
 * @tparam Functor 函数对象类型
 * @param f 函数对象
 * @return 键提取器
 */
template <typename ObjectType, typename Functor>
auto make_key(const Functor& f = Functor()) {
    using key_type = typename lambda_return_type<ObjectType, Functor>::type;
    return functor_key<ObjectType, key_type, Functor>(f);
}

/**
 * 键提取器工厂函数，复合键版本
 * @tparam FirstExtractor 第一个键提取器类型
 * @tparam Extractors 剩余键提取器类型列表
 * @param first 第一个键提取器实例
 * @param extractors 剩余键提取器实例
 * @return 复合键提取器
 */
template <typename FirstExtractor, typename... Extractors>
auto make_key(const FirstExtractor& first, const Extractors&... extractors) {
    return composite_key<FirstExtractor, Extractors...>(first, extractors...);
}

/**
 * 对象ID键提取器
 * @tparam ObjectType 类类型
 */
template <typename ObjectType>
class object_id_key {
public:
    using object_type                     = ObjectType;
    using key_type                        = typename ObjectType::object_id_type;
    static constexpr int  key_count       = 1;
    static constexpr bool is_compound_key = false;

    /**
     * 提取键值
     * @param obj 对象
     * @return 键值
     */
    key_type operator()(const object_type& obj) const {
        return obj.get_object_id();
    }

    /**
     * 将键值添加到键缓冲区
     * @param key 键缓冲区
     * @param obj 对象
     */
    void extract_key(mdb_key& key, const object_type& obj) const {
        key.append_value(obj.get_object_id());
    }

    /**
     * 将指定键值添加到键缓冲区
     * @param key 键缓冲区
     * @param value 键值
     */
    template <typename T>
    void append_key(mdb_key& key, const T& value) const {
        key.append_value(static_cast<key_type>(value));
    }
};

} // namespace mc::database

#endif // MC_DATABASE_KEY_EXTRACTOR_H