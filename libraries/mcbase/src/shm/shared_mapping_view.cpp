/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/shm/shared_mapping_view.h>

#include <sys/mman.h>

namespace mc::shm {

shared_mapping_view::shared_mapping_view() noexcept : m_address(nullptr), m_size(0)
{}

shared_mapping_view::shared_mapping_view(mc::string_view name, std::size_t size, detail::open_mode mode)
    : m_backend(detail::shared_memory_backend::open(name, size, mode)), m_address(nullptr), m_size(0)
{
    if (!m_backend.is_valid()) {
        return;
    }

    m_size = m_backend.size();
    if (m_size == 0) {
        return;
    }

    m_address = mmap(nullptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_backend.fd(), 0);
    if (m_address == MAP_FAILED) {
        m_address = nullptr;
        m_size = 0;
        m_backend = detail::shared_memory_backend{};
    }
}

shared_mapping_view::~shared_mapping_view()
{
    if (m_address != nullptr && m_size > 0) {
        munmap(m_address, m_size);
    }
}

shared_mapping_view::shared_mapping_view(shared_mapping_view&& other) noexcept
    : m_backend(std::move(other.m_backend)), m_address(other.m_address), m_size(other.m_size)
{
    other.m_address = nullptr;
    other.m_size = 0;
}

shared_mapping_view& shared_mapping_view::operator=(shared_mapping_view&& other) noexcept
{
    if (this != &other) {
        if (m_address != nullptr && m_size > 0) {
            munmap(m_address, m_size);
        }

        m_backend = std::move(other.m_backend);
        m_address = other.m_address;
        m_size = other.m_size;
        other.m_address = nullptr;
        other.m_size = 0;
    }
    return *this;
}

bool shared_mapping_view::is_valid() const noexcept
{
    return m_address != nullptr;
}

bool shared_mapping_view::created() const noexcept
{
    return m_backend.created();
}

void* shared_mapping_view::data() const noexcept
{
    return m_address;
}

std::size_t shared_mapping_view::size() const noexcept
{
    return m_size;
}

const mc::string& shared_mapping_view::name() const noexcept
{
    return m_backend.name();
}

} // namespace mc::shm
