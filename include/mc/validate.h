#ifndef VALIDATE_H
#define VALIDATE_H

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace mc {
namespace validate {

// 类型错误异常
class TypeError : public std::runtime_error {
public:
    explicit TypeError(const std::string& message)
        : std::runtime_error("TypeError: " + message)
    {}
};

// 范围错误异常
class RangeError : public std::runtime_error {
public:
    explicit RangeError(const std::string& message)
        : std::runtime_error("RangeError: " + message)
    {}
};

// 范围错误异常
class InternalError : public std::runtime_error {
public:
    explicit InternalError(const std::string& message)
        : std::runtime_error("InternalError: " + message)
    {}
};

class Validator {
public:
    template <typename T, typename ValueType>
    void check(ValueType&& value) const
    {
        if (std::is_integral_v<T>) {
            check_integer<T>(std::forward<ValueType>(value));
            return;
        }

        throw InternalError("Unsupported type for validation");
    }

private:
    // 这个函数的功能是判断value是否可以转为T类型
    template <typename T, typename ValueType>
    void check_integer(ValueType&& value) const
    {
        // 获取值的基础类型（去除引用和const）
        using ValueBaseType = typename std::remove_cv<
            typename std::remove_reference<ValueType>::type>::type;

        // 确保传入的 value 的基础类型也是整数类型
        if constexpr (!std::is_integral_v<ValueBaseType>) {
            throw TypeError(std::string("Value type error, expected integer, get type with name: ") + typeid(ValueBaseType).name());
        } else {
            using Limits = std::numeric_limits<T>;

            // 范围检查 - 使用 long long 进行比较以避免溢出
            if constexpr (std::is_signed_v<ValueBaseType> && std::is_signed_v<T>) {
                // 两个都是有符号类型
                if (static_cast<long long>(value) < static_cast<long long>(Limits::min()) ||
                    static_cast<long long>(value) > static_cast<long long>(Limits::max())) {
                    throw RangeError(std::string("Value out of range for target type: ") + typeid(T).name());
                }
            } else if constexpr (!std::is_signed_v<ValueBaseType> && !std::is_signed_v<T>) {
                // 两个都是无符号类型
                if (static_cast<unsigned long long>(value) < static_cast<unsigned long long>(Limits::min()) ||
                    static_cast<unsigned long long>(value) > static_cast<unsigned long long>(Limits::max())) {
                    throw RangeError(std::string("Value out of range for target type: ") + typeid(T).name());
                }
            } else if constexpr (std::is_signed_v<ValueBaseType> && !std::is_signed_v<T>) {
                // 值是有符号的，目标是无符号的
                if (value < 0 || static_cast<unsigned long long>(value) > static_cast<unsigned long long>(Limits::max())) {
                    throw RangeError(std::string("Value out of range for target type: ") + typeid(T).name());
                }
            } else {
                // 值是无符号的，目标是有符号的
                if (static_cast<unsigned long long>(value) > static_cast<unsigned long long>(Limits::max())) {
                    throw RangeError(std::string("Value out of range for target type: ") + typeid(T).name());
                }
            }
        }
    }
};

}  // namespace validate
}  // namespace mc

#endif // VALIDATE_H