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
#include <cstddef>
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
#include "securec.h"

/**
 * @brief 主命名空间
 */
namespace mc {

//------------------------------------------------------------------------------
// 常用常量的定义
//------------------------------------------------------------------------------
constexpr std::size_t MAX_NUM_ARRAY_ELEMENTS = 1024 * 1024;

#ifndef MC_FLOAT_PRECISION
#define MC_FLOAT_PRECISION 6
#endif

#ifndef MC_FLOAT_EPSILON
#define MC_FLOAT_EPSILON 1e-6
#endif

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
#define MC_LIKELY(x) __builtin_expect(!!(x), 1)
#else
#define MC_LIKELY(x) (x)
#endif

/**
 * @brief 提示编译器条件很可能为假的分支预测优化宏
 */
#if defined(__GNUC__) || defined(__clang__)
#define MC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define MC_UNLIKELY(x) (x)
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

/**
 * @brief 导出符号
 */
#if defined(MC_COMPILER_GCC) || defined(MC_COMPILER_CLANG)
#define MC_API __attribute__((visibility("default")))
#else
#define MC_API
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
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum>::type operator|(Enum lhs, Enum rhs)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template <typename Enum>
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum>::type operator&(Enum lhs, Enum rhs)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template <typename Enum>
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum>::type operator^(Enum lhs, Enum rhs)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
}

template <typename Enum>
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum>::type operator~(Enum rhs)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(~static_cast<underlying>(rhs));
}

template <typename Enum>
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum&>::type operator|=(Enum& lhs, Enum rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

template <typename Enum>
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum&>::type operator&=(Enum& lhs, Enum rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

template <typename Enum>
typename std::enable_if<enable_enum_flags<Enum>::enable, Enum&>::type operator^=(Enum& lhs, Enum rhs)
{
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
#define MC_ENABLE_ENUM_FLAGS(Enum)                                                                                     \
    template <>                                                                                                        \
    struct mc::enable_enum_flags<Enum> {                                                                               \
        static constexpr bool enable = true;                                                                           \
    }

//------------------------------------------------------------------------------
// 实用工具函数
//------------------------------------------------------------------------------

/**
 * @brief 生成随机整数
 */
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type random(T min, T max)
{
    static std::random_device        rd;
    static std::mt19937              gen(rd());
    std::uniform_int_distribution<T> dist(min, max);
    return dist(gen);
}

/**
 * @brief 生成随机浮点数
 */
template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type random(T min, T max)
{
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
    explicit scope_timer(const std::string& name) : m_name(name), m_start(std::chrono::high_resolution_clock::now())
    {}

    ~scope_timer()
    {
        auto end      = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start).count();

        MC_UNUSED(end);
        MC_UNUSED(duration);
    }

private:
    std::string                                                 m_name;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

//------------------------------------------------------------------------------
// 字节序转换函数
//------------------------------------------------------------------------------

#ifndef MC_IS_LITTLE_ENDIAN
#define MC_IS_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#endif

/**
 * @brief 判断系统是否为小端序
 */
inline bool is_little_endian() noexcept
{
    constexpr uint16_t test = 0x0102;
    return *reinterpret_cast<const uint8_t*>(&test) == 0x02;
}

template <typename T>
typename std::enable_if_t<std::is_unsigned_v<T>, T> swap_bytes(T value)
{
    if constexpr (sizeof(T) == 1) {
        return value;
    } else if constexpr (sizeof(T) == 2) {
        return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
    } else if constexpr (sizeof(T) == 4) {
        return ((value & 0xFF000000) >> 24) | ((value & 0x00FF0000) >> 8) | ((value & 0x0000FF00) << 8) |
               ((value & 0x000000FF) << 24);
    } else if constexpr (sizeof(T) == 8) {
        return ((value & 0xFF00000000000000ULL) >> 56) | ((value & 0x00FF000000000000ULL) >> 40) |
               ((value & 0x0000FF0000000000ULL) >> 24) | ((value & 0x000000FF00000000ULL) >> 8) |
               ((value & 0x00000000FF000000ULL) << 8) | ((value & 0x0000000000FF0000ULL) << 24) |
               ((value & 0x000000000000FF00ULL) << 40) | ((value & 0x00000000000000FFULL) << 56);
    } else {
        static_assert(sizeof(std::size_t) == 1 || sizeof(std::size_t) == 2 || sizeof(std::size_t) == 4 ||
                          sizeof(std::size_t) == 8,
                      "Unsupported size_t type");
    }
}

/**
 * @brief 有符号整数字节序转换
 */
template <typename T>
typename std::enable_if_t<std::is_signed_v<T>, T> swap_bytes(T value)
{
    using unsigned_t = typename std::make_unsigned<T>::type;
    unsigned_t temp  = static_cast<unsigned_t>(value);
    temp             = swap_bytes(temp);
    return static_cast<T>(temp);
}

/**
 * @brief 位转换辅助函数
 */
template <typename To, typename From>
inline To bit_cast(const From& src) noexcept
{
    static_assert(sizeof(To) == sizeof(From), "bit_cast requires same size types");
    To dst;
    (void)memcpy_s(&dst, sizeof(To), &src, sizeof(From));
    return dst;
}

/**
 * @brief 浮点数字节序转换
 */
inline float swap_bytes(float value)
{
    return bit_cast<float>(swap_bytes(bit_cast<uint32_t>(value)));
}

/**
 * @brief 双精度浮点数字节序转换
 */
inline double swap_bytes(double value)
{
    return bit_cast<double>(swap_bytes(bit_cast<uint64_t>(value)));
}

/**
 * @brief 网络字节序转主机字节序（16位）
 */
inline uint16_t ntoh(uint16_t value)
{
    return is_little_endian() ? swap_bytes(value) : value;
}

inline int16_t ntoh(int16_t value)
{
    return static_cast<int16_t>(ntoh(static_cast<uint16_t>(value)));
}

/**
 * @brief 网络字节序转主机字节序（32位）
 */
inline uint32_t ntoh(uint32_t value)
{
    return is_little_endian() ? swap_bytes(value) : value;
}

inline int32_t ntoh(int32_t value)
{
    return static_cast<int32_t>(ntoh(static_cast<uint32_t>(value)));
}

/**
 * @brief 网络字节序转主机字节序（64位）
 */
inline uint64_t ntoh(uint64_t value)
{
    return is_little_endian() ? swap_bytes(value) : value;
}

inline int64_t ntoh(int64_t value)
{
    return static_cast<int64_t>(ntoh(static_cast<uint64_t>(value)));
}

/**
 * @brief 主机字节序转网络字节序（16位）
 */
inline uint16_t hton(uint16_t value)
{
    return is_little_endian() ? swap_bytes(value) : value;
}

inline int16_t hton(int16_t value)
{
    return static_cast<int16_t>(hton(static_cast<uint16_t>(value)));
}

/**
 * @brief 主机字节序转网络字节序（32位）
 */
inline uint32_t hton(uint32_t value)
{
    return is_little_endian() ? swap_bytes(value) : value;
}

inline int32_t hton(int32_t value)
{
    return static_cast<int32_t>(hton(static_cast<uint32_t>(value)));
}

/**
 * @brief 主机字节序转网络字节序（64位）
 */
inline uint64_t hton(uint64_t value)
{
    return is_little_endian() ? swap_bytes(value) : value;
}

inline int64_t hton(int64_t value)
{
    return static_cast<int64_t>(hton(static_cast<uint64_t>(value)));
}

/**
 * @brief 计算结构体成员相对于结构体起始位置的偏移量
 *
 * @param TYPE 结构体类型
 * @param MEMBER 成员名称
 * @return size_t 成员偏移量（字节数）
 */
#define MC_OFFSETOF(TYPE, MEMBER) (static_cast<size_t>(offsetof(TYPE, MEMBER)))

/**
 * @brief 计算成员指针对应的偏移量
 *
 * @param TYPE 结构体类型
 * @param PTR 成员指针
 * @param OFFSET 偏移量
 * @return TYPE* 成员指针
 */
#define MC_MEMBER_PTR(TYPE, PTR, OFFSET) (reinterpret_cast<TYPE>(reinterpret_cast<std::uintptr_t>(PTR) + OFFSET))

/**
 * @brief 计算两个指针之间的偏移量
 *
 * @param P1 指针1
 * @param P2 指针2
 * @return intptr_t 偏移量（字节数）
 */
#define MC_PTR_OFFSET(P1, P2) (reinterpret_cast<intptr_t>(P1) - reinterpret_cast<intptr_t>(P2))

/**
 * @brief 计算成员指针对应的偏移量
 *
 * 注意：该宏使用了特殊技巧，在某些编译器可能存在兼容性问题
 *
 * @param MEMBER 成员指针
 * @return size_t 成员偏移量（字节数）
 */
#define MC_MEMBER_OFFSETOF(TYPE, MEMBER) (reinterpret_cast<std::size_t>(&(reinterpret_cast<TYPE*>(0)->*MEMBER)))

#define MC_ALIGN(value, alignment)    (value & ~(alignment - 1))
#define MC_ALIGN_UP(value, alignment) ((value + alignment - 1) & ~(alignment - 1))

using thread_id = uint32_t;
thread_id get_thread_id();

namespace detail {
template <typename T>
struct is_function_pointer : std::false_type {};

template <typename R, typename... Args>
struct is_function_pointer<R (*)(Args...)> : std::true_type {};
} // namespace detail

template <typename F>
inline auto get_function_offset(F function)
    -> std::enable_if_t<std::is_member_function_pointer_v<F> || detail::is_function_pointer<F>::value, std::uintptr_t>
{
    union {
        F              function;
        std::uintptr_t offset;
    } u;
    u.offset   = 0;
    u.function = function;
    return u.offset;
}

/*
 * 检查字符是否是合法的 identifier 字符
 *
 * 允许的字符包括：
 * 1. 字母（a-z, A-Z）
 * 2. 数字（0-9）
 * 3. 下划线（_）
 */
constexpr bool is_identifier_char(char c) noexcept
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

constexpr bool is_first_identifier_char(char c) noexcept
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

constexpr std::size_t max_identifier_length = 255;
constexpr bool        is_identifier(std::string_view s) noexcept
{
    if (s.empty() || s.size() > max_identifier_length) {
        return false;
    }

    if (!is_first_identifier_char(s[0])) {
        return false;
    }

    for (size_t i = 1; i < s.size(); i++) {
        if (!is_identifier_char(s[i])) {
            return false;
        }
    }

    return true;
}

constexpr bool is_valid_interface_name(std::string_view name)
{
    // 必须至少有一个点分隔符
    bool has_dot = false;

    // 不能以点开头或结尾
    if (name.empty() || name[0] == '.' || name[name.size() - 1] == '.' || name.size() > max_identifier_length) {
        return false;
    }

    // 检查每个分段
    bool segment_start = true;
    for (size_t i = 0; i < name.size(); ++i) {
        char c = name[i];

        if (c == '.') {
            // 找到点分隔符
            has_dot = true;

            // 不允许连续的点
            if (i > 0 && name[i - 1] == '.') {
                return false;
            }

            segment_start = true;
        } else if (segment_start) {
            if (!is_first_identifier_char(c)) {
                return false;
            }
            segment_start = false;
        } else if (!is_identifier_char(c)) {
            return false;
        }
    }

    return has_dot;
}

template <typename Derived, typename Base>
std::uintptr_t get_base_offset()
{
    alignas(alignof(Derived)) char buffer[sizeof(Derived)];

    Derived* derived_ptr = reinterpret_cast<Derived*>(buffer);
    Base*    base_ptr    = static_cast<Base*>(derived_ptr);
    return reinterpret_cast<std::uintptr_t>(base_ptr) - reinterpret_cast<std::uintptr_t>(derived_ptr);
}

MC_API void set_current_thread_name(const std::string& name);
} // namespace mc

#endif // MC_COMMON_H
