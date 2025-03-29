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
 * @file common.h
 * @brief mc 命名空间中常用的宏、模板、函数等定义
 */
#ifndef MC_COMMON_H
#define MC_COMMON_H

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

/**
 * @brief 主命名空间
 */
namespace mc {

//------------------------------------------------------------------------------
// 常用常量的定义
//------------------------------------------------------------------------------
constexpr std::size_t MAX_NUM_ARRAY_ELEMENTS = 1024 * 1024;

//------------------------------------------------------------------------------
// 编译器和平台相关宏
//------------------------------------------------------------------------------

// 编译器检测
#if defined(__clang__)
#define MC_COMPILER_CLANG
#define MC_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
#define MC_COMPILER_GCC
#define MC_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
#define MC_COMPILER_MSVC
#define MC_COMPILER_VERSION _MSC_VER
#else
#define MC_COMPILER_UNKNOWN
#define MC_COMPILER_VERSION 0
#endif

// 平台检测
#if defined(_WIN32) || defined(_WIN64)
#define MC_PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define MC_PLATFORM_APPLE
#elif defined(__linux__)
#define MC_PLATFORM_LINUX
#else
#define MC_PLATFORM_UNKNOWN
#endif

// 架构检测
#if defined(__x86_64__) || defined(_M_X64)
#define MC_ARCH_X64
#elif defined(__i386) || defined(_M_IX86)
#define MC_ARCH_X86
#elif defined(__arm__) || defined(_M_ARM)
#define MC_ARCH_ARM
#elif defined(__aarch64__)
#define MC_ARCH_ARM64
#else
#define MC_ARCH_UNKNOWN
#endif

//------------------------------------------------------------------------------
// 条件分支预测优化宏
//------------------------------------------------------------------------------

/**
 * @brief 提示编译器条件很可能为真的分支预测优化宏
 */
#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#else
#define LIKELY(x) (x)
#endif

/**
 * @brief 提示编译器条件很可能为假的分支预测优化宏
 */
#if defined(__GNUC__) || defined(__clang__)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define UNLIKELY(x) (x)
#endif

//------------------------------------------------------------------------------
// 工具宏
//------------------------------------------------------------------------------

/**
 * @brief 未使用变量标记
 */
#define MC_UNUSED(var) (void)(var)

/**
 * @brief 判断两个浮点数是否相等
 */
#define MC_FLOAT_EQUAL(a, b, epsilon) (std::abs((a) - (b)) <= (epsilon))

/**
 * @brief 强制内联函数
 */
#if defined(MC_COMPILER_MSVC)
#define MC_ALWAYS_INLINE __forceinline
#else
#define MC_ALWAYS_INLINE inline __attribute__((always_inline))
#endif

/**
 * @brief 强制不内联函数
 */
#if defined(MC_COMPILER_MSVC)
#define MC_NEVER_INLINE __declspec(noinline)
#else
#define MC_NEVER_INLINE __attribute__((noinline))
#endif

/**
 * @brief 弃用警告
 */
#if defined(MC_COMPILER_MSVC)
#define MC_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#define MC_DEPRECATED(msg) __attribute__((deprecated(msg)))
#endif

//------------------------------------------------------------------------------
// 拷贝和移动控制类
//------------------------------------------------------------------------------

/**
 * @brief 禁止拷贝的基类
 *
 * 任何希望禁止拷贝的类可以私有继承此类
 * 例如: class MyClass : private noncopyable {};
 */
class noncopyable {
protected:
    // 允许构造和析构
    constexpr noncopyable() = default;
    ~noncopyable()          = default;

    // 禁止拷贝
    noncopyable(const noncopyable&)            = delete;
    noncopyable& operator=(const noncopyable&) = delete;
};

/**
 * @brief 禁止移动的基类
 *
 * 任何希望禁止移动的类可以私有继承此类
 * 例如: class MyClass : private nonmovable {};
 */
class nonmovable {
protected:
    // 允许构造和析构
    constexpr nonmovable() = default;
    ~nonmovable()          = default;

    // 禁止移动
    nonmovable(nonmovable&&)            = delete;
    nonmovable& operator=(nonmovable&&) = delete;
};

/**
 * @brief 禁止拷贝和移动的基类
 *
 * 任何希望同时禁止拷贝和移动的类可以私有继承此类
 * 例如: class MyClass : private noncopyable_nonmovable {};
 */
class noncopyable_nonmovable : private noncopyable, private nonmovable {
protected:
    constexpr noncopyable_nonmovable() = default;
    ~noncopyable_nonmovable()          = default;
};

//------------------------------------------------------------------------------
// 类型特性和模板
//------------------------------------------------------------------------------

/**
 * @brief 检查类型是否具有特定成员函数的工具类
 * @note 用法：
 *   has_member_function<T, void(int)>::value 检查 T 是否有 void func(int) 成员函数
 */
template <typename, typename T>
struct has_member_function;

/**
 * @brief 类型安全的枚举类辅助工具
 */
template <typename Enum>
struct enable_enum_flags {
    static constexpr bool enable = false;
};

/**
 * @brief 为具有 enable_enum_flags 特化的枚举类重载位运算符
 */
template <typename Enum>
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum>::type operator|(Enum lhs, Enum rhs) {
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template <typename Enum>
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum>::type operator&(Enum lhs, Enum rhs) {
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template <typename Enum>
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum>::type operator^(Enum lhs, Enum rhs) {
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
}

template <typename Enum>
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum>::type operator~(Enum rhs) {
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(~static_cast<underlying>(rhs));
}

template <typename Enum>
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum&>::type operator|=(Enum& lhs,
                                                                                 Enum  rhs) {
    lhs = lhs | rhs;
    return lhs;
}

template <typename Enum>
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum&>::type operator&=(Enum& lhs,
                                                                                 Enum  rhs) {
    lhs = lhs & rhs;
    return lhs;
}

template <typename Enum>
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum&>::type operator^=(Enum& lhs,
                                                                                 Enum  rhs) {
    lhs = lhs ^ rhs;
    return lhs;
}

/**
 * @brief 定义枚举类的位运算
 * @note 用法：
 *   enum class MyFlags { Flag1 = 1, Flag2 = 2, Flag3 = 4 };
 *   MC_ENABLE_ENUM_FLAGS(MyFlags)
 *   MyFlags flags = MyFlags::Flag1 | MyFlags::Flag2;
 */
#define MC_ENABLE_ENUM_FLAGS(Enum)                                                                 \
    template <>                                                                                    \
    struct mc::enable_enum_flags<Enum> {                                                           \
        static constexpr bool enable = true;                                                       \
    }

/**
 * @brief 安全的数值转换
 */
template <typename To, typename From>
typename std::enable_if<std::is_arithmetic<From>::value && std::is_arithmetic<To>::value, To>::type
safe_cast(From value) {
    if (std::is_signed<From>::value && std::is_unsigned<To>::value) {
        if (value < 0) {
            throw std::out_of_range("Cannot convert negative value to unsigned type");
        }
    }

    if ((std::is_integral<From>::value && std::is_integral<To>::value) &&
        (sizeof(From) > sizeof(To) ||
         (std::is_signed<From>::value && std::is_unsigned<To>::value))) {
        if (value > static_cast<From>(std::numeric_limits<To>::max())) {
            throw std::out_of_range("Value too large for target type");
        }
        if (std::is_signed<From>::value &&
            value < static_cast<From>(std::numeric_limits<To>::min())) {
            throw std::out_of_range("Value too small for target type");
        }
    }

    return static_cast<To>(value);
}

/**
 * @brief 获取类型的字符串表示
 */
template <typename T>
std::string type_name() {
#ifdef MC_COMPILER_CLANG
    const char* name  = __PRETTY_FUNCTION__;
    const char* start = std::strstr(name, "T = ") + 4;
    const char* end   = std::strstr(start, "]");
    return std::string(start, end - start);
#elif defined(MC_COMPILER_GCC)
    const char* name  = __PRETTY_FUNCTION__;
    const char* start = std::strstr(name, "T = ") + 4;
    const char* end   = std::strstr(start, ";");
    return std::string(start, end - start);
#elif defined(MC_COMPILER_MSVC)
    const char* name  = __FUNCSIG__;
    const char* start = std::strstr(name, "type_name<") + 10;
    const char* end   = std::strstr(start, ">(void)");
    return std::string(start, end - start);
#else
    return "unknown_type";
#endif
}

//------------------------------------------------------------------------------
// 实用工具函数
//------------------------------------------------------------------------------

/**
 * @brief 范围约束函数
 */
template <typename T>
T clamp(const T& value, const T& min, const T& max) {
    return value < min ? min : (value > max ? max : value);
}

/**
 * @brief 安全地删除指针
 */
template <typename T>
void safe_delete(T*& ptr) {
    if (ptr) {
        delete ptr;
        ptr = nullptr;
    }
}

/**
 * @brief 安全地删除数组指针
 */
template <typename T>
void safe_delete_array(T*& ptr) {
    if (ptr) {
        delete[] ptr;
        ptr = nullptr;
    }
}

/**
 * @brief 生成随机整数
 */
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type random(T min, T max) {
    static std::random_device        rd;
    static std::mt19937              gen(rd());
    std::uniform_int_distribution<T> dist(min, max);
    return dist(gen);
}

/**
 * @brief 生成随机浮点数
 */
template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type random(T min, T max) {
    static std::random_device         rd;
    static std::mt19937               gen(rd());
    std::uniform_real_distribution<T> dist(min, max);
    return dist(gen);
}

/**
 * @brief 创建基于作用域的计时器
 * @note 用法：
 *   {
 *     auto timer = mc::scope_timer("Operation");
 *     // 要测量的代码
 *   } // 离开作用域时自动打印耗时
 */
class scope_timer {
public:
    explicit scope_timer(const std::string& name)
        : m_name(name), m_start(std::chrono::high_resolution_clock::now()) {
    }

    ~scope_timer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start).count();
        std::cout << m_name << " 耗时: " << duration << "ms" << std::endl;
    }

private:
    std::string                                                 m_name;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

//------------------------------------------------------------------------------
// 字节序转换函数
//------------------------------------------------------------------------------

/**
 * @brief 判断系统是否为小端序
 */
inline bool is_little_endian() {
    static const uint16_t test = 0x0102;
    return *reinterpret_cast<const uint8_t*>(&test) == 0x02;
}

/**
 * @brief 16位整数字节序转换
 */
inline uint16_t swap_bytes(uint16_t value) {
    return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
}

/**
 * @brief 32位整数字节序转换
 */
inline uint32_t swap_bytes(uint32_t value) {
    return ((value & 0xFF000000) >> 24) | ((value & 0x00FF0000) >> 8) |
           ((value & 0x0000FF00) << 8) | ((value & 0x000000FF) << 24);
}

/**
 * @brief 64位整数字节序转换
 */
inline uint64_t swap_bytes(uint64_t value) {
    return ((value & 0xFF00000000000000ULL) >> 56) | ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0x0000FF0000000000ULL) >> 24) | ((value & 0x000000FF00000000ULL) >> 8) |
           ((value & 0x00000000FF000000ULL) << 8) | ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x000000000000FF00ULL) << 40) | ((value & 0x00000000000000FFULL) << 56);
}

/**
 * @brief 浮点数字节序转换
 */
inline float swap_bytes(float value) {
    uint32_t temp = swap_bytes(*reinterpret_cast<uint32_t*>(&value));
    return *reinterpret_cast<float*>(&temp);
}

/**
 * @brief 双精度浮点数字节序转换
 */
inline double swap_bytes(double value) {
    uint64_t temp = swap_bytes(*reinterpret_cast<uint64_t*>(&value));
    return *reinterpret_cast<double*>(&temp);
}

/**
 * @brief 网络字节序转主机字节序（16位）
 */
inline uint16_t ntoh(uint16_t value) {
    return is_little_endian() ? swap_bytes(value) : value;
}

/**
 * @brief 网络字节序转主机字节序（32位）
 */
inline uint32_t ntoh(uint32_t value) {
    return is_little_endian() ? swap_bytes(value) : value;
}

/**
 * @brief 网络字节序转主机字节序（64位）
 */
inline uint64_t ntoh(uint64_t value) {
    return is_little_endian() ? swap_bytes(value) : value;
}

/**
 * @brief 主机字节序转网络字节序（16位）
 */
inline uint16_t hton(uint16_t value) {
    return is_little_endian() ? swap_bytes(value) : value;
}

/**
 * @brief 主机字节序转网络字节序（32位）
 */
inline uint32_t hton(uint32_t value) {
    return is_little_endian() ? swap_bytes(value) : value;
}

/**
 * @brief 主机字节序转网络字节序（64位）
 */
inline uint64_t hton(uint64_t value) {
    return is_little_endian() ? swap_bytes(value) : value;
}

} // namespace mc

#endif // MC_COMMON_H