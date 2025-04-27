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

#ifndef MC_IO_BUFFER_H
#define MC_IO_BUFFER_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include <mc/common.h>
#include <mc/exception.h>

// 默认分配内存的对齐大小
#define IO_BUFFER_DEFAULT_ALIGNMENT 8

namespace mc {
namespace io {
namespace detail {

class shard_buffer {
public:
    using free_function = void (*)(void* buf, void* user_data);

    shard_buffer();
    shard_buffer(const shard_buffer& other);
    shard_buffer& operator=(const shard_buffer& other);
    shard_buffer(shard_buffer&& other) noexcept;
    shard_buffer& operator=(shard_buffer&& other) noexcept;
    ~shard_buffer();

    shard_buffer(void* buf, free_function fn, void* user_data);

    uint8_t* data() const noexcept {
        return m_buffer;
    }

    bool is_shared() const noexcept;
    void unshare();

private:
    void ensure_shared_info();
    void share_from(const shard_buffer& other);
    void free();

    struct shared_info {
        shared_info() : ref_count(1) {
        }
        std::atomic<uint32_t> ref_count;
    };

    shared_info*  m_shared_info{nullptr};
    uint8_t*      m_buffer{nullptr};
    free_function m_free_fn;
    void*         m_user_data;
};

} // namespace detail

/**
 * @brief 内存缓冲区管理类
 *
 * io_buffer 提供对内存缓冲区的高效管理，支持如下功能：
 * - 支持引用计数共享底层缓冲区
 * - 支持缓冲区链接形成链表结构
 * - 支持自定义内存释放函数
 * - 支持预留头部和尾部空间
 * - 支持对齐操作满足特定协议需求
 *
 * 内存布局如下:
 * +------------+--------------------+-----------+
 * | headroom   |        data        |  tailroom |
 * +------------+--------------------+-----------+
 * ^            ^                    ^           ^
 * buffer()   data()               tail()      buffer_end()
 */
class io_buffer {
public:
    using free_function = detail::shard_buffer::free_function;
    static void default_free(void* buf, void*) {
        std::free(buf);
    };

    /**
     * @brief 默认构造函数，创建一个空的缓冲区
     */
    io_buffer() noexcept;

    /**
     * @brief 移动构造函数
     */
    io_buffer(io_buffer&& other) noexcept;

    /**
     * @brief 移动赋值运算符
     */
    io_buffer& operator=(io_buffer&& other) noexcept;

    /**
     * @brief 复制构造函数
     *
     * 创建一个共享同一数据的新 io_buffer
     */
    io_buffer(const io_buffer& other);

    /**
     * @brief 复制赋值运算符
     */
    io_buffer& operator=(const io_buffer& other);

    /**
     * @brief 析构函数
     *
     * 释放所有资源并处理引用计数
     */
    ~io_buffer();

    /**
     * @brief 创建指定容量的缓冲区
     *
     * @param capacity 需要分配的容量大小
     * @return 创建的 io_buffer 对象指针
     */
    static std::unique_ptr<io_buffer> create(std::size_t capacity);

    /**
     * @brief 接管外部缓冲区的所有权
     *
     * @param buf 外部缓冲区指针
     * @param capacity 缓冲区容量
     * @param length 有效数据长度
     * @param free_fn 自定义释放函数
     * @param user_data 传递给释放函数的用户数据
     * @return 创建的 io_buffer 对象指针
     */
    static std::unique_ptr<io_buffer> take_ownership(void* buf, std::size_t capacity,
                                                     std::size_t   length    = 0,
                                                     free_function free_fn   = nullptr,
                                                     void*         user_data = nullptr);

    /**
     * @brief 封装外部缓冲区但不获取所有权
     *
     * @param buf 外部缓冲区指针
     * @param length 有效数据长度
     * @return 创建的 io_buffer 对象指针
     */
    static std::unique_ptr<io_buffer> wrap(const void* buf, std::size_t length);

    /**
     * @brief 复制数据到新的缓冲区
     *
     * @param data 要复制的数据
     * @param length 数据长度
     * @param headroom 预留的头部空间
     * @param tailroom 预留的尾部空间
     * @return 创建的 io_buffer 对象指针
     */
    static std::unique_ptr<io_buffer> copy_buffer(const void* data, std::size_t length,
                                                  std::size_t headroom = 0,
                                                  std::size_t tailroom = 0);

    /**
     * @brief 获取当前有效数据指针
     *
     * @return 有效数据的起始指针
     */
    const uint8_t* data() const noexcept {
        return m_data;
    }

    /**
     * @brief 获取可写的数据指针
     *
     * @return 可写的数据指针
     */
    uint8_t* mutable_data() noexcept {
        return m_data;
    }

    /**
     * @brief 获取有效数据长度
     *
     * @return 有效数据的长度
     */
    std::size_t length() const noexcept {
        return m_length;
    }

    /**
     * @brief 获取缓冲区起始位置
     *
     * @return 缓冲区的起始指针
     */
    const uint8_t* buffer() const noexcept {
        return m_buffer.data();
    }

    /**
     * @brief 获取缓冲区总容量
     *
     * @return 缓冲区的总容量
     */
    std::size_t capacity() const noexcept {
        return m_capacity;
    }

    /**
     * @brief 获取有效数据尾部指针
     *
     * @return 有效数据的尾部指针
     */
    const uint8_t* tail() const noexcept {
        return m_data + m_length;
    }

    /**
     * @brief 获取可写的尾部指针
     *
     * @return 可写的尾部指针
     */
    uint8_t* mutable_tail() noexcept {
        return m_data + m_length;
    }

    /**
     * @brief 获取缓冲区结束位置
     *
     * @return 缓冲区的结束指针
     */
    const uint8_t* buffer_end() const noexcept {
        return m_buffer.data() + m_capacity;
    }

    /**
     * @brief 获取头部预留空间大小
     *
     * @return 头部预留空间大小
     */
    std::size_t headroom() const noexcept {
        return m_data - m_buffer.data();
    }

    /**
     * @brief 获取尾部可用空间大小
     *
     * @return 尾部可用空间大小
     */
    std::size_t tailroom() const noexcept {
        return m_capacity - (m_data - m_buffer.data()) - m_length;
    }

    /**
     * @brief 判断缓冲区是否为空
     *
     * @return 如果缓冲区为空则返回true
     */
    bool empty() const noexcept {
        return m_length == 0;
    }

    /**
     * @brief 判断缓冲区是否被共享
     *
     * @return 如果缓冲区被共享则返回true
     */
    bool is_shared() const noexcept;

    /**
     * @brief 在尾部追加空间
     *
     * @param amount 要追加的字节数
     */
    void append(std::size_t amount) noexcept;

    /**
     * @brief 在头部预留空间
     *
     * @param amount 要预留的字节数
     */
    void prepend(std::size_t amount) noexcept;

    /**
     * @brief 截断尾部数据
     *
     * @param amount 要截断的字节数
     */
    void trim_end(std::size_t amount) noexcept;

    /**
     * @brief 截断头部数据
     *
     * @param amount 要截断的字节数
     */
    void trim_start(std::size_t amount) noexcept;

    /**
     * @brief 清空数据
     */
    void clear() noexcept;

    /**
     * @brief 确保缓冲区拥有足够的头部和尾部空间
     *
     * @param min_headroom 最小头部空间
     * @param min_tailroom 最小尾部空间
     */
    void reserve(std::size_t min_headroom, std::size_t min_tailroom);

    /**
     * @brief 数据对齐到指定边界
     *
     * @param alignment 对齐边界（通常是1,2,4或8）
     * @return 实际填充的字节数
     */
    std::size_t align(std::size_t alignment);

    /**
     * @brief 写入数据到缓冲区尾部
     *
     * @param data 数据指针
     * @param length 数据长度
     * @return 实际写入的字节数
     */
    std::size_t write(const void* data, std::size_t length);

    /**
     * @brief 在指定位置写入数据
     *
     * @param offset 偏移位置
     * @param data 要写入的数据
     * @param length 数据长度
     * @return 实际写入的字节数
     */
    std::size_t write_at_offset(std::size_t offset, const void* data, std::size_t length);

    /**
     * @brief 按照指定字节序写入基本数据类型
     *
     * @tparam T 数据类型
     * @param value 要写入的值
     * @param is_little_endian 是否为小端字节序
     * @return 实际写入的字节数
     */
    template <typename T,
              typename std::enable_if<std::is_trivially_copyable<T>::value, int>::type = 0>
    std::size_t write_value(T value, bool is_little_endian = mc::is_little_endian()) {
        if constexpr (sizeof(T) > 1 && std::is_arithmetic_v<T>) {
            if (is_little_endian != mc::is_little_endian()) {
                value = mc::swap_bytes(value);
            }
        }

        return write(&value, sizeof(T));
    }

    /**
     * @brief 按照指定字节序写入基本数据类型
     *
     * @tparam T 数据类型
     * @param offset 偏移位置
     * @param value 要写入的值
     * @param is_little_endian 是否为小端字节序
     * @return 实际写入的字节数
     */
    template <typename T,
              typename std::enable_if<std::is_trivially_copyable<T>::value, int>::type = 0>
    std::size_t write_value_at_offset(std::size_t offset, T value,
                                      bool is_little_endian = mc::is_little_endian()) {
        if constexpr (sizeof(T) > 1 && std::is_arithmetic_v<T>) {
            if (is_little_endian != mc::is_little_endian()) {
                value = mc::swap_bytes(value);
            }
        }

        return write_at_offset(offset, &value, sizeof(T));
    }

    /**
     * @brief 从指定位置读取数据，如果实际数据长度小于请求的长度，则抛出异常
     *
     * @param offset 偏移位置
     * @param length 数据长度
     * @return 读取的数据
     */
    std::string_view read(std::size_t offset, std::size_t length) const;

    /**
     * @brief 从指定位置读取数据，如果实际数据长度小于请求的长度，则抛出异常
     *
     * @param offset 偏移位置
     * @param data 数据指针
     * @param length 数据长度
     * @return 实际读取的数据
     */
    std::size_t read(std::size_t offset, void* data, std::size_t length) const;

    /**
     * @brief 从指定位置读取数据，如果实际数据长度小于请求的长度，则返回false
     *
     * @param offset 偏移位置
     * @param length 数据长度
     * @return 是否读取成功
     */
    bool try_read(std::size_t offset, void* data, std::size_t length) const noexcept;

    /**
     * @brief 从指定位置读取数据，如果实际数据长度小于请求的长度，则返回空
     *
     * @param offset 偏移位置
     * @param length 数据长度
     * @return 读取的数据
     */
    std::string_view try_read(std::size_t offset, std::size_t length) const noexcept;

    /**
     * @brief 从指定位置读取一些数据，实际读取的字节数可能小于请求的长度
     *
     * @param offset 偏移位置
     * @param length 数据长度
     * @return 实际读取的数据
     */
    std::string_view read_some(std::size_t offset, std::size_t length) const;

    /**
     * @brief 从指定位置读取一些数据，实际读取的字节数可能小于请求的长度
     *
     * @param offset 偏移位置
     * @param data 数据指针
     * @param length 数据长度
     * @return 实际读取的字节数
     */
    std::size_t read_some(std::size_t offset, void* data, std::size_t length) const;

    /**
     * @brief 从指定位置读取基本数据类型，如果实际数据长度小于请求的长度，则抛出异常
     *
     * @tparam T 数据类型
     * @param offset 偏移位置
     * @param is_little_endian 是否为小端字节序
     * @return 读取的值
     */
    template <typename T,
              typename std::enable_if<std::is_trivially_copyable<T>::value, int>::type = 0>
    T read_value(std::size_t offset, bool is_little_endian = mc::is_little_endian()) const {
        T value{};
        read(offset, &value, sizeof(T));

        if constexpr (sizeof(T) > 1 && std::is_arithmetic_v<T>) {
            if (is_little_endian != mc::is_little_endian()) {
                value = mc::swap_bytes(value);
            }
        }

        return value;
    }

    template <typename T,
              typename std::enable_if<std::is_trivially_copyable<T>::value, int>::type = 0>
    bool read_value(std::size_t offset, T& value,
                    bool is_little_endian = mc::is_little_endian()) const {
        if (!try_read(offset, &value, sizeof(T))) {
            return false;
        }

        if constexpr (sizeof(T) > 1 && std::is_arithmetic_v<T>) {
            if (is_little_endian != mc::is_little_endian()) {
                value = mc::swap_bytes(value);
            }
        }

        return true;
    }

    /**
     * @brief 确保尾部有足够空间，必要时扩容
     *
     * @param amount 需要的空间大小
     * @return 是否成功确保空间
     */
    bool ensure_tailroom(std::size_t amount);

    /**
     * @brief 克隆当前缓冲区
     *
     * @return 克隆的io_buffer对象
     */
    std::unique_ptr<io_buffer> clone() const;

    /**
     * @brief 确保缓冲区不被共享，必要时创建副本
     */
    void unshare();

    /**
     * @brief 判断是否为链表
     *
     * @return 如果是链表则返回true
     */
    bool is_chained() const noexcept;

    /**
     * @brief 计算链表中所有数据的总长度
     *
     * @return 链表总数据长度
     */
    std::size_t compute_chain_length() const noexcept;

    /**
     * @brief 追加缓冲区到链表末尾
     *
     * @param buf 要追加的缓冲区
     */
    void append_to_chain(std::unique_ptr<io_buffer>&& buf);

    /**
     * @brief 在当前节点后插入缓冲区
     *
     * @param buf 要插入的缓冲区
     */
    void insert_after(std::unique_ptr<io_buffer>&& buf);

    /**
     * @brief 合并链表为单个缓冲区
     *
     * @return 合并后的数据视图
     */
    std::string_view normalize();

private:
    /**
     * @brief 创建指定容量的缓冲区
     *
     * @param capacity 需要分配的容量大小
     */
    io_buffer(std::size_t capacity);

    /**
     * @brief 接管外部缓冲区的内部实现
     *
     * @param buf 外部缓冲区指针
     * @param capacity 缓冲区容量
     * @param length 有效数据长度
     * @param free_fn 自定义释放函数，如果为 nullptr，则不释放
     * @param user_data 传递给释放函数的用户数据
     */
    io_buffer(void* buf, std::size_t capacity, std::size_t length, free_function fn,
              void* user_data);

    /**
     * @brief 分配内存
     */
    void allocate(std::size_t capacity);

    /**
     * @brief 释放链表
     */
    void free_chain();

    /**
     * @brief   缓冲区
     */
    detail::shard_buffer m_buffer;

    /**
     * @brief 有效数据
     */
    uint8_t* m_data{nullptr};

    /**
     * @brief 有效数据长度
     */
    std::size_t m_length{0};

    /**
     * @brief 缓冲区总容量
     */
    std::size_t m_capacity{0};

    /**
     * @brief 链表前向指针
     */
    io_buffer* m_next{this};

    /**
     * @brief 链表后向指针
     */
    io_buffer* m_prev{this};
};

} // namespace io
} // namespace mc

#endif // MC_IO_BUFFER_H