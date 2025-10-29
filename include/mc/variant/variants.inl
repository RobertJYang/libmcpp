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
 * @file variants.inl
 * @brief variants 类的模板实现
 */

#ifndef MC_VARIANTS_INL
#define MC_VARIANTS_INL

#include <mc/array.h>
#include <mc/variant/variant_common.h>

namespace mc {

template <typename T, typename Allocator>
array<T, Allocator>::array(const std::initializer_list<variant_reference>& init,
                           const Allocator&                                alloc)
    : m_data(mc::make_shared<detail::array_impl<mc::variant, Allocator>>(alloc)) {
    m_data->reserve(init.size());
    for (const auto& ref : init) {
        m_data->push_back(ref.get());
    }
}

template <typename T, typename Allocator>
template <typename InputIt>
array<T, Allocator>::array(
    InputIt first, InputIt last,
    const Allocator& alloc,
    std::enable_if_t<is_variant_reference_v<typename std::iterator_traits<InputIt>::value_type>>*)
    : m_data(mc::make_shared<detail::array_impl<mc::variant, Allocator>>(alloc)) {
    for (auto it = first; it != last; ++it) {
        m_data->push_back(it->get());
    }
}

template <typename T, typename Allocator>
variant detail::array_impl<T, Allocator>::do_at(size_t index) const {
    if (index >= base_type::size()) {
        throw_out_of_range_error("array_impl::at: index out of range");
    }
    if constexpr (mc::is_variant_constructible_v<T>) {
        return variant((*this)[index]);
    } else {
        return variant();
    }
}

template <typename T, typename Allocator>
void detail::array_impl<T, Allocator>::do_set(size_t index, const variant& value) {
    if (index >= base_type::size()) {
        throw_out_of_range_error("array_impl::set: index out of range");
    }
    if constexpr (mc::is_variant_constructible_v<T>) {
        if constexpr (std::is_same_v<T, variant>) {
            (*this)[index] = value;
        } else {
            try {
                from_variant(value, (*this)[index]);
            } catch (const std::exception& e) {
                throw_bad_cast_error(("Cannot assign value to strongly-typed array: " + std::string(e.what())).c_str());
            }
        }
    } else {
        MC_UNUSED(value);
        throw_bad_cast_error("Cannot set value: type T is not convertible to variant");
    }
}

template <typename T, typename Allocator>
void detail::array_impl<T, Allocator>::do_push_back(const variant& value) {
    if constexpr (mc::is_variant_constructible_v<T>) {
        base_type::push_back(value.as<T>());
    } else {
        MC_UNUSED(value);
        throw_bad_cast_error("Cannot push_back: type T is not convertible to variant");
    }
}

template <typename T, typename Allocator>
void detail::array_impl<T, Allocator>::do_pop_back() {
    base_type::pop_back();
}

template <typename T, typename Allocator>
void detail::array_impl<T, Allocator>::do_insert(size_t pos, const variant& value) {
    if (pos > base_type::size()) {
        throw_out_of_range_error("array_impl::insert: position out of range");
    }

    if constexpr (mc::is_variant_constructible_v<T>) {
        base_type::insert(base_type::begin() + pos, value.as<T>());
    } else {
        MC_UNUSED(value);
        throw_bad_cast_error("Cannot insert: type T is not convertible to variant");
    }
}

template <typename T, typename Allocator>
void detail::array_impl<T, Allocator>::do_resize(size_t count, const variant& value) {
    if constexpr (mc::is_variant_constructible_v<T>) {
        base_type::resize(count, value.as<T>());
    } else {
        MC_UNUSED(value);
        MC_UNUSED(count);
        throw_bad_cast_error("Cannot resize with value: type T is not convertible to variant");
    }
}

template <typename T, typename Allocator>
void detail::array_impl<T, Allocator>::do_for_each(std::function<void(const variant&)> visitor) const {
    if constexpr (mc::is_variant_constructible_v<T>) {
        for (const auto& item : *this) {
            visitor(variant(item));
        }
    } else {
        MC_UNUSED(visitor);
    }
}

template <typename T, typename Allocator>
bool detail::array_impl<T, Allocator>::supports_reference_access() const {
    return std::is_same_v<T, variant>;
}

template <typename T, typename Allocator>
variant* detail::array_impl<T, Allocator>::get_ptr(size_t index) {
    if (index >= base_type::size()) {
        return nullptr;
    }
    if constexpr (std::is_same_v<T, variant>) {
        return &(*this)[index];
    } else {
        return nullptr;
    }
}

template <typename T, typename Allocator>
const variant* detail::array_impl<T, Allocator>::get_ptr(size_t index) const {
    if (index >= base_type::size()) {
        return nullptr;
    }
    if constexpr (std::is_same_v<T, variant>) {
        return &(*this)[index];
    } else {
        return nullptr;
    }
}

template <typename T, typename Allocator>
variants::variants(mc::array<T, Allocator> arr,
                   std::enable_if_t<is_variant_constructible_v<T> || is_variant_v<T>>*) {
    auto impl_ptr = arr.get_impl();
    if (impl_ptr) {
        m_data = mc::shared_ptr<i_variants>(impl_ptr.get());
    } else {
        m_data = mc::make_shared<detail::array_impl<T, Allocator>>();
    }
}

template <typename T, typename Allocator>
variants::variants(const std::vector<T, Allocator>& vec,
                   std::enable_if_t<is_variant_constructible_v<T> || is_variant_v<T>>*) {
    auto data = mc::make_shared<detail::array_impl<variant>>();
    for (const auto& item : vec) {
        static_cast<std::vector<variant>*>(data.get())->push_back(variant(item));
    }
    m_data = data;
}

template <typename T, typename Allocator>
variants::variants(std::vector<T, Allocator>&& vec,
                   std::enable_if_t<is_variant_constructible_v<T> || is_variant_v<T>>*) {
    if constexpr (std::is_same_v<T, variant>) {
        m_data = mc::make_shared<detail::array_impl<variant>>(vec);
    } else {
        auto data = mc::make_shared<detail::array_impl<variant>>();
        for (auto&& item : vec) {
            static_cast<std::vector<variant>*>(data.get())->push_back(variant(std::move(item)));
        }
        m_data = data;
    }
}

template <typename T>
variants::variants(const std::initializer_list<T>& list,
                   std::enable_if_t<is_variant_constructible_v<T> &&
                                    !is_variant_reference_v<T> &&
                                    !std::is_same_v<T, variant>>*) {
    auto data = mc::make_shared<detail::array_impl<variant>>();
    for (const auto& item : list) {
        static_cast<std::vector<variant>*>(data.get())->push_back(variant(item));
    }
    m_data = data;
}

// ========== 特殊构造函数实现：处理 variant_reference 类型 ==========

template <typename InputIt>
variants::variants(InputIt first, InputIt last,
                   std::enable_if_t<is_variant_reference_v<typename std::iterator_traits<InputIt>::value_type>>*) {
    auto data = mc::make_shared<detail::array_impl<variant>>();
    for (auto it = first; it != last; ++it) {
        static_cast<std::vector<variant>*>(data.get())->push_back(it->get());
    }
    m_data = data;
}

template <typename InputIt>
variants_iterator variants::insert(variants_iterator pos, InputIt first, InputIt last) {
    size_t index = pos - begin();
    insert(index, first, last);
    return begin() + index;
}

template <typename T, typename Allocator>
mc::shared_ptr<mc::i_variants> detail::array_impl<T, Allocator>::copy() const {
    return mc::make_shared<detail::array_impl<T, Allocator>>(
        static_cast<const std::vector<T, Allocator>&>(*this));
}

template <typename T, typename Allocator>
mc::shared_ptr<mc::i_variants> detail::array_impl<T, Allocator>::deep_copy(mc::detail::copy_context* ctx) const {
    if (!ctx) {
        mc::detail::copy_context local_ctx;
        return deep_copy(&local_ctx);
    }

    // 检测循环引用：如果 this 已经被拷贝过，直接返回已拷贝的版本
    if (ctx->has_copied(this)) {
        return ctx->get_copied<array_impl>(this);
    }

    // 创建新的 array_impl 并记录到上下文
    auto result = mc::make_shared<array_impl<T, Allocator>>();
    result->reserve(this->size());
    ctx->record_copied(this, result);

    // 递归深拷贝元素，deep_copy > copy > 直接拷贝
    if constexpr (mc::traits::has_deep_copy_v<T>) {
        // 优先使用深拷贝
        for (const auto& item : *this) {
            result->push_back(item.deep_copy(ctx));
        }
    } else if constexpr (mc::traits::has_copy_v<T>) {
        // 其次使用浅拷贝
        for (const auto& item : *this) {
            result->push_back(item.copy());
        }
    } else {
        // 最后直接拷贝
        for (const auto& item : *this) {
            static_cast<std::vector<T, Allocator>&>(*result).push_back(item);
        }
    }
    return result;
}

// ========== variants 与 mc::array<T> 的比较运算符 ==========

template <typename T, typename Allocator>
bool operator==(const variants& lhs, const mc::array<T, Allocator>& rhs) {
    if ((lhs.data() == rhs.get_impl().get()) || (lhs.empty() && rhs.empty())) {
        return true;
    }
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }
    return true;
}

template <typename T, typename Allocator>
bool operator==(const mc::array<T, Allocator>& lhs, const variants& rhs) {
    return rhs == lhs;
}

template <typename T, typename Allocator>
bool operator!=(const variants& lhs, const mc::array<T, Allocator>& rhs) {
    return !(lhs == rhs);
}

template <typename T, typename Allocator>
bool operator!=(const mc::array<T, Allocator>& lhs, const variants& rhs) {
    return !(lhs == rhs);
}

template <typename T, typename Allocator>
bool operator<(const variants& lhs, const mc::array<T, Allocator>& rhs) {
    if (lhs.empty()) {
        return !rhs.empty();
    }
    if (rhs.empty()) {
        return false;
    }
    // 字典序比较
    size_t min_size = std::min(lhs.size(), rhs.size());
    for (size_t i = 0; i < min_size; ++i) {
        if (lhs[i] < rhs[i]) {
            return true;
        } else if (lhs[i] > rhs[i]) {
            return false;
        }
    }
    return lhs.size() < rhs.size();
}

template <typename T, typename Allocator>
bool operator<(const mc::array<T, Allocator>& lhs, const variants& rhs) {
    if (lhs.empty()) {
        return !rhs.empty();
    }
    if (rhs.empty()) {
        return false;
    }
    // 字典序比较
    size_t min_size = std::min(lhs.size(), rhs.size());
    for (size_t i = 0; i < min_size; ++i) {
        if (lhs[i] < rhs[i]) {
            return true;
        } else if (lhs[i] > rhs[i]) {
            return false;
        }
    }
    return lhs.size() < rhs.size();
}

template <typename T, typename Allocator>
bool operator<=(const variants& lhs, const mc::array<T, Allocator>& rhs) {
    return !(rhs < lhs);
}

template <typename T, typename Allocator>
bool operator<=(const mc::array<T, Allocator>& lhs, const variants& rhs) {
    return !(rhs < lhs);
}

template <typename T, typename Allocator>
bool operator>(const variants& lhs, const mc::array<T, Allocator>& rhs) {
    return rhs < lhs;
}

template <typename T, typename Allocator>
bool operator>(const mc::array<T, Allocator>& lhs, const variants& rhs) {
    return rhs < lhs;
}

template <typename T, typename Allocator>
bool operator>=(const variants& lhs, const mc::array<T, Allocator>& rhs) {
    return !(lhs < rhs);
}

template <typename T, typename Allocator>
bool operator>=(const mc::array<T, Allocator>& lhs, const variants& rhs) {
    return !(lhs < rhs);
}

// ========== variants 与 std::vector<T> 的比较运算符 ==========

template <typename T, typename Allocator>
bool operator==(const variants& lhs, const std::vector<T, Allocator>& rhs) {
    if (lhs.empty() && rhs.empty()) {
        return true;
    }
    if (lhs.size() != rhs.size()) {
        return false;
    }
    // 逐个比较元素
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }
    return true;
}

template <typename T, typename Allocator>
bool operator==(const std::vector<T, Allocator>& lhs, const variants& rhs) {
    return rhs == lhs;
}

template <typename T, typename Allocator>
bool operator!=(const variants& lhs, const std::vector<T, Allocator>& rhs) {
    return !(lhs == rhs);
}

template <typename T, typename Allocator>
bool operator!=(const std::vector<T, Allocator>& lhs, const variants& rhs) {
    return !(lhs == rhs);
}

template <typename T, typename Allocator>
bool operator<(const variants& lhs, const std::vector<T, Allocator>& rhs) {
    if (lhs.empty()) {
        return !rhs.empty();
    }
    if (rhs.empty()) {
        return false;
    }
    // 字典序比较
    size_t min_size = std::min(lhs.size(), rhs.size());
    for (size_t i = 0; i < min_size; ++i) {
        if (lhs[i] < rhs[i]) {
            return true;
        } else if (lhs[i] > rhs[i]) {
            return false;
        }
    }
    return lhs.size() < rhs.size();
}

template <typename T, typename Allocator>
bool operator<(const std::vector<T, Allocator>& lhs, const variants& rhs) {
    if (lhs.empty()) {
        return !rhs.empty();
    }
    if (rhs.empty()) {
        return false;
    }
    // 字典序比较
    size_t min_size = std::min(lhs.size(), rhs.size());
    for (size_t i = 0; i < min_size; ++i) {
        if (lhs[i] < rhs[i]) {
            return true;
        } else if (lhs[i] > rhs[i]) {
            return false;
        }
    }
    return lhs.size() < rhs.size();
}

template <typename T, typename Allocator>
bool operator<=(const variants& lhs, const std::vector<T, Allocator>& rhs) {
    return !(rhs < lhs);
}

template <typename T, typename Allocator>
bool operator<=(const std::vector<T, Allocator>& lhs, const variants& rhs) {
    return !(rhs < lhs);
}

template <typename T, typename Allocator>
bool operator>(const variants& lhs, const std::vector<T, Allocator>& rhs) {
    return rhs < lhs;
}

template <typename T, typename Allocator>
bool operator>(const std::vector<T, Allocator>& lhs, const variants& rhs) {
    return rhs < lhs;
}

template <typename T, typename Allocator>
bool operator>=(const variants& lhs, const std::vector<T, Allocator>& rhs) {
    return !(lhs < rhs);
}

template <typename T, typename Allocator>
bool operator>=(const std::vector<T, Allocator>& lhs, const variants& rhs) {
    return !(lhs < rhs);
}

} // namespace mc

#endif // MC_VARIANTS_INL
