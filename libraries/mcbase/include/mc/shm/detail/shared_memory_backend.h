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

#ifndef MC_SHM_DETAIL_SHARED_MEMORY_BACKEND_H
#define MC_SHM_DETAIL_SHARED_MEMORY_BACKEND_H

#include <cstddef>

#include <mc/common.h>
#include <mc/string.h>
#include <mc/string_view.h>

namespace mc::shm::detail {

enum class open_mode {
    open_existing,
    create_only,
    create_or_open,
};

class MC_API shared_memory_backend {
public:
    shared_memory_backend() noexcept;
    shared_memory_backend(mc::string name, int fd, bool created) noexcept;
    ~shared_memory_backend();

    shared_memory_backend(const shared_memory_backend&)            = delete;
    shared_memory_backend& operator=(const shared_memory_backend&) = delete;

    shared_memory_backend(shared_memory_backend&& other) noexcept;
    shared_memory_backend& operator=(shared_memory_backend&& other) noexcept;

    static shared_memory_backend open(mc::string_view name, std::size_t size, open_mode mode);
    static bool                  remove(mc::string_view name) noexcept;
    static mc::string            format_name(mc::string_view name);

    bool               is_valid() const noexcept;
    int                fd() const noexcept;
    bool               created() const noexcept;
    const mc::string&  name() const noexcept;
    std::size_t        size() const noexcept;

private:
    mc::string  m_name;
    int         m_fd;
    bool        m_created;
    std::size_t m_size;
};

} // namespace mc::shm::detail

#endif // MC_SHM_DETAIL_SHARED_MEMORY_BACKEND_H
