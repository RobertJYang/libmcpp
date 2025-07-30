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

#ifndef MC_IO_STREAM_H
#define MC_IO_STREAM_H

#include <cstdint>
#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

#include <mc/common.h>
#include <mc/exception.h>
#include <mc/io/io_buffer.h>

namespace mc::io {

/**
 * @brief 流的定位方式枚举
 */
enum class seek_mode {
    begin,   // 从流开始位置
    current, // 从当前位置
    end      // 从流结束位置
};

/**
 * @brief 基于 io_buffer 的流操作类
 *
 * io_stream 提供基于 io_buffer 的流式读写接口，支持：
 * - 读写各种基础数据类型，支持指定字节序
 * - 支持在流中随机定位
 * - 支持检查可读/可写状态
 * - 支持对齐写入位置
 * - 支持预留头部和尾部空间
 */
class MC_API io_stream {
public:
    /**
     * @brief 默认构造函数，创建一个空的流
     */
    io_stream();

    /**
     * @brief 从已有的 io_buffer 创建流
     *
     * @param buffer 用于初始化流的缓冲区
     * @param writable 是否可写
     */
    explicit io_stream(std::unique_ptr<io_buffer> buffer, bool writable = true);

    /**
     * @brief 创建具有指定初始容量的流
     *
     * @param capacity 初始容量
     */
    explicit io_stream(std::size_t capacity);

    /**
     * @brief 移动构造函数
     */
    io_stream(io_stream&& other) noexcept;

    /**
     * @brief 移动赋值操作符
     */
    io_stream& operator=(io_stream&& other) noexcept;

    /**
     * @brief 析构函数
     */
    ~io_stream();

    /**
     * @brief 获取底层 io_buffer
     *
     * @return 底层 io_buffer 的指针
     */
    io_buffer* get_buffer() const;

    /**
     * @brief 释放并获取底层缓冲区的所有权
     *
     * @return 底层 io_buffer 的独占指针
     */
    std::unique_ptr<io_buffer> release_buffer();

    /**
     * @brief 重置流，使用新的缓冲区
     *
     * @param buffer 新的缓冲区
     * @param writable 是否可写
     */
    void reset(std::unique_ptr<io_buffer> buffer, bool writable = true);

    /**
     * @brief 获取当前读取位置
     *
     * @return 当前读取位置
     */
    std::size_t get_read_pos() const noexcept;

    /**
     * @brief 获取当前写入位置
     *
     * @return 当前写入位置
     */
    std::size_t get_write_pos() const noexcept;

    /**
     * @brief 获取流的总长度
     *
     * @return 流的总长度
     */
    std::size_t length() const noexcept;

    /**
     * @brief 获取剩余可读字节数
     *
     * @return 剩余可读字节数
     */
    std::size_t readable_bytes() const noexcept;

    /**
     * @brief 获取已写入字节数
     *
     * @return 已写入字节数
     */
    std::size_t written_bytes() const noexcept;

    /**
     * @brief 检查是否还有可读数据
     *
     * @param length 需要检查的数据长度
     * @return 如果有足够长度的可读数据则返回 true
     */
    bool has_remaining(std::size_t length = 1) const noexcept;

    /**
     * @brief 设置读取位置
     *
     * @param pos 新的读取位置
     * @param mode 定位模式
     * @return 实际设置的位置
     */
    std::size_t seek_read(std::int64_t pos, seek_mode mode = seek_mode::begin);

    /**
     * @brief 设置写入位置
     *
     * @param pos 新的写入位置
     * @param mode 定位模式
     * @return 实际设置的位置
     */
    std::size_t seek_write(std::int64_t pos, seek_mode mode = seek_mode::begin);

    /**
     * @brief 跳过指定字节数
     *
     * @param skip_size 要跳过的字节数
     * @return 实际跳过的字节数
     */
    std::size_t skip(std::size_t skip_size);

    /**
     * @brief 读取数据到指定缓冲区
     *
     * @param data 目标缓冲区
     * @param length 要读取的字节数
     * @return 实际读取的字节数
     */
    std::size_t read(void* data, std::size_t length);

    /**
     * @brief 读取字符串
     *
     * @param length 要读取的字符串长度
     * @return 读取的字符串视图
     */
    std::string_view read(std::size_t length);

    /**
     * @brief 查看数据
     *
     * @param max_length 最大长度
     * @return 数据视图
     */
    std::string_view peek(std::size_t max_length = std::numeric_limits<std::size_t>::max()) const;

    /**
     * @brief 读取数据到指定缓冲区
     *
     * @param data 目标缓冲区
     * @param length 要读取的字节数
     * @return 实际读取的字节数
     */
    std::size_t read_some(void* data, std::size_t length);

    /**
     * @brief 读取字符串
     *
     * @param length 要读取的字符串长度
     * @return 读取的字符串视图
     */
    std::string_view read_some(std::size_t length);

    /**
     * @brief 尝试读取数据
     *
     * @param data 目标缓冲区
     * @param length 要读取的字节数
     * @return 是否读取成功
     */
    bool try_read(void* data, std::size_t length);

    /**
     * @brief 尝试读取字符串
     *
     * @param length 要读取的字符串长度
     * @return 读取的字符串视图
     */
    std::string_view try_read(std::size_t length);

    /**
     * @brief 读取基本数据类型
     *
     * @tparam T 数据类型
     * @param is_little_endian 是否为小端字节序
     * @return 读取的值
     */
    template <typename T,
              typename std::enable_if<std::is_trivially_copyable<T>::value, int>::type = 0>
    T read_value(bool is_little_endian = mc::is_little_endian()) {
        T value = {};
        if (!read_value(value, is_little_endian)) {
            MC_THROW(mc::out_of_range_exception, "读取位置超出缓冲区范围");
        }

        return value;
    }

    /**
     * @brief 读取基本数据类型
     *
     * @tparam T 数据类型
     * @param value 读取的值的引用
     * @param is_little_endian 是否为小端字节序
     * @return 是否读取成功
     */
    template <typename T,
              typename std::enable_if<std::is_trivially_copyable<T>::value, int>::type = 0>
    bool read_value(T& value, bool is_little_endian = mc::is_little_endian()) {
        if (!m_buffer->read_value(m_read_pos, value, is_little_endian)) {
            return false;
        }

        m_read_pos += sizeof(T);
        return true;
    }

    /**
     * @brief 写入数据
     *
     * @param data 源数据
     * @param length 要写入的字节数
     * @return 实际写入的字节数
     */
    std::size_t write(const void* data, std::size_t length);

    /**
     * @brief 写入字符串
     *
     * @param str 要写入的字符串
     * @return 实际写入的字节数
     */
    std::size_t write(std::string_view str);

    /**
     * @brief 写入基本数据类型
     *
     * @tparam T 数据类型
     * @param value 要写入的值
     * @param is_little_endian 是否为小端字节序
     * @return 写入后的位置
     */
    template <typename T,
              typename std::enable_if<std::is_trivially_copyable<T>::value, int>::type = 0>
    std::size_t write_value(T value, bool is_little_endian = mc::is_little_endian()) {
        if (!m_writable) {
            MC_THROW(mc::eof_exception, "流不可写");
        }

        m_write_pos += m_buffer->write_value_at_offset(m_write_pos, value, is_little_endian);
        return m_write_pos;
    }

    /**
     * @brief 对齐写入位置
     *
     * @param alignment 对齐字节数
     * @return 实际填充的字节数
     */
    std::size_t align(std::size_t alignment);

    /**
     * @brief 对齐读取位置
     *
     * @param alignment 对齐字节数
     * @return 实际填充的字节数
     */
    std::size_t align_read(std::size_t alignment);

    std::optional<std::size_t> try_align_read(std::size_t alignment);

    /**
     * @brief 清空流
     */
    void clear();

    /**
     * @brief 获取缓冲区的头部空间
     *
     * @return 头部空间大小
     */
    std::size_t get_headroom() const noexcept;

    /**
     * @brief 获取缓冲区的尾部空间
     *
     * @return 尾部空间大小
     */
    std::size_t get_tailroom() const noexcept;

    /**
     * @brief 预留头部和尾部空间
     *
     * @param headroom 头部空间大小
     * @param tailroom 尾部空间大小
     */
    void reserve(std::size_t headroom, std::size_t tailroom);

    std::string_view get_data() const;
    std::string_view get_writeable_data(std::size_t min_length = 1024);

    // 一个辅助机制用于自动回填写入长度
    template <typename LengthType = uint32_t>
    struct write_length_guard : mc::noncopyable {
        static constexpr std::size_t invalid_pos = std::numeric_limits<std::size_t>::max();

        write_length_guard(io_stream& stream, bool is_little_endian = mc::is_little_endian())
            : m_stream(stream), m_is_little_endian(is_little_endian) {
        }

        // 预先写入长度
        void prepare_length_field(std::size_t alignment = sizeof(LengthType)) {
            if (alignment > 1) {
                m_stream.align(alignment);
            }

            m_write_pos = m_stream.get_write_pos();
            m_stream.write_value<LengthType>(0, m_is_little_endian);
        }

        void set_write_pos(std::size_t pos) {
            m_write_pos = pos;
        }

        void set_body_start_pos(std::size_t body_start_pos) {
            m_body_start_pos = body_start_pos;
        }

        void set_body_start_pos() {
            set_body_start_pos(m_stream.get_write_pos());
        }

        ~write_length_guard() {
            fire();
        }

        /**
         * @brief 触发回填消息体大小
         */
        void fire() {
            if (m_write_pos == invalid_pos) {
                return;
            }

            std::size_t length = m_stream.get_write_pos() - m_body_start_pos;
            m_stream.seek_write(m_write_pos, seek_mode::begin);
            m_write_pos = invalid_pos;
            m_stream.write_value<LengthType>(length, m_is_little_endian);
            m_stream.seek_write(0, seek_mode::end);
        }

        io_stream&  m_stream;
        std::size_t m_write_pos{invalid_pos};
        std::size_t m_body_start_pos{invalid_pos};
        bool        m_is_little_endian{false};
    };

private:
    /**
     * @brief 确保流可写
     */
    void ensure_writable() const;

    /**
     * @brief 底层缓冲区
     */
    std::unique_ptr<io_buffer> m_buffer;

    /**
     * @brief 当前读取位置
     */
    std::size_t m_read_pos{0};

    /**
     * @brief 当前写入位置
     */
    std::size_t m_write_pos{0};

    /**
     * @brief 流是否可写
     */
    bool m_writable{true};
};

} // namespace mc::io

#endif // MC_IO_STREAM_H