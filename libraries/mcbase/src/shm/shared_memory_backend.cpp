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

#include <mc/shm/detail/shared_memory_backend.h>

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mc::shm::detail {
namespace {

constexpr mode_t      default_permission_bits = 0666;
constexpr std::size_t max_portable_shm_name   = 30;

} // namespace

shared_memory_backend::shared_memory_backend() noexcept : m_fd(-1), m_created(false), m_size(0)
{}

shared_memory_backend::shared_memory_backend(mc::string name, int fd, bool created) noexcept
    : m_name(std::move(name)), m_fd(fd), m_created(created), m_size(0)
{}

shared_memory_backend::~shared_memory_backend()
{
    if (m_fd >= 0) {
        close(m_fd);
    }
}

shared_memory_backend::shared_memory_backend(shared_memory_backend&& other) noexcept
    : m_name(std::move(other.m_name)), m_fd(other.m_fd), m_created(other.m_created), m_size(other.m_size)
{
    other.m_fd      = -1;
    other.m_created = false;
    other.m_size    = 0;
}

shared_memory_backend& shared_memory_backend::operator=(shared_memory_backend&& other) noexcept
{
    if (this != &other) {
        if (m_fd >= 0) {
            close(m_fd);
        }

        m_name          = std::move(other.m_name);
        m_fd            = other.m_fd;
        m_created       = other.m_created;
        m_size          = other.m_size;
        other.m_fd      = -1;
        other.m_size    = 0;
        other.m_created = false;
    }
    return *this;
}

shared_memory_backend shared_memory_backend::open(mc::string_view name, std::size_t size, open_mode mode)
{
    const mc::string formatted_name = format_name(name);
    bool             created        = false;
    int              fd             = -1;

    switch (mode) {
        case open_mode::open_existing:
            fd = shm_open(formatted_name.c_str(), O_RDWR, default_permission_bits);
            break;
        case open_mode::create_only:
            fd      = shm_open(formatted_name.c_str(), O_RDWR | O_CREAT | O_EXCL, default_permission_bits);
            created = fd >= 0;
            break;
        case open_mode::create_or_open:
            fd = shm_open(formatted_name.c_str(), O_RDWR | O_CREAT | O_EXCL, default_permission_bits);
            if (fd >= 0) {
                created = true;
            } else if (errno == EEXIST) {
                fd = shm_open(formatted_name.c_str(), O_RDWR, default_permission_bits);
            }
            break;
    }

    if (fd < 0) {
        return {};
    }

    if (created && size > 0) {
        if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
            close(fd);
            shm_unlink(formatted_name.c_str());
            return {};
        }
    }

    struct stat statbuf{};
    if (fstat(fd, &statbuf) != 0) {
        close(fd);
        if (created) {
            shm_unlink(formatted_name.c_str());
        }
        return {};
    }

    if (!created && size > 0 && static_cast<std::size_t>(statbuf.st_size) < size) {
        close(fd);
        return {};
    }

    shared_memory_backend backend(formatted_name, fd, created);
    backend.m_size = static_cast<std::size_t>(statbuf.st_size);
    return backend;
}

bool shared_memory_backend::remove(mc::string_view name) noexcept
{
    const mc::string formatted_name = format_name(name);
    if (formatted_name.empty()) {
        return false;
    }

    if (shm_unlink(formatted_name.c_str()) == 0) {
        return true;
    }

    return errno == ENOENT;
}

mc::string shared_memory_backend::format_name(mc::string_view name)
{
    if (name.empty()) {
        return {};
    }

    mc::string raw_name(name);
    if (!raw_name.empty() && raw_name.front() == '/') {
        raw_name.erase(0, 1);
    }

    mc::string portable_name;
    portable_name.reserve(raw_name.size());
    for (const unsigned char ch : raw_name) {
        portable_name.push_back(std::isalnum(ch) ? static_cast<char>(ch) : '_');
    }
    if (portable_name.empty()) {
        portable_name = "mc_shm";
    }

    if (portable_name.size() > max_portable_shm_name) {
        const auto hash_value = std::hash<std::string>{}(static_cast<std::string>(portable_name));
        char       suffix[17] = {};
        std::snprintf(suffix, sizeof(suffix), "%016zx", hash_value);
        portable_name = portable_name.substr(0, max_portable_shm_name - 17) + "_" + suffix;
    }

    return "/" + portable_name;
}

bool shared_memory_backend::is_valid() const noexcept
{
    return m_fd >= 0;
}

int shared_memory_backend::fd() const noexcept
{
    return m_fd;
}

bool shared_memory_backend::created() const noexcept
{
    return m_created;
}

const mc::string& shared_memory_backend::name() const noexcept
{
    return m_name;
}

std::size_t shared_memory_backend::size() const noexcept
{
    return m_size;
}

} // namespace mc::shm::detail
