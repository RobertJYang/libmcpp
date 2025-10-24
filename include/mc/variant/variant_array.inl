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
 * @file array_extension.h
 * @brief mc::array 的 variant_extension 支持
 *
 * 此文件提供 mc::array<T> (T != variant) 到 variant 的隐式转换支持
 * 必须在 variant.h 之后包含
 */
#ifndef MC_ARRAY_EXTENSION_H
#define MC_ARRAY_EXTENSION_H

#include <mc/array.h>
#include <mc/traits.h>
#include <mc/variant/extension_type_id.h>
#include <mc/variant/variant_common.h>
#include <mc/variant/variant_extension.h>

namespace mc {

/**
 * @brief 为 array_impl<T> 特化类型信息
 */
template <typename T, typename Allocator>
struct extension_type_info_traits<detail::array_impl<T, Allocator>> {
    static extension_type_info get() {
        // 计算类型特征
        extension_type_trait traits = extension_type_trait::iterable |
                                      extension_type_trait::indexable;

        // 如果元素类型是 variant 或可转换为 variant，支持遍历访问
        if constexpr (is_variant_v<T> || is_variant_constructible_v<T>) {
            // 已经包含 iterable
        }

        // 如果元素类型是 variant，支持零拷贝引用访问
        if constexpr (is_variant_v<T>) {
            traits = traits | extension_type_trait::ref_accessible;
        }

        return extension_type_info(
            extension_type_ids::typed_array,
            "typed_array",
            traits);
    }
};

namespace detail {
template <typename T, typename Allocator>
void array_impl<T, Allocator>::visit(std::function<void(const mc::variant&)>&& visitor) const {
    if constexpr (mc::is_variant_v<T> || mc::is_variant_constructible_v<T>) {
        for (const auto& item : *this) {
            visitor(item);
        }
    }
}

template <typename T, typename Allocator>
bool array_impl<T, Allocator>::supports_reference_access() const {
    // 只有 T == variant 时才支持零开销引用访问
    return mc::is_variant_v<T>;
}

template <typename T, typename Allocator>
mc::variant* array_impl<T, Allocator>::get_ptr(std::size_t index) {
    if constexpr (mc::is_variant_v<T>) {
        if (index >= this->size()) {
            return nullptr;
        }
        return reinterpret_cast<mc::variant*>(&(*this)[index]);
    } else {
        MC_UNUSED(index);
        return nullptr;
    }
}

template <typename T, typename Allocator>
const mc::variant* array_impl<T, Allocator>::get_ptr(std::size_t index) const {
    if constexpr (mc::is_variant_v<T>) {
        if (index >= this->size()) {
            return nullptr;
        }
        return reinterpret_cast<const mc::variant*>(&(*this)[index]);
    } else {
        MC_UNUSED(index);
        return nullptr;
    }
}

template <typename T, typename Allocator>
mc::variant array_impl<T, Allocator>::get(std::size_t index) const {
    if (index >= this->size()) {
        throw_out_of_range_error("array_impl: 索引越界");
    }

    if constexpr (mc::is_variant_v<T>) {
        // T == variant：直接返回
        return (*this)[index];
    } else if constexpr (mc::is_variant_constructible_v<T>) {
        // T 可以转换为 variant
        return mc::variant((*this)[index]);
    } else {
        // T 无法转换为 variant：不支持
        throw_runtime_error("array_impl: 元素类型无法转换为 variant");
    }
}

template <typename T, typename Allocator>
void array_impl<T, Allocator>::set(std::size_t index, const mc::variant& value) {
    if (index >= this->size()) {
        throw_out_of_range_error("array_impl: 索引越界");
    }

    if constexpr (std::is_same_v<T, bool>) {
        // bool 特殊处理：std::vector<bool> 返回代理对象，无法直接用 from_variant
        bool temp;
        from_variant(value, temp);
        (*this)[index] = temp;
    } else if constexpr (mc::is_variant_v<T>) {
        // T == variant：直接赋值
        (*this)[index] = value;
    } else if constexpr (mc::is_variant_constructible_v<T>) {
        // T 可以从 variant 转换：提取值
        from_variant(value, (*this)[index]);
    } else {
        // T 无法从 variant 转换：不支持
        MC_UNUSED(value);
        throw_runtime_error("array_impl: 元素类型无法从 variant 转换");
    }
}

} // namespace detail

/**
 * @brief to_variant 重载：将 mc::array<T> 转换为 variant
 *
 * - 如果 T == variant：转换为 variant.array_type（内置数组类型）
 * - 如果 T != variant：转换为 variant.extension_type（直接使用 array_impl）
 */
template <typename T, typename Allocator, typename Config>
void to_variant(const mc::array<T, Allocator>& arr, variant_base<Config>& var) {
    if constexpr (is_variant_v<T>) {
        var = variant_base<Config>(arr);
    } else {
        auto impl_ptr = arr.get_data();
        if (impl_ptr) {
            var = variant_base<Config>(mc::static_pointer_cast<variant_extension_base>(impl_ptr));
        } else {
            // 空数组：创建一个空的 array_impl 作为 extension
            auto empty_impl = mc::make_shared<detail::array_impl<T, Allocator>>();
            var             = variant_base<Config>(mc::static_pointer_cast<variant_extension_base>(empty_impl));
        }
    }
}

/**
 * @brief from_variant 重载：从 variant 中提取 mc::array<T>
 *
 * 支持两种转换路径：
 * 1. mc::variants -> mc::array<variant>（内置数组）
 * 2. mc::array<int> -> mc::array<variant>（强类型数组 extension）
 */
template <typename T, typename Allocator, typename Config>
void from_variant(const variant_base<Config>& var, mc::array<T, Allocator>& arr) {
    // 路径1：内置数组类型 -> mc::array<T>
    if (var.is_array()) {
        if constexpr (is_variant_v<T>) {
            // T 是 variant：直接返回
            arr = var.get_array();
            return;
        } else if constexpr (is_variant_constructible_v<T>) {
            mc::array<T, Allocator> result;
            for (const auto& item : var.get_array()) {
                T converted;
                from_variant(item, converted);
                result.emplace_back(std::move(converted));
            }
            arr = std::move(result);
            return;
        }
        throw_bad_cast_error("类型转换异常: 目标数组元素类型不支持从 variant 转换");
    }

    // 路径2：extension 类型 -> mc::array<T>
    if (!var.is_extension()) {
        throw_bad_cast_error("类型转换异常: variant 不是数组类型");
    }

    auto ext = var.as_extension();
    if (!ext || ext->get_type_id() != extension_type_ids::typed_array) {
        throw_bad_cast_error("类型转换异常: extension 不是数组类型");
    }

    // 尝试精确类型匹配（最优路径：零拷贝）
    if (auto impl = mc::dynamic_pointer_cast<detail::array_impl<T, Allocator>>(ext)) {
        arr = mc::array<T, Allocator>::from_impl(impl);
        return;
    }

    // 类型不匹配，尝试元素转换（仅支持 T == variant）
    if constexpr (is_variant_v<T>) {
        const auto type_info = ext->get_type_info();
        if (type_info.is_iterable()) {
            mc::array<T, Allocator> result;
            ext->visit([&](const mc::variant& item) {
                result.push_back(item);
            });
            arr = result;
            return;
        }
    }

    throw_bad_cast_error("类型转换异常: 数组元素类型不匹配");
}

/**
 * @brief variant_base::as_array() 的实现
 *
 * 必须在 from_variant 定义之后，以确保编译器能找到正确的重载
 */
template <typename Config>
typename variant_base<Config>::array_type variant_base<Config>::as_array() const {
    if (m_type == type_id::array_type) {
        return m_array;
    }

    if (m_type == type_id::extension_type) {
        array_type result;
        mc::from_variant<typename array_type::value_type, typename array_type::allocator_type, Config>(
            *this, result);
        return result;
    }

    throw_type_error("array", m_type);
}

} // namespace mc

#endif // MC_ARRAY_EXTENSION_H
