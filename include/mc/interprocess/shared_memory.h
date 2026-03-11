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
 * @file shared_memory.h
 * @brief 共享内存管理类，负责共享内存的分配和管理
 */
#ifndef MC_INTERPROCESS_SHARED_MEMORY_H
#define MC_INTERPROCESS_SHARED_MEMORY_H

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include <mc/interprocess/shared_memory_allocator.h>

namespace mc {
namespace interprocess {

template <typename T>
class offset_ptr;
/**
 * @brief 共享内存管理类，负责创建、打开和管理共享内存
 */
class MC_API shared_memory {
public:
    /**
     * @brief 常量定义
     */
    static constexpr uint32_t HEADER_MAGIC    = 0x4D434442; // "MCDB"
    static constexpr uint32_t HEADER_VERSION  = 1;
    static constexpr size_t   MIN_MEMORY_SIZE = 1 * 1024; // 1KB

    /**
     * @brief 格式化共享内存名称，确保名称带有"/"前缀
     * @param name 原始共享内存名称
     * @return 格式化后的共享内存名称
     */
    static std::string format_shm_name(const std::string& name);

    /**
     * @brief 创建或打开共享内存
     * @param name 共享内存名称，用于标识共享内存段
     * @param size 共享内存大小，只在首次创建时生效
     * @return 共享内存管理对象，如果创建失败返回nullptr
     */
    static std::shared_ptr<shared_memory> create(const std::string& name, size_t size);

    /**
     * @brief 析构函数，释放共享内存资源
     */
    ~shared_memory();

    /**
     * @brief 获取共享内存分配器
     * @return 共享内存分配器引用
     */
    shared_memory_allocator& get_allocator();

    /**
     * @brief 获取共享内存名称
     * @return 共享内存名称
     */
    std::string_view get_name() const;

    /**
     * @brief 获取共享内存大小
     * @return 共享内存大小
     */
    size_t get_size() const;

    /**
     * @brief 获取共享内存ID
     * @return 共享内存ID
     */
    int get_shmid() const;

    /**
     * @brief 获取共享内存基地址
     * @return 共享内存基地址
     */
    void* get_address() const;

    /**
     * @brief 检查共享内存是否有效
     * @return 共享内存是否有效
     */
    bool is_valid() const;

    /**
     * @brief 计算指针相对于共享内存基地址的偏移量
     * @param ptr 指针
     * @return 偏移量，如果指针不在共享内存范围内则返回0
     */
    size_t get_offset(const void* ptr) const;

    /**
     * @brief 从偏移量计算出指针
     * @param offset 偏移量
     * @return 指针，如果偏移量无效则返回nullptr
     */
    void* get_ptr_from_offset(size_t offset) const;

    /**
     * @brief 获取数据区域的起始地址（跳过头部）
     * @return 数据区域的起始地址
     */
    void* get_data_address() const;

    /**
     * @brief 获取数据区域的大小
     * @return 数据区域的大小
     */
    size_t get_data_size() const;

    /**
     * @brief 创建偏移指针
     * @tparam T 指针类型
     * @param ptr 真实指针
     * @return 偏移指针
     */
    template <typename T>
    offset_ptr<T> make_offset_ptr(T* ptr) const
    {
        return offset_ptr<T>(this, ptr);
    }

    /**
     * @brief 创建偏移指针
     * @tparam T 指针类型
     * @param offset 偏移量
     * @return 偏移指针
     */
    template <typename T>
    offset_ptr<T> make_offset_ptr(size_t offset) const
    {
        return offset_ptr<T>(this, offset);
    }

    /**
     * @brief 创建偏移指针
     * @tparam T 指针类型
     * @tparam U 偏移指针类型
     * @param other 偏移指针
     * @return 偏移指针
     */
    template <typename T>
    offset_ptr<T> make_offset_ptr(offset_ptr<T> other) const
    {
        return offset_ptr<T>(this, other.get_offset());
    }

private:
    /**
     * @brief 构造函数
     * @param name 共享内存名称
     * @param fd 文件描述符
     * @param addr 共享内存基地址
     * @param size 共享内存大小
     */
    shared_memory(std::string name, int fd, void* addr, size_t size);

    // 禁止拷贝构造和赋值
    shared_memory(const shared_memory&)            = delete;
    shared_memory& operator=(const shared_memory&) = delete;

    // 内部方法
    bool init_memory();
    bool register_process();
    void unregister_process();

    // 共享内存头部结构，存储在共享内存的开始位置
    struct shared_memory_header {
        std::atomic<uint32_t> magic;                // 魔数，用于检查完整性
        std::atomic<uint32_t> version;              // 版本号
        std::atomic<size_t>   total_size;           // 共享内存总大小
        std::atomic<size_t>   used_size;            // 已使用大小
        std::atomic<pid_t>    creator_pid;          // 创建者进程ID
        std::atomic<size_t>   process_count;        // 使用进程数量
        std::atomic<size_t>   active_processes[64]; // 活跃进程列表，存储pid
    };

    // 成员变量
    std::string             m_name;         // 共享内存名称
    int                     m_fd;           // 文件描述符
    void*                   m_addr;         // 共享内存基地址
    size_t                  m_size;         // 共享内存大小
    shared_memory_header*   m_header;       // 共享内存头部
    shared_memory_allocator m_allocator;    // 共享内存分配器
    size_t                  m_process_slot; // 当前进程在活跃进程列表中的位置

    // 声明友元类，使offset_ptr可以访问shared_memory的私有成员
    template <typename T>
    friend class offset_ptr;
};

/**
 * @brief 共享内存引用计数器，用于跨进程引用计数
 */
class shared_ref_counter {
public:
    /**
     * @brief 创建引用计数器
     * @param allocator 共享内存分配器
     * @return 引用计数器指针，如果创建失败返回nullptr
     */
    static std::shared_ptr<shared_ref_counter> create(shared_memory_allocator& allocator);

    /**
     * @brief 析构函数
     */
    ~shared_ref_counter();

    /**
     * @brief 增加引用计数
     * @return 增加后的引用计数
     */
    size_t add_ref();

    /**
     * @brief 减少引用计数
     * @return 减少后的引用计数
     */
    size_t release();

    /**
     * @brief 获取当前引用计数
     * @return 当前引用计数
     */
    size_t get_count() const;

    /**
     * @brief 注册当前进程到引用计数器
     * @return 注册成功返回true，失败返回false
     */
    bool register_process();

    /**
     * @brief 从引用计数器中注销当前进程
     * @return 注销成功返回true，失败返回false
     */
    bool unregister_process();

    /**
     * @brief 检查进程是否仍然活跃
     * @param pid 进程ID
     * @return 如果进程仍然活跃则返回true，否则返回false
     */
    static bool is_process_alive(pid_t pid);

private:
    /**
     * @brief 构造函数
     * @param allocator 共享内存分配器
     * @param counter_addr 计数器地址
     */
    shared_ref_counter(shared_memory_allocator& allocator, void* counter_addr);

    // 引用计数器结构
    struct ref_count {
        std::atomic<size_t> count;             // 总引用计数
        std::atomic<size_t> process_count[64]; // 每个进程的引用计数
        std::atomic<pid_t>  process_pids[64];  // 进程ID列表
    };

    // 成员变量
    shared_memory_allocator& m_allocator;    // 共享内存分配器
    ref_count*               m_counter;      // 引用计数器地址
    size_t                   m_process_slot; // 当前进程在进程列表中的位置
};

/**
 * @brief 共享指针类，用于在共享内存中管理对象生命周期
 * @tparam T 对象类型
 */
template <typename T>
class shared_ptr {
public:
    /**
     * @brief 默认构造函数，创建空指针
     */
    shared_ptr()
        : m_ptr(nullptr), m_counter(nullptr)
    {
    }

    /**
     * @brief 构造函数，从普通指针创建共享指针
     * @param ptr 对象指针
     * @param counter 引用计数器
     */
    shared_ptr(T* ptr, std::shared_ptr<shared_ref_counter> counter)
        : m_ptr(ptr), m_counter(counter)
    {
        if (m_ptr && m_counter) {
            m_counter->add_ref();
        }
    }

    /**
     * @brief 拷贝构造函数
     * @param other 其他共享指针
     */
    shared_ptr(const shared_ptr& other)
        : m_ptr(other.m_ptr), m_counter(other.m_counter)
    {
        if (m_ptr && m_counter) {
            m_counter->add_ref();
        }
    }

    /**
     * @brief 移动构造函数
     * @param other 其他共享指针
     */
    shared_ptr(shared_ptr&& other) noexcept
        : m_ptr(other.m_ptr), m_counter(std::move(other.m_counter))
    {
        other.m_ptr = nullptr;
    }

    /**
     * @brief 析构函数
     */
    ~shared_ptr()
    {
        reset();
    }

    /**
     * @brief 拷贝赋值运算符
     * @param other 其他共享指针
     * @return 共享指针引用
     */
    shared_ptr& operator=(const shared_ptr& other)
    {
        if (this != &other) {
            reset();
            m_ptr     = other.m_ptr;
            m_counter = other.m_counter;
            if (m_ptr && m_counter) {
                m_counter->add_ref();
            }
        }
        return *this;
    }

    /**
     * @brief 移动赋值运算符
     * @param other 其他共享指针
     * @return 共享指针引用
     */
    shared_ptr& operator=(shared_ptr&& other) noexcept
    {
        if (this != &other) {
            reset();
            m_ptr       = other.m_ptr;
            m_counter   = std::move(other.m_counter);
            other.m_ptr = nullptr;
        }
        return *this;
    }

    /**
     * @brief 重置指针
     */
    void reset()
    {
        if (m_ptr && m_counter) {
            if (m_counter->release() == 0) {
                m_ptr->~T();
                // 内存释放由引用计数器处理
            }
            m_ptr     = nullptr;
            m_counter = nullptr;
        }
    }

    /**
     * @brief 获取原始指针
     * @return 原始指针
     */
    T* get() const
    {
        return m_ptr;
    }

    /**
     * @brief 解引用运算符
     * @return 对象引用
     */
    T& operator*() const
    {
        return *m_ptr;
    }

    /**
     * @brief 箭头运算符
     * @return 对象指针
     */
    T* operator->() const
    {
        return m_ptr;
    }

    /**
     * @brief 布尔运算符
     * @return 如果指针不为空则返回true，否则返回false
     */
    explicit operator bool() const
    {
        return m_ptr != nullptr;
    }

    /**
     * @brief 获取引用计数
     * @return 当前引用计数
     */
    size_t use_count() const
    {
        return m_counter ? m_counter->get_count() : 0;
    }

private:
    T*                                  m_ptr;     // 对象指针
    std::shared_ptr<shared_ref_counter> m_counter; // 引用计数器
};

/**
 * @brief 在共享内存中创建对象
 * @tparam T 对象类型
 * @tparam Args 构造函数参数类型
 * @param allocator 共享内存分配器
 * @param args 构造函数参数
 * @return 共享指针
 */
template <typename T, typename... Args>
shared_ptr<T> make_shared(shared_memory_allocator& allocator, Args&&... args)
{
    // 分配内存
    void* mem = allocator.allocate(sizeof(T));
    if (!mem) {
        return shared_ptr<T>();
    }

    // 创建引用计数器
    auto counter = shared_ref_counter::create(allocator);
    if (!counter) {
        allocator.deallocate(mem);
        return shared_ptr<T>();
    }

    // 在分配的内存上构造对象
    T* ptr = new (mem) T(std::forward<Args>(args)...);

    // 创建并返回共享指针
    return shared_ptr<T>(ptr, counter);
}

} // namespace interprocess
} // namespace mc

#include "mc/interprocess/offset_ptr.h"

#endif // MC_INTERPROCESS_SHARED_MEMORY_H