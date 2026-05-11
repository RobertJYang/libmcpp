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

#ifndef MC_RUNTIME_DETAIL_ALIGNED_BUFFER_H
#define MC_RUNTIME_DETAIL_ALIGNED_BUFFER_H

#include <cstddef>
#include <new>
#include <type_traits>

namespace mc::runtime::detail {

/// @brief 固定大小的对齐原始内存缓冲区
/// 不负责对象的构造与析构，仅提供类型安全的内存访问接口。
template <std::size_t StorageSize, std::size_t StorageAlign = alignof(std::max_align_t)>
class aligned_buffer {
public:
    void* data() noexcept
    {
        return &m_storage;
    }

    const void* data() const noexcept
    {
        return &m_storage;
    }

    template <typename T>
    T* get() noexcept
    {
        return std::launder(reinterpret_cast<T*>(&m_storage));
    }

    template <typename T>
    const T* get() const noexcept
    {
        return std::launder(reinterpret_cast<const T*>(&m_storage));
    }

private:
    typename std::aligned_storage<StorageSize, StorageAlign>::type m_storage;
};

} // namespace mc::runtime::detail

#endif // MC_RUNTIME_DETAIL_ALIGNED_BUFFER_H
