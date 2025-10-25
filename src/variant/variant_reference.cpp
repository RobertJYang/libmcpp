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

#include <mc/exception.h>
#include <mc/variant/variant_base.h>
#include <mc/variant/variant_reference.h>
#include <mc/variant/variants.h>

namespace mc {

template <typename Config>
variant_reference<Config>::variant_reference(variant_type& var)
    : m_holder(&var) {
}

template <typename Config>
variant_reference<Config>::variant_reference(variant_type&& var)
    : m_holder(value_holder(std::move(var))) {
}

template <typename Config>
variant_reference<Config>::variant_reference(mc::shared_ptr<variant_extension_base> ext, std::size_t index)
    : m_holder(extension_accessor(std::move(ext), index)) {
}

template <typename Config>
variant_reference<Config>::variant_reference(mc::shared_ptr<variant_extension_base> ext, std::string key)
    : m_holder(extension_accessor(std::move(ext), std::move(key))) {
}

template <typename Config>
variant_reference<Config>::variant_reference(variants cont, std::size_t index)
    : m_holder(variants_accessor(std::move(cont), index)) {
}

template <typename Config>
variant_reference<Config>& variant_reference<Config>::operator=(variant_reference&& other) noexcept {
    if (this != &other) {
        *this = other.get();
    }
    return *this;
}

template <typename Config>
void variant_reference<Config>::swap(variant_reference& other) noexcept {
    auto temp = this->get();
    *this     = other.get();
    other     = temp;
}

template <typename Config>
typename variant_reference<Config>::variant_type& variant_reference<Config>::get() {
    return std::visit([](auto&& holder) -> variant_type& {
        using T = std::decay_t<decltype(holder)>;
        if constexpr (std::is_same_v<T, variant_type*>) {
            return *holder;
        } else if constexpr (std::is_same_v<T, extension_accessor>) {
            // 对于 extension，需要先获取值，缓存，然后返回引用
            if (!holder.cached_value.has_value()) {
                if (auto* idx = std::get_if<std::size_t>(&holder.key)) {
                    holder.cached_value = holder.extension->get(*idx);
                } else {
                    holder.cached_value = holder.extension->get(std::get<std::string>(holder.key));
                }
            }
            return *holder.cached_value;
        } else if constexpr (std::is_same_v<T, variants_accessor>) {
            // 对于 variants，尝试直接获取指针引用
            if (holder.container.supports_reference_access()) {
                if (auto* ptr = holder.container.get_ptr(holder.index)) {
                    return *ptr;
                }
            }
            // 如果无法获取指针引用，使用缓存机制
            if (!holder.cached_value.has_value()) {
                holder.cached_value = holder.container.at(holder.index);
            }
            return *holder.cached_value;
        } else if constexpr (std::is_same_v<T, value_holder>) {
            // 对于临时值，直接返回引用
            return holder.value;
        } else {
            // 不应该到达这里，但为了编译通过需要返回值
            throw_runtime_error("Invalid variant_reference state");
        }
    }, m_holder);
}

template <typename Config>
const typename variant_reference<Config>::variant_type& variant_reference<Config>::get() const {
    return std::visit([](auto&& holder) -> const variant_type& {
        using T = std::decay_t<decltype(holder)>;
        if constexpr (std::is_same_v<T, variant_type*>) {
            return *holder;
        } else if constexpr (std::is_same_v<T, extension_accessor>) {
            if (!holder.cached_value.has_value()) {
                if (auto* idx = std::get_if<std::size_t>(&holder.key)) {
                    holder.cached_value = holder.extension->get(*idx);
                } else {
                    holder.cached_value = holder.extension->get(std::get<std::string>(holder.key));
                }
            }
            return *holder.cached_value;
        } else if constexpr (std::is_same_v<T, variants_accessor>) {
            // 对于 variants，尝试直接获取指针引用
            if (holder.container.supports_reference_access()) {
                if (auto* ptr = holder.container.get_ptr(holder.index)) {
                    return *ptr;
                }
            }
            // 如果无法获取指针引用，使用缓存机制
            if (!holder.cached_value.has_value()) {
                holder.cached_value = holder.container.at(holder.index);
            }
            return *holder.cached_value;
        } else if constexpr (std::is_same_v<T, value_holder>) {
            return holder.value;
        } else {
            // 不应该到达这里，但为了编译通过需要返回值
            throw_runtime_error("Invalid variant_reference state");
        }
    }, m_holder);
}

template <typename Config>
variant_reference<Config>::operator const variant_type&() const {
    return get();
}

template <typename Config>
variant_reference<Config>::operator variant_type&() {
    return get();
}

template <typename Config>
variant_reference<Config>& variant_reference<Config>::operator=(const variant_type& value) {
    std::visit([&value](auto&& holder) {
        using T = std::decay_t<decltype(holder)>;
        if constexpr (std::is_same_v<T, variant_type*>) {
            // 引用模式：直接赋值
            *holder = value;
        } else if constexpr (std::is_same_v<T, extension_accessor>) {
            // extension 模式：调用 set 方法
            if (auto* idx = std::get_if<std::size_t>(&holder.key)) {
                holder.extension->set(*idx, value);
            } else {
                holder.extension->set(std::get<std::string>(holder.key), value);
            }
            // 更新缓存
            holder.cached_value = value;
        } else if constexpr (std::is_same_v<T, variants_accessor>) {
            // variants 模式：调用 set 方法
            holder.container.set(holder.index, value);
            // 更新缓存
            holder.cached_value = value;
        } else if constexpr (std::is_same_v<T, value_holder>) {
            // 临时值模式：直接赋值
            holder.value = value;
        }
    }, m_holder);
    return *this;
}

template <typename Config>
variant_reference<Config> variant_reference<Config>::operator[](std::size_t pos) {
    return get()[pos];
}

template <typename Config>
variant_reference<Config> variant_reference<Config>::operator[](std::size_t pos) const {
    return get()[pos];
}

template <typename Config>
variant_reference<Config> variant_reference<Config>::operator[](std::string_view key) {
    return get()[key];
}

template <typename Config>
variant_reference<Config> variant_reference<Config>::operator[](std::string_view key) const {
    return get()[key];
}

template <typename Config>
typename variant_reference<Config>::variant_type& variant_reference<Config>::operator*() {
    return get();
}

template <typename Config>
const typename variant_reference<Config>::variant_type& variant_reference<Config>::operator*() const {
    return get();
}

template <typename Config>
typename variant_reference<Config>::variant_type* variant_reference<Config>::operator->() {
    return &get();
}

template <typename Config>
const typename variant_reference<Config>::variant_type* variant_reference<Config>::operator->() const {
    return &get();
}

template <typename Config>
void swap(variant_reference<Config> lhs, variant_reference<Config> rhs) noexcept {
    auto temp = lhs.get();
    lhs       = rhs.get();
    rhs       = temp;
}

// 显式实例化
template class MC_API variant_reference<variant_config<>>;
template void MC_API  swap(variant_reference<variant_config<>> lhs, variant_reference<variant_config<>> rhs) noexcept;

} // namespace mc
