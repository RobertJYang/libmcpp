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
 * @file offset_ptr.h
 * @brief 偏移指针类，用于在多进程共享内存中安全传递指针
 */
#ifndef MC_INTERPROCESS_OFFSET_PTR_H
#define MC_INTERPROCESS_OFFSET_PTR_H

#include <cstddef>
#include <stdexcept>

namespace mc {
namespace interprocess {
class shared_memory;

/**
 * @brief 偏移指针类，用于在多进程间安全传递共享内存中的指针
 *
 * 该类存储的是相对于共享内存基地址的偏移量，而不是真实指针，
 * 因此可以在不同进程间安全传递，到达目标进程后自动转换为真实指针。
 *
 * @tparam T 指针指向的类型
 */
template <typename T>
class offset_ptr {
public:
    /**
     * @brief 默认构造函数，创建空指针
     */
    offset_ptr() noexcept : m_offset(0), m_shm(nullptr)
    {}

    /**
     * @brief 从共享内存和真实指针构造
     * @param shm 共享内存对象
     * @param ptr 真实指针
     */
    offset_ptr(const shared_memory* shm, T* ptr) noexcept;

    /**
     * @brief 从共享内存和偏移量构造
     * @param shm 共享内存对象
     * @param offset 相对于共享内存基地址的偏移量
     */
    offset_ptr(const shared_memory* shm, size_t offset) noexcept;

    /**
     * @brief 拷贝构造函数
     */
    offset_ptr(const offset_ptr& other) noexcept = default;

    /**
     * @brief 移动构造函数
     */
    offset_ptr(offset_ptr&& other) noexcept = default;

    /**
     * @brief 拷贝赋值运算符
     */
    offset_ptr& operator=(const offset_ptr& other) noexcept = default;

    /**
     * @brief 移动赋值运算符
     */
    offset_ptr& operator=(offset_ptr&& other) noexcept = default;

    /**
     * @brief 解引用操作符
     * @return 对象引用
     */
    T& operator*() const;

    /**
     * @brief 箭头操作符
     * @return 真实指针
     */
    T* operator->() const;

    /**
     * @brief 获取真实指针
     * @return 真实指针
     */
    T* get() const;

    /**
     * @brief 转换为bool类型，检查指针是否为空
     * @return 如果指针不为空则返回true
     */
    explicit operator bool() const noexcept;

    /**
     * @brief 重置指针
     */
    void reset() noexcept;

    /**
     * @brief 获取偏移量
     * @return 相对于共享内存基地址的偏移量
     */
    size_t get_offset() const noexcept
    {
        return m_offset;
    }

    /**
     * @brief 获取共享内存对象
     * @return 共享内存对象指针
     */
    const shared_memory* get_shared_memory() const noexcept
    {
        return m_shm;
    }

private:
    size_t               m_offset; // 偏移量
    const shared_memory* m_shm;    // 共享内存对象
};

// offset_ptr模板类的实现
template <typename T>
offset_ptr<T>::offset_ptr(const shared_memory* shm, T* ptr) noexcept : m_offset(0), m_shm(shm)
{
    if (shm && ptr) {
        m_offset = shm->get_offset(ptr);
    }
}

template <typename T>
offset_ptr<T>::offset_ptr(const shared_memory* shm, size_t offset) noexcept : m_offset(offset), m_shm(shm)
{}

template <typename T>
T& offset_ptr<T>::operator*() const
{
    T* ptr = get();
    if (!ptr) {
        throw std::runtime_error("无效的偏移指针");
    }
    return *ptr;
}

template <typename T>
T* offset_ptr<T>::operator->() const
{
    return get();
}

template <typename T>
T* offset_ptr<T>::get() const
{
    if (!m_shm || m_offset == 0) {
        return nullptr;
    }
    return static_cast<T*>(m_shm->get_ptr_from_offset(m_offset));
}

template <typename T>
offset_ptr<T>::operator bool() const noexcept
{
    return m_shm && m_offset > 0;
}

template <typename T>
void offset_ptr<T>::reset() noexcept
{
    m_offset = 0;
    m_shm    = nullptr;
}

} // namespace interprocess
} // namespace mc

#endif // MC_INTERPROCESS_OFFSET_PTR_H