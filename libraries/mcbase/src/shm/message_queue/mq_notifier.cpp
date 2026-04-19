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

#include "mq_notifier.h"

#include "mq_private.h"

#include <cctype>
#include <cerrno>
#include <limits>

#ifdef _WIN32
#include <algorithm>
#else
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace mc::shm::detail {
namespace {

mc::string sanitize_name(mc::string_view value)
{
    mc::string result;
    result.reserve(value.size());
    for (const unsigned char ch : value) {
        result.push_back(std::isalnum(ch) ? static_cast<char>(ch) : '_');
    }
    if (result.empty()) {
        result = "default";
    }
    return result;
}

} // namespace

mq_notifier::mq_notifier() noexcept = default;

mq_notifier::mq_notifier(mc::string_view name) : m_impl(std::make_shared<impl>())
{
    if (name.empty()) {
        m_impl->name = make_default_name("default");
    } else {
        m_impl->name = mc::string(name);
    }

#ifdef _WIN32
    m_impl->handle = CreateEventA(nullptr, TRUE, FALSE, m_impl->name.c_str());
    if (m_impl->handle == nullptr) {
        m_impl.reset();
    }
#else
    if (mkfifo(m_impl->name.c_str(), 0666) != 0 && errno != EEXIST) {
        m_impl.reset();
        return;
    }

    m_impl->fd = ::open(m_impl->name.c_str(), O_RDWR | O_NONBLOCK);
    if (m_impl->fd < 0) {
        m_impl.reset();
    }
#endif
}

mq_notifier::~mq_notifier()
{
    if (!is_valid()) {
        return;
    }

#ifdef _WIN32
    CloseHandle(m_impl->handle);
#else
    close(m_impl->fd);
#endif
}

mq_notifier::mq_notifier(mq_notifier&& other) noexcept : m_impl(std::move(other.m_impl))
{}

mq_notifier& mq_notifier::operator=(mq_notifier&& other) noexcept
{
    if (this != &other) {
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

bool mq_notifier::is_valid() const noexcept
{
    return m_impl != nullptr;
}

const mc::string& mq_notifier::name() const noexcept
{
    static const mc::string empty;
    return is_valid() ? m_impl->name : empty;
}

bool mq_notifier::notify() const noexcept
{
    if (!is_valid()) {
        return false;
    }

#ifdef _WIN32
    return SetEvent(m_impl->handle) != 0;
#else
    const std::uint8_t marker = 1;
    const auto         rc     = write(m_impl->fd, &marker, sizeof(marker));
    return rc == static_cast<ssize_t>(sizeof(marker)) || errno == EAGAIN;
#endif
}

void mq_notifier::drain() const noexcept
{
    if (!is_valid()) {
        return;
    }

#ifdef _WIN32
    ResetEvent(m_impl->handle);
#else
    std::uint8_t buffer[64];
    while (read(m_impl->fd, buffer, sizeof(buffer)) > 0) {
    }
#endif
}

#ifndef _WIN32
int mq_notifier::native_handle() const noexcept
{
    return is_valid() ? m_impl->fd : -1;
}
#endif

mc::string mq_notifier::make_default_name(mc::string_view queue_name)
{
#ifdef _WIN32
    return "Global\\mc_shm_queue_" + sanitize_name(queue_name);
#else
    return "/tmp/mc_shm_queue_" + sanitize_name(queue_name) + ".fifo";
#endif
}

mc::string mq_notifier::make_space_name(mc::string_view queue_name)
{
#ifdef _WIN32
    return "Global\\mc_shm_queue_space_" + sanitize_name(queue_name);
#else
    return "/tmp/mc_shm_queue_space_" + sanitize_name(queue_name) + ".fifo";
#endif
}

bool mq_notifier::remove(mc::string_view name) noexcept
{
    if (name.empty()) {
        return false;
    }

    const mc::string owned_name(name);

#ifdef _WIN32
    HANDLE handle = OpenEventA(EVENT_MODIFY_STATE, FALSE, owned_name.c_str());
    if (handle != nullptr) {
        CloseHandle(handle);
        return true;
    }
    return GetLastError() == ERROR_FILE_NOT_FOUND;
#else
    if (unlink(owned_name.c_str()) == 0) {
        return true;
    }
    return errno == ENOENT;
#endif
}

bool mq_notifier::wait_until(std::chrono::steady_clock::time_point deadline) const noexcept
{
    if (!is_valid()) {
        return false;
    }

#ifdef _WIN32
    const auto now = std::chrono::steady_clock::now();
    const auto timeout =
        deadline <= now
            ? 0UL
            : static_cast<unsigned long>(std::min<std::chrono::milliseconds>(
                                             std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now),
                                             std::chrono::milliseconds(static_cast<long long>(INFINITE - 1)))
                                             .count());
    return WaitForSingleObject(m_impl->handle, timeout) == WAIT_OBJECT_0;
#else
    const auto now = std::chrono::steady_clock::now();
    const auto timeout =
        deadline <= now ? 0
                        : static_cast<int>(std::min<std::chrono::milliseconds>(
                                               std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now),
                                               std::chrono::milliseconds(std::numeric_limits<int>::max()))
                                               .count());

    struct pollfd fd;
    fd.fd      = m_impl->fd;
    fd.events  = POLLIN;
    fd.revents = 0;
    return ::poll(&fd, 1, timeout) > 0;
#endif
}

} // namespace mc::shm::detail
