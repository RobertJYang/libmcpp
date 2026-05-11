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

/**
 * @file string_storage.h
 * @brief mc::string 内部存储类型。
 */
#ifndef MCBASE_SRC_STRING_STRING_STORAGE_H
#define MCBASE_SRC_STRING_STRING_STORAGE_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <mc/memory/shared_base.h>
#include <mc/string_view.h>

namespace mc::detail {

// mc::string 内部字节缓冲
struct string_bytes {
    std::vector<char> m_buffer;

    string_bytes();
    explicit string_bytes(mc::string_view v);
    explicit string_bytes(std::size_t n, char ch);

    void ensure_initialized();
    void assign(mc::string_view v);

    bool        empty() const noexcept;
    std::size_t size() const noexcept;
    std::size_t capacity() const noexcept;

    mc::string_view view() const noexcept;
    const char*     data() const noexcept;
    char*           data_mutable() noexcept;

    bool overlaps(mc::string_view value) const noexcept;

    void clear() noexcept;
    void reserve(std::size_t n);
    void shrink_to_fit();

    void push_back(char c);
    void pop_back();
    void resize(std::size_t n, char ch);
    void insert(std::size_t pos, std::size_t count, char ch);

    char&       operator[](std::size_t i) noexcept;
    const char& operator[](std::size_t i) const noexcept;
    char&       at(std::size_t i);
    char&       front();
    char&       back();

    using iterator         = std::vector<char>::iterator;
    using const_iterator   = std::vector<char>::const_iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;

    iterator         begin() noexcept;
    iterator         end() noexcept;
    reverse_iterator rbegin() noexcept;
    reverse_iterator rend() noexcept;

    void append(const char* str);
    void append(const char* p, std::size_t n);
    void append(std::size_t n, char ch);

    void erase(std::size_t pos, std::size_t count);
    void erase(iterator first, iterator last);
    void replace(std::size_t pos, std::size_t count, const char* s, std::size_t n);

    std::size_t find(mc::string_view needle, std::size_t pos) const;
};

struct string_storage : mc::memory::shared_base {
    using id_type = std::uint32_t; // 对应 mc::quark::id_type，通过裸别名避免对 quark.h 的重依赖

    id_type      m_quark_id{0U};
    string_bytes bytes;

    string_storage();
    explicit string_storage(mc::string_view view);
    explicit string_storage(std::size_t n, char ch);
    explicit string_storage(id_type id) noexcept;

    bool is_quark() const noexcept;

    mc::string_view view_const() const noexcept;
    const char*     data_const() const noexcept;
    std::size_t     size_const() const noexcept;
    bool            empty_const() const noexcept;
    std::size_t     capacity_const() const noexcept;
};

} // namespace mc::detail

#endif // MCBASE_SRC_STRING_STRING_STORAGE_H
