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

/**
 * @file typename.h
 * @brief 类型名称相关的定义
 */
#ifndef MC_REFLECT_TYPENAME_H
#define MC_REFLECT_TYPENAME_H

#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <list>
#include <deque>
#include <array>
#include <optional>
#include <variant>
#include <memory>

namespace mc {
namespace reflect {

/**
 * @brief 获取类型名称的模板类
 * 
 * @tparam T 要获取名称的类型
 */
template<typename T>
struct get_typename {
    static const char* name() {
        return T::class_name();
    }
};

// 基本类型特化
template<> struct get_typename<void> { static const char* name() { return "void"; } };
template<> struct get_typename<bool> { static const char* name() { return "bool"; } };
template<> struct get_typename<long> { static const char* name() { return "long"; } };
template<> struct get_typename<unsigned long> { static const char* name() { return "unsigned long"; } };
template<> struct get_typename<float> { static const char* name() { return "float"; } };
template<> struct get_typename<double> { static const char* name() { return "double"; } };
template<> struct get_typename<long double> { static const char* name() { return "long double"; } };
template<> struct get_typename<std::string> { static const char* name() { return "std::string"; } };
template<> struct get_typename<std::string_view> { static const char* name() { return "std::string_view"; } };

// 固定大小整数类型特化
template<> struct get_typename<int8_t> { static const char* name() { return "int8_t"; } };
template<> struct get_typename<uint8_t> { static const char* name() { return "uint8_t"; } };
template<> struct get_typename<int16_t> { static const char* name() { return "int16_t"; } };
template<> struct get_typename<uint16_t> { static const char* name() { return "uint16_t"; } };
template<> struct get_typename<int32_t> { static const char* name() { return "int32_t"; } };
template<> struct get_typename<uint32_t> { static const char* name() { return "uint32_t"; } };
template<> struct get_typename<int64_t> { static const char* name() { return "int64_t"; } };
template<> struct get_typename<uint64_t> { static const char* name() { return "uint64_t"; } };

// 容器类型特化
template<typename T, typename A>
struct get_typename<std::vector<T, A>> {
    static const char* name() {
        static std::string s = std::string("std::vector<") + get_typename<T>::name() + ">";
        return s.c_str();
    }
};

template<typename K, typename V, typename C, typename A>
struct get_typename<std::map<K, V, C, A>> {
    static const char* name() {
        static std::string s = std::string("std::map<") + get_typename<K>::name() + "," + get_typename<V>::name() + ">";
        return s.c_str();
    }
};

template<typename K, typename V, typename H, typename E, typename A>
struct get_typename<std::unordered_map<K, V, H, E, A>> {
    static const char* name() {
        static std::string s = std::string("std::unordered_map<") + get_typename<K>::name() + "," + get_typename<V>::name() + ">";
        return s.c_str();
    }
};

template<typename T, typename C, typename A>
struct get_typename<std::set<T, C, A>> {
    static const char* name() {
        static std::string s = std::string("std::set<") + get_typename<T>::name() + ">";
        return s.c_str();
    }
};

template<typename T, typename H, typename E, typename A>
struct get_typename<std::unordered_set<T, H, E, A>> {
    static const char* name() {
        static std::string s = std::string("std::unordered_set<") + get_typename<T>::name() + ">";
        return s.c_str();
    }
};

template<typename T, typename A>
struct get_typename<std::list<T, A>> {
    static const char* name() {
        static std::string s = std::string("std::list<") + get_typename<T>::name() + ">";
        return s.c_str();
    }
};

template<typename T, typename A>
struct get_typename<std::deque<T, A>> {
    static const char* name() {
        static std::string s = std::string("std::deque<") + get_typename<T>::name() + ">";
        return s.c_str();
    }
};

template<typename T, std::size_t N>
struct get_typename<std::array<T, N>> {
    static const char* name() {
        static std::string s = std::string("std::array<") + get_typename<T>::name() + "," + std::to_string(N) + ">";
        return s.c_str();
    }
};

// 智能指针特化
template<typename T>
struct get_typename<std::unique_ptr<T>> {
    static const char* name() {
        static std::string s = std::string("std::unique_ptr<") + get_typename<T>::name() + ">";
        return s.c_str();
    }
};

template<typename T>
struct get_typename<std::shared_ptr<T>> {
    static const char* name() {
        static std::string s = std::string("std::shared_ptr<") + get_typename<T>::name() + ">";
        return s.c_str();
    }
};

template<typename T>
struct get_typename<std::weak_ptr<T>> {
    static const char* name() {
        static std::string s = std::string("std::weak_ptr<") + get_typename<T>::name() + ">";
        return s.c_str();
    }
};

// 其他常用类型特化
template<typename T>
struct get_typename<std::optional<T>> {
    static const char* name() {
        static std::string s = std::string("std::optional<") + get_typename<T>::name() + ">";
        return s.c_str();
    }
};

// 指针特化
template<typename T>
struct get_typename<T*> {
    static const char* name() {
        static std::string s = std::string(get_typename<T>::name()) + "*";
        return s.c_str();
    }
};

// 引用特化
template<typename T>
struct get_typename<T&> {
    static const char* name() {
        static std::string s = std::string(get_typename<T>::name()) + "&";
        return s.c_str();
    }
};

// const 特化
template<typename T>
struct get_typename<const T> {
    static const char* name() {
        static std::string s = std::string("const ") + get_typename<T>::name();
        return s.c_str();
    }
};

// 数组特化
template<typename T, std::size_t N>
struct get_typename<T[N]> {
    static const char* name() {
        static std::string s = std::string(get_typename<T>::name()) + "[" + std::to_string(N) + "]";
        return s.c_str();
    }
};

} // namespace reflect
} // namespace mc

#endif // MC_REFLECT_TYPENAME_H 