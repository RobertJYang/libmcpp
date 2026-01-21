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

#include <limits>

#include <mc/array.h>
#include <mc/dict.h>
#include <mc/variant/variant_base.h>
#include <mc/variant/variant_common.h>
#include <mc/variant/variant_reference.h>
#include <mc/variant/variants.h>
#include <mc/variant/variants.inl>

namespace mc {

variants::variants() {
}

// 拷贝构造函数
variants::variants(const variants& other) {
    other.ensure_data();
    m_data = other.m_data;
}

// 拷贝赋值运算符
variants& variants::operator=(const variants& other) {
    other.ensure_data();
    m_data = other.m_data;
    return *this;
}

variants::variants(const mc::variant& other) {
    if (other.is_array()) {
        *this = other.get_array();
    } else {
        throw_type_error("array", other.get_type());
    }
}

variants::variants(const std::allocator<char>& alloc) {
    MC_UNUSED(alloc);
}

variants::variants(const variants& other, const std::allocator<char>& alloc) : variants(other) {
    // 忽略分配器参数，因为 variants 不使用分配器
    MC_UNUSED(alloc);
}

variants::variants(mc::shared_ptr<i_variants> data) : m_data(std::move(data)) {
}

void variants::ensure_data() const {
    if (!m_data) {
        m_data = mc::make_shared<detail::array_impl<variant>>();
    }
}

// 基本操作实现
size_t variants::size() const {
    return m_data ? m_data->do_size() : 0;
}

bool variants::empty() const {
    return m_data ? m_data->do_empty() : true;
}

// 元素访问实现
variant variants::at(size_t index) const {
    if (!m_data) {
        throw_runtime_error("variants is empty");
    }
    return m_data->do_at(index);
}

void variants::set(size_t index, const variant& value) {
    ensure_data();
    m_data->do_set(index, value);
}

// 修改操作实现
void variants::push_back(const variant& value) {
    ensure_data();
    m_data->do_push_back(value);
}

// emplace_back 实现
void variants::emplace_back(variant&& value) {
    ensure_data();
    m_data->do_push_back(std::move(value));
}

void variants::emplace_back(const variant& value) {
    ensure_data();
    m_data->do_push_back(value);
}

void variants::pop_back() {
    if (!m_data) {
        throw_runtime_error("variants is empty");
    }
    m_data->do_pop_back();
}

void variants::clear() {
    if (m_data) {
        m_data->do_clear();
    }
}

// 容量管理实现
void variants::reserve(size_t new_cap) {
    ensure_data();
    m_data->do_reserve(new_cap);
}

void variants::resize(size_t count) {
    ensure_data();
    m_data->do_resize(count);
}

void variants::resize(size_t count, const variant& value) {
    ensure_data();
    m_data->do_resize(count, value);
}

size_t variants::capacity() const {
    return m_data ? m_data->do_capacity() : 0;
}

size_t variants::max_size() const {
    return m_data ? m_data->do_max_size() : std::numeric_limits<size_t>::max();
}

void variants::shrink_to_fit() {
    if (m_data) {
        m_data->do_shrink_to_fit();
    }
}

// assign 方法实现
void variants::assign(size_t count, const variant& value) {
    ensure_data();
    m_data->do_assign(count, value);
}

void variants::assign(std::initializer_list<variant> ilist) {
    clear();
    for (const auto& item : ilist) {
        push_back(item);
    }
}

// swap 方法实现
void variants::swap(variants& other) noexcept {
    std::swap(m_data, other.m_data);
}

// 迭代器支持实现
void variants::for_each(std::function<void(const variant&)> visitor) const {
    if (m_data) {
        m_data->do_for_each(visitor);
    }
}

// 类型信息实现
std::type_index variants::element_type() const {
    return m_data ? m_data->element_type() : typeid(void);
}

std::string variants::element_type_name() const {
    return m_data ? m_data->element_type_name() : "void";
}

// 比较运算符实现
bool variants::operator==(const variants& other) const {
    if (m_data == other.m_data) {
        return true;
    }
    if (!m_data || !other.m_data) {
        return empty() && other.empty();
    }
    if (size() != other.size()) {
        return false;
    }

    // 逐个比较元素
    for (size_t i = 0; i < size(); ++i) {
        if (at(i) != other.at(i)) {
            return false;
        }
    }
    return true;
}

bool variants::operator!=(const variants& other) const {
    return !(*this == other);
}

bool variants::operator<(const variants& other) const {
    size_t min_size = std::min(size(), other.size());
    for (size_t i = 0; i < min_size; ++i) {
        if ((*this)[i] < other[i]) {
            return true;
        } else if ((*this)[i] > other[i]) {
            return false;
        }
    }
    return size() < other.size();
}

bool variants::operator>(const variants& other) const {
    size_t min_size = std::min(size(), other.size());
    for (size_t i = 0; i < min_size; ++i) {
        if ((*this)[i] > other[i]) {
            return true;
        } else if ((*this)[i] < other[i]) {
            return false;
        }
    }
    return size() > other.size();
}

bool variants::operator<=(const variants& other) const {
    return !(*this > other);
}

bool variants::operator>=(const variants& other) const {
    return !(*this < other);
}

// 深拷贝实现
variants variants::deep_copy(mc::detail::copy_context* ctx) const {
    if (!m_data) {
        return variants();
    }

    return variants(m_data->deep_copy(ctx));
}

// 引用访问支持实现
bool variants::supports_reference_access() const {
    return m_data ? m_data->supports_reference_access() : false;
}

variant* variants::get_ptr(size_t index) {
    if (!m_data) {
        return nullptr;
    }
    return m_data->get_ptr(index);
}

const variant* variants::get_ptr(size_t index) const {
    if (!m_data) {
        return nullptr;
    }
    return m_data->get_ptr(index);
}

// operator[] 实现
variant_reference variants::operator[](size_t index) {
    if (!m_data) {
        throw_runtime_error("variants is empty");
    }
    return variant_reference(*this, index);
}

variant variants::operator[](size_t index) const {
    if (!m_data) {
        throw_runtime_error("variants is empty");
    }
    return m_data->do_at(index);
}

// 获取可修改的引用
variant_reference variants::at_ref(size_t index) {
    if (!m_data) {
        throw_runtime_error("variants is empty");
    }
    return variant_reference(*this, index);
}

// 访问首尾元素
variant_reference variants::front() {
    if (!m_data || m_data->do_empty()) {
        throw_runtime_error("variants is empty");
    }
    return variant_reference(*this, 0);
}

variant variants::front() const {
    if (!m_data) {
        throw_runtime_error("variants is empty");
    }
    return m_data->do_front();
}

variant_reference variants::back() {
    if (!m_data || m_data->do_empty()) {
        throw_runtime_error("variants is empty");
    }
    return variant_reference(*this, m_data->do_size() - 1);
}

variant variants::back() const {
    if (!m_data) {
        throw_runtime_error("variants is empty");
    }
    return m_data->do_back();
}

// variants_iterator 实现
variants_iterator::variants_iterator() : m_data(nullptr), m_index(0) {
}

variants_iterator::variants_iterator(mc::shared_ptr<i_variants> data, size_t index)
    : m_data(data), m_index(index) {
}

variants_iterator& variants_iterator::operator++() {
    ++m_index;
    return *this;
}

variants_iterator variants_iterator::operator++(int) {
    variants_iterator tmp = *this;
    ++(*this);
    return tmp;
}

variants_iterator& variants_iterator::operator--() {
    --m_index;
    return *this;
}

variants_iterator variants_iterator::operator--(int) {
    variants_iterator tmp = *this;
    --(*this);
    return tmp;
}

variants_iterator& variants_iterator::operator+=(difference_type n) {
    m_index += n;
    return *this;
}

variants_iterator& variants_iterator::operator-=(difference_type n) {
    m_index -= n;
    return *this;
}

variants_iterator variants_iterator::operator+(difference_type n) const {
    variants_iterator tmp = *this;
    tmp += n;
    return tmp;
}

variants_iterator variants_iterator::operator-(difference_type n) const {
    variants_iterator tmp = *this;
    tmp -= n;
    return tmp;
}

variants_iterator::difference_type variants_iterator::operator-(const variants_iterator& other) const {
    return static_cast<difference_type>(m_index) - static_cast<difference_type>(other.m_index);
}

variants_iterator::reference variants_iterator::operator*() const {
    if (!m_data) {
        throw_runtime_error("variants_iterator: invalid iterator");
    }
    if (m_index >= m_data->do_size()) {
        throw_out_of_range_error("variants_iterator: index out of range");
    }
    return variant_reference(variants(m_data), m_index);
}

variants_iterator::reference variants_iterator::operator[](difference_type n) const {
    return *(*this + n);
}

bool variants_iterator::operator==(const variants_iterator& other) const {
    return m_data == other.m_data && m_index == other.m_index;
}

bool variants_iterator::operator!=(const variants_iterator& other) const {
    return !(*this == other);
}

bool variants_iterator::operator<(const variants_iterator& other) const {
    return m_data == other.m_data && m_index < other.m_index;
}

bool variants_iterator::operator<=(const variants_iterator& other) const {
    return m_data == other.m_data && m_index <= other.m_index;
}

bool variants_iterator::operator>(const variants_iterator& other) const {
    return m_data == other.m_data && m_index > other.m_index;
}

bool variants_iterator::operator>=(const variants_iterator& other) const {
    return m_data == other.m_data && m_index >= other.m_index;
}

// variants_const_iterator 实现
variants_const_iterator::variants_const_iterator() : m_data(nullptr), m_index(0) {
}

variants_const_iterator::variants_const_iterator(mc::shared_ptr<i_variants> data, size_t index)
    : m_data(data), m_index(index) {
}

variants_const_iterator::variants_const_iterator(const variants_iterator& other)
    : m_data(other.m_data), m_index(other.m_index) {
}

variants_const_iterator& variants_const_iterator::operator++() {
    ++m_index;
    return *this;
}

variants_const_iterator variants_const_iterator::operator++(int) {
    variants_const_iterator tmp = *this;
    ++(*this);
    return tmp;
}

variants_const_iterator& variants_const_iterator::operator--() {
    --m_index;
    return *this;
}

variants_const_iterator variants_const_iterator::operator--(int) {
    variants_const_iterator tmp = *this;
    --(*this);
    return tmp;
}

variants_const_iterator& variants_const_iterator::operator+=(difference_type n) {
    m_index += n;
    return *this;
}

variants_const_iterator& variants_const_iterator::operator-=(difference_type n) {
    m_index -= n;
    return *this;
}

variants_const_iterator variants_const_iterator::operator+(difference_type n) const {
    variants_const_iterator tmp = *this;
    tmp += n;
    return tmp;
}

variants_const_iterator variants_const_iterator::operator-(difference_type n) const {
    variants_const_iterator tmp = *this;
    tmp -= n;
    return tmp;
}

variants_const_iterator::difference_type variants_const_iterator::operator-(const variants_const_iterator& other) const {
    return static_cast<difference_type>(m_index) - static_cast<difference_type>(other.m_index);
}

variants_const_iterator::reference variants_const_iterator::operator*() const {
    if (!m_data) {
        throw_runtime_error("variants_const_iterator: invalid iterator");
    }
    if (m_index >= m_data->do_size()) {
        throw_out_of_range_error("variants_const_iterator: index out of range");
    }
    return variant_reference(variants(m_data), m_index);
}

variants_const_iterator::reference variants_const_iterator::operator[](difference_type n) const {
    return *(*this + n);
}

bool variants_const_iterator::operator==(const variants_const_iterator& other) const {
    return m_data == other.m_data && m_index == other.m_index;
}

bool variants_const_iterator::operator!=(const variants_const_iterator& other) const {
    return !(*this == other);
}

bool variants_const_iterator::operator<(const variants_const_iterator& other) const {
    return m_data == other.m_data && m_index < other.m_index;
}

bool variants_const_iterator::operator<=(const variants_const_iterator& other) const {
    return m_data == other.m_data && m_index <= other.m_index;
}

bool variants_const_iterator::operator>(const variants_const_iterator& other) const {
    return m_data == other.m_data && m_index > other.m_index;
}

bool variants_const_iterator::operator>=(const variants_const_iterator& other) const {
    return m_data == other.m_data && m_index >= other.m_index;
}

// 插入操作实现
void variants::insert(size_t pos, const variant& value) {
    ensure_data();
    m_data->do_insert(pos, value);
}

void variants::insert(size_t pos, size_t count, const variant& value) {
    ensure_data();
    for (size_t i = 0; i < count; ++i) {
        m_data->do_insert(pos + i, value);
    }
}

// 删除操作实现
void variants::erase(size_t pos) {
    if (!m_data) {
        throw_runtime_error("variants is empty");
    }
    m_data->do_erase(pos);
}

void variants::erase(size_t first, size_t last) {
    if (!m_data) {
        throw_runtime_error("variants is empty");
    }
    m_data->do_erase(first, last);
}

variants_iterator variants::erase(variants_const_iterator pos) {
    if (!m_data) {
        throw_runtime_error("variants is empty");
    }
    size_t index = pos - begin();
    m_data->do_erase(index);
    return begin() + index;
}

variants_iterator variants::erase(variants_const_iterator first, variants_const_iterator last) {
    if (!m_data) {
        throw_runtime_error("variants is empty");
    }
    size_t first_index = first - begin();
    size_t last_index  = last - begin();
    m_data->do_erase(first_index, last_index);
    return begin() + first_index;
}

// 拷贝操作实现
variants variants::copy() const {
    return variants(m_data->copy());
}

// 初始化列表构造函数实现
variants::variants(const std::initializer_list<variant>& list) {
    auto data = mc::make_shared<detail::array_impl<variant>>();

    for (const auto& item : list) {
        static_cast<std::vector<variant>*>(data.get())->push_back(item);
    }

    m_data = data;
}

variants::variants(const std::initializer_list<variant_reference>& list) {
    auto data = mc::make_shared<detail::array_impl<variant>>();
    for (const auto& ref : list) {
        static_cast<std::vector<variant>*>(data.get())->push_back(ref.get());
    }
    m_data = data;
}

// 模板版本的 insert 实现
template <typename InputIt>
void variants::insert(size_t pos, InputIt first, InputIt last) {
    ensure_data();
    size_t index = pos;
    for (auto it = first; it != last; ++it, ++index) {
        m_data->do_insert(index, *it);
    }
}

// 显式实例化常用的模板版本
template void MC_API variants::insert<variants_const_iterator>(size_t pos, variants_const_iterator first, variants_const_iterator last);
template void MC_API variants::insert<variants_iterator>(size_t pos, variants_iterator first, variants_iterator last);

// 迭代器版本的 insert 显式实例化
template variants_iterator MC_API variants::insert<variants_const_iterator>(variants_iterator pos, variants_const_iterator first, variants_const_iterator last);
template variants_iterator MC_API variants::insert<variants_iterator>(variants_iterator pos, variants_iterator first, variants_iterator last);

// variants 迭代器方法实现
variants_const_iterator variants::begin() const {
    return variants_const_iterator(m_data, 0);
}

variants_const_iterator variants::end() const {
    return variants_const_iterator(m_data, m_data ? m_data->do_size() : 0);
}

variants_iterator variants::begin() {
    return variants_iterator(m_data, 0);
}

variants_iterator variants::end() {
    return variants_iterator(m_data, m_data ? m_data->do_size() : 0);
}

variants_const_iterator variants::cbegin() const {
    return begin();
}

variants_const_iterator variants::cend() const {
    return end();
}

// 反向迭代器实现
variants::reverse_iterator variants::rbegin() {
    return reverse_iterator(end());
}

variants::const_reverse_iterator variants::rbegin() const {
    return const_reverse_iterator(end());
}

variants::const_reverse_iterator variants::crbegin() const {
    return const_reverse_iterator(cend());
}

variants::reverse_iterator variants::rend() {
    return reverse_iterator(begin());
}

variants::const_reverse_iterator variants::rend() const {
    return const_reverse_iterator(begin());
}

variants::const_reverse_iterator variants::crend() const {
    return const_reverse_iterator(cbegin());
}

size_t calculate_array_hash(const variants& array_data) {
    if (array_data.empty()) {
        return 0;
    }

    // 使用黄金比例作为种子
    size_t       h    = 0x9e3779b9 ^ array_data.size();
    const size_t step = (array_data.size() >> 5) + 1;
    for (size_t l1 = array_data.size(); l1 >= step; l1 -= step) {
        h = h ^ ((h << 5) + (h >> 2) + array_data[l1 - 1].hash());
    }

    return h;
}

const i_variants* variants::data() const {
    return m_data.get();
}

i_variants* variants::data() {
    return m_data.get();
}

} // namespace mc
