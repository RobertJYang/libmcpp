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

#ifndef MC_MEMORY_OBJECT_POOL_H
#define MC_MEMORY_OBJECT_POOL_H

#include <mc/memory/fixed_size_pool.h>

#include <new>
#include <type_traits>
#include <utility>

namespace mc::memory {

template <typename T>
class object_pool {
public:
    using value_type = T;
    using options    = fixed_size_pool::options;

    object_pool() : m_storage_pool(sizeof(T), alignof(T))
    {}

    explicit object_pool(const options& opts) : m_storage_pool(sizeof(T), alignof(T), opts)
    {}

    object_pool(const object_pool&)            = delete;
    object_pool& operator=(const object_pool&) = delete;
    object_pool(object_pool&&)                 = delete;
    object_pool& operator=(object_pool&&)      = delete;

    template <typename... Args>
    T* create(Args&&... args)
    {
        void* storage = m_storage_pool.allocate();
        try {
            return new (storage) T(std::forward<Args>(args)...);
        } catch (...) {
            m_storage_pool.deallocate(storage);
            throw;
        }
    }

    void destroy(T* object) noexcept
    {
        if (object == nullptr) {
            return;
        }
        object->~T();
        m_storage_pool.deallocate(object);
    }

    void reserve(std::size_t slot_count)
    {
        m_storage_pool.reserve(slot_count);
    }

    std::size_t trim(std::size_t keep_free_slots = 0)
    {
        return m_storage_pool.trim(keep_free_slots);
    }

private:
    fixed_size_pool m_storage_pool;
};

} // namespace mc::memory

#endif // MC_MEMORY_OBJECT_POOL_H
