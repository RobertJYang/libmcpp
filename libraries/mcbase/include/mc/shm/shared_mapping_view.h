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

#ifndef MC_SHM_SHARED_MAPPING_VIEW_H
#define MC_SHM_SHARED_MAPPING_VIEW_H

#include <cstddef>

#include <mc/common.h>
#include <mc/string.h>
#include <mc/string_view.h>
#include <mc/shm/detail/shared_memory_backend.h>

namespace mc::shm {

class MC_API shared_mapping_view {
public:
    shared_mapping_view() noexcept;
    shared_mapping_view(mc::string_view name, std::size_t size, detail::open_mode mode);
    ~shared_mapping_view();

    shared_mapping_view(const shared_mapping_view&)            = delete;
    shared_mapping_view& operator=(const shared_mapping_view&) = delete;

    shared_mapping_view(shared_mapping_view&& other) noexcept;
    shared_mapping_view& operator=(shared_mapping_view&& other) noexcept;

    bool               is_valid() const noexcept;
    bool               created() const noexcept;
    void*              data() const noexcept;
    std::size_t        size() const noexcept;
    const mc::string&  name() const noexcept;

private:
    detail::shared_memory_backend m_backend;
    void*                         m_address;
    std::size_t                   m_size;
};

} // namespace mc::shm

#endif // MC_SHM_SHARED_MAPPING_VIEW_H
