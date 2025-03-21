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
 * @file shared_memory_allocator.h
 * @brief 共享内存分配器，用于在共享内存中分配对象
 */
#ifndef MC_INTERPROCESS_SHARED_MEMORY_ALLOCATOR_H
#define MC_INTERPROCESS_SHARED_MEMORY_ALLOCATOR_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace mc {
namespace interprocess {

class shared_memory;

/**
 * @brief 共享内存分配器，用于在共享内存中分配对象
 * 
 * 这个分配器使用简单的内存池算法，从共享内存中分配内存。
 * 采用了进程间共享的引用计数机制，确保即使在进程崩溃的情况下也能正确回收资源。
 */
class shared_memory_allocator {
public:
    /**
     * @brief 内存块头部结构
     */
    struct memory_block_header {
        std::atomic<size_t> size;       // 块大小
        std::atomic<bool> is_free;      // 是否空闲
        std::atomic<uint32_t> magic;     // 魔数，用于检查块完整性
    };

    /**
     * @brief 共享内存分配器构造函数
     * @param base_addr 共享内存基地址
     * @param size 共享内存总大小
     */
    shared_memory_allocator(void* base_addr, size_t size);

    /**
     * @brief 分配内存
     * @param size 要分配的内存大小
     * @return 分配的内存指针，如果分配失败返回nullptr
     */
    void* allocate(size_t size);

    /**
     * @brief 释放内存
     * @param ptr 要释放的内存指针
     */
    void deallocate(void* ptr);

    /**
     * @brief 类型安全的内存分配
     * @tparam T 要分配的对象类型
     * @return 分配的内存指针，如果分配失败返回nullptr
     */
    template<typename T>
    T* allocate() {
        return static_cast<T*>(allocate(sizeof(T)));
    }

    /**
     * @brief 类型安全的内存分配（数组版本）
     * @tparam T 要分配的对象类型
     * @param n 元素数量
     * @return 分配的内存指针，如果分配失败返回nullptr
     */
    template<typename T>
    T* allocate_array(size_t n) {
        return static_cast<T*>(allocate(sizeof(T) * n));
    }

    /**
     * @brief 类型安全的内存释放
     * @tparam T 要释放的对象类型
     * @param ptr 要释放的内存指针
     */
    template<typename T>
    void deallocate(T* ptr) {
        deallocate(static_cast<void*>(ptr));
    }

    /**
     * @brief 在已分配内存中构造对象
     * @tparam T 要构造的对象类型
     * @tparam Args 构造函数参数类型
     * @param ptr 内存指针
     * @param args 构造函数参数
     * @return 构造的对象指针
     */
    template<typename T, typename... Args>
    T* construct(void* ptr, Args&&... args) {
        return new (ptr) T(std::forward<Args>(args)...);
    }

    /**
     * @brief 析构对象
     * @tparam T 要析构的对象类型
     * @param ptr 对象指针
     */
    template<typename T>
    void destroy(T* ptr) {
        if (ptr) {
            ptr->~T();
        }
    }

    /**
     * @brief 分配并构造对象
     * @tparam T 要构造的对象类型
     * @tparam Args 构造函数参数类型
     * @param args 构造函数参数
     * @return 构造的对象指针，如果分配失败返回nullptr
     */
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocate(sizeof(T));
        if (!mem) {
            return nullptr;
        }
        return construct<T>(mem, std::forward<Args>(args)...);
    }

    /**
     * @brief 分配并构造对象数组
     * @tparam T 要构造的对象类型
     * @param n 元素数量
     * @return 构造的对象数组指针，如果分配失败返回nullptr
     * @note 仅支持默认构造的对象
     */
    template<typename T>
    T* create_array(size_t n) {
        if (n == 0) {
            return nullptr;
        }
        
        void* mem = allocate(sizeof(T) * n);
        if (!mem) {
            return nullptr;
        }
        
        T* array = static_cast<T*>(mem);
        for (size_t i = 0; i < n; ++i) {
            construct<T>(array + i);
        }
        
        return array;
    }

    /**
     * @brief 析构并释放对象
     * @tparam T 要析构的对象类型
     * @param ptr 对象指针
     */
    template<typename T>
    void destroy_and_deallocate(T* ptr) {
        if (ptr) {
            destroy(ptr);
            deallocate(ptr);
        }
    }

    /**
     * @brief 析构并释放对象数组
     * @tparam T 要析构的对象类型
     * @param ptr 对象数组指针
     * @param n 元素数量
     */
    template<typename T>
    void destroy_and_deallocate_array(T* ptr, size_t n) {
        if (ptr) {
            for (size_t i = 0; i < n; ++i) {
                destroy(ptr + i);
            }
            deallocate(ptr);
        }
    }

    /**
     * @brief 获取已分配内存总大小
     * @return 已分配内存总大小
     */
    size_t get_allocated_size() const;

    /**
     * @brief 获取剩余可用内存大小
     * @return 剩余可用内存大小
     */
    size_t get_available_size() const;

    /**
     * @brief 获取共享内存基地址
     * @return 共享内存基地址
     */
    void* get_base_address() const;

    /**
     * @brief 获取共享内存总大小
     * @return 共享内存总大小
     */
    size_t get_total_size() const;

private:
    // 成员变量
    void* m_base_addr;                 // 共享内存基地址
    size_t m_size;                     // 共享内存总大小
    std::atomic<size_t> m_allocated;   // 已分配内存总大小
    
    // 内部方法
    memory_block_header* find_free_block(size_t size);
    memory_block_header* get_header(void* ptr);
    void* get_user_ptr(memory_block_header* header);
    void split_block(memory_block_header* block, size_t size);
    void merge_adjacent_blocks();
};

/**
 * @brief 提供类似std::allocator接口的分配器适配器
 * @tparam T 要分配的对象类型
 */
template <typename T>
class shared_allocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    /**
     * @brief 默认构造函数
     */
    shared_allocator() = delete;
    
    /**
     * @brief 构造函数
     * @param allocator 共享内存分配器
     */
    explicit shared_allocator(shared_memory_allocator& allocator) 
        : m_allocator(allocator) {}
    
    /**
     * @brief 拷贝构造函数
     */
    shared_allocator(const shared_allocator& other) = default;
    
    /**
     * @brief 类型转换构造函数
     * @tparam U 其他类型
     * @param other 其他类型的分配器
     */
    template <typename U>
    shared_allocator(const shared_allocator<U>& other) 
        : m_allocator(other.m_allocator) {}
    
    /**
     * @brief 分配内存
     * @param n 元素数量
     * @return 分配的内存指针
     */
    pointer allocate(size_type n) {
        return static_cast<pointer>(m_allocator.allocate(sizeof(T) * n));
    }
    
    /**
     * @brief 释放内存
     * @param p 内存指针
     * @param n 元素数量
     */
    void deallocate(pointer p, size_type n) {
        m_allocator.deallocate(p);
    }
    
    /**
     * @brief 构造对象
     * @tparam U 对象类型
     * @tparam Args 构造函数参数类型
     * @param p 内存指针
     * @param args 构造函数参数
     */
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        m_allocator.construct<U>(p, std::forward<Args>(args)...);
    }
    
    /**
     * @brief 析构对象
     * @tparam U 对象类型
     * @param p 对象指针
     */
    template <typename U>
    void destroy(U* p) {
        m_allocator.destroy(p);
    }
    
    /**
     * @brief 类型重绑定
     * @tparam U 其他类型
     */
    template <typename U>
    struct rebind {
        using other = shared_allocator<U>;
    };

private:
    shared_memory_allocator& m_allocator;
    
    template <typename U>
    friend class shared_allocator;
};

/**
 * @brief 比较两个分配器是否相等
 * @tparam T 第一个分配器的类型
 * @tparam U 第二个分配器的类型
 * @param lhs 第一个分配器
 * @param rhs 第二个分配器
 * @return 两个分配器是否相等
 */
template <typename T, typename U>
bool operator==(const shared_allocator<T>& lhs, const shared_allocator<U>& rhs) {
    return &lhs.m_allocator == &rhs.m_allocator;
}

/**
 * @brief 比较两个分配器是否不相等
 * @tparam T 第一个分配器的类型
 * @tparam U 第二个分配器的类型
 * @param lhs 第一个分配器
 * @param rhs 第二个分配器
 * @return 两个分配器是否不相等
 */
template <typename T, typename U>
bool operator!=(const shared_allocator<T>& lhs, const shared_allocator<U>& rhs) {
    return !(lhs == rhs);
}

} // namespace interprocess
} // namespace mc

#endif // MC_INTERPROCESS_SHARED_MEMORY_ALLOCATOR_H 