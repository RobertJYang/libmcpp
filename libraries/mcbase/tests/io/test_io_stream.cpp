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

#include <cstring>
#include <gtest/gtest.h>
#include <mc/exception.h>
#include <mc/io/io_stream.h>
#include <optional>
#include <string>
#include <vector>

// io_stream 基本功能测试
TEST(io_stream_test, basic_operations)
{
    // 创建一个容量为 100 的流
    mc::io::io_stream stream(100);

    // 测试初始状态
    EXPECT_EQ(stream.get_read_pos(), 0);
    EXPECT_EQ(stream.get_write_pos(), 0);
    EXPECT_EQ(stream.length(), 0);
    EXPECT_EQ(stream.readable_bytes(), 0);
    EXPECT_FALSE(stream.has_remaining());

    // 写入一些数据
    const char* test_data = "测试数据";
    std::size_t data_len  = strlen(test_data);
    EXPECT_EQ(stream.write(test_data, data_len), data_len);

    // 检查写入后的状态
    EXPECT_EQ(stream.get_read_pos(), 0);
    EXPECT_EQ(stream.get_write_pos(), data_len);
    EXPECT_EQ(stream.length(), data_len);
    EXPECT_EQ(stream.readable_bytes(), data_len);
    EXPECT_TRUE(stream.has_remaining());

    // 读取并验证数据
    char buffer[100] = {0};
    EXPECT_EQ(stream.read(buffer, data_len), data_len);
    EXPECT_STREQ(buffer, test_data);

    // 检查读取后的状态
    EXPECT_EQ(stream.get_read_pos(), data_len);
    EXPECT_EQ(stream.get_write_pos(), data_len);
    EXPECT_EQ(stream.readable_bytes(), 0);
    EXPECT_FALSE(stream.has_remaining());
}

// 测试基本数据类型的读写
TEST(io_stream_test, basic_data_types)
{
    mc::io::io_stream stream(1024);

    // 写入各种基本数据类型
    uint8_t  u8_value  = 123;
    int16_t  i16_value = -12345;
    uint32_t u32_value = 0x12345678;
    int64_t  i64_value = -1234567890123456789LL;
    float    f_value   = 3.14159f;
    double   d_value   = 2.718281828459;

    stream.write_value(u8_value);
    stream.write_value(i16_value);
    stream.write_value(u32_value);
    stream.write_value(i64_value);
    stream.write_value(f_value);
    stream.write_value(d_value);

    // 将读取位置移动到 0，准备从头读取
    stream.seek_read(0);

    // 读取并验证
    EXPECT_EQ(stream.read_value<uint8_t>(), u8_value);
    EXPECT_EQ(stream.read_value<int16_t>(), i16_value);
    EXPECT_EQ(stream.read_value<uint32_t>(), u32_value);
    EXPECT_EQ(stream.read_value<int64_t>(), i64_value);
    EXPECT_FLOAT_EQ(stream.read_value<float>(), f_value);
    EXPECT_DOUBLE_EQ(stream.read_value<double>(), d_value);
}

// 测试字符串读写
TEST(io_stream_test, string_operations)
{
    mc::io::io_stream stream(1024);

    // 写入普通字符串
    mc::string_view test_str = "这是一个测试字符串";
    EXPECT_EQ(stream.write(test_str), test_str.size());

    // 将读取位置移动到 0，准备从头读取
    stream.seek_read(0);

    // 读取普通字符串
    auto read_str = stream.read(test_str.size());
    EXPECT_EQ(read_str, test_str);

    // 读取超出范围的位置会抛出异常
    stream.seek_read(0);
    EXPECT_THROW(stream.read(test_str.size() + 1), mc::out_of_range_exception);

    // 尝试读取超出范围的位置
    stream.seek_read(0);
    EXPECT_TRUE(stream.try_read(test_str.size() + 1).empty());

    // 读取一些数据
    stream.seek_read(0);
    EXPECT_EQ(stream.read_some(test_str.size() + 1), test_str);
}

// 测试位置操作
TEST(io_stream_test, position_operations)
{
    mc::io::io_stream stream(1024);

    // 写入一些数据
    for (uint32_t i = 0; i < 10; ++i) {
        stream.write_value(i);
    }

    // 测试读取位置操作
    stream.seek_read(0);
    EXPECT_EQ(stream.get_read_pos(), 0);

    stream.seek_read(4 * sizeof(uint32_t), mc::io::seek_mode::begin);
    EXPECT_EQ(stream.read_value<uint32_t>(), 4);

    stream.seek_read(2 * sizeof(uint32_t), mc::io::seek_mode::current);
    EXPECT_EQ(stream.read_value<uint32_t>(), 7);

    stream.seek_read(-3 * sizeof(uint32_t), mc::io::seek_mode::end);
    EXPECT_EQ(stream.read_value<uint32_t>(), 7);

    // 测试写入位置操作
    stream.seek_write(2 * sizeof(uint32_t), mc::io::seek_mode::begin);
    stream.write_value<uint32_t>(100);

    stream.seek_read(2 * sizeof(uint32_t), mc::io::seek_mode::begin);
    EXPECT_EQ(stream.read_value<uint32_t>(), 100);
}

// 测试边界条件
TEST(io_stream_test, boundary_conditions)
{
    mc::io::io_stream stream(10);

    // 写入超过当前容量
    std::vector<char> large_data(100, 'A');
    EXPECT_EQ(stream.write(large_data.data(), large_data.size()), large_data.size());

    // 重置位置
    stream.seek_read(0);
    stream.seek_write(0);

    // 读取越界测试
    char buffer[10];
    stream.read(buffer, 10);

    // 尝试读取超出范围的位置，会抛出异常
    stream.seek_read(stream.length() - 1);
    EXPECT_THROW(stream.read_value<uint64_t>(), mc::out_of_range_exception);

    // 尝试读取超出范围的位置，会返回 false
    uint64_t value = 0;
    stream.seek_read(stream.length() - 1);
    EXPECT_FALSE(stream.read_value(value));
}

// 测试流的复位和清空
TEST(io_stream_test, reset_and_clear)
{
    mc::io::io_stream stream(100);

    // 写入数据
    stream.write("测试数据");

    // 清空流
    stream.clear();

    // 验证状态重置
    EXPECT_EQ(stream.get_read_pos(), 0);
    EXPECT_EQ(stream.get_write_pos(), 0);
    EXPECT_EQ(stream.length(), 0);
    EXPECT_FALSE(stream.has_remaining());

    // 重新写入数据
    stream.write("新数据");

    // 释放缓冲区
    auto buffer = stream.release_buffer();

    // 验证流被重置
    EXPECT_EQ(stream.get_read_pos(), 0);
    EXPECT_EQ(stream.get_write_pos(), 0);
    EXPECT_EQ(stream.length(), 0);

    // 用新缓冲区重置流
    stream.reset(std::move(buffer), true);
    EXPECT_TRUE(stream.has_remaining());
}

// 测试使用共享 io_buffer 创建流
TEST(io_stream_test, buffer_sharing)
{
    // 创建一个 buffer 并写入数据
    auto        buffer   = mc::io::io_buffer::create(100);
    const char* test_str = "共享缓冲区测试";
    std::size_t str_len  = strlen(test_str);
    buffer->write(test_str, str_len);

    // 使用这个 buffer 创建流
    mc::io::io_stream stream(buffer->clone(), false); // 不可写

    // 尝试写入，应该抛出异常
    EXPECT_THROW(stream.write("写入测试"), mc::eof_exception);

    // 但可以读取
    char buffer_data[100] = {0};
    EXPECT_EQ(stream.read(buffer_data, str_len), str_len);
    EXPECT_STREQ(buffer_data, test_str);
}

// 测试负向 seek_write 时扩展前置空间
TEST(io_stream_test, seek_write_negative_expands_headroom)
{
    mc::io::io_stream stream(8);

    stream.seek_write(-5, mc::io::seek_mode::current);
    EXPECT_EQ(stream.get_write_pos(), 0U);
    EXPECT_GE(stream.length(), 5U);

    // 写入验证前置预留后仍可写入
    uint8_t value = 0x1A;
    stream.write_value(value);
    stream.seek_read(0);
    EXPECT_EQ(stream.read_value<uint8_t>(), value);
}

// 测试随机访问
TEST(io_stream_test, random_access)
{
    mc::io::io_stream stream(100);

    // 写入一系列整数
    for (uint32_t i = 0; i < 10; ++i) {
        stream.write_value(i);
    }

    // 随机访问读取
    stream.seek_read(3 * sizeof(uint32_t));
    EXPECT_EQ(stream.read_value<uint32_t>(), 3);

    stream.seek_read(7 * sizeof(uint32_t));
    EXPECT_EQ(stream.read_value<uint32_t>(), 7);

    stream.seek_read(0);
    EXPECT_EQ(stream.read_value<uint32_t>(), 0);

    // 随机访问写入
    stream.seek_write(5 * sizeof(uint32_t));
    stream.write_value<uint32_t>(50);

    stream.seek_read(5 * sizeof(uint32_t));
    EXPECT_EQ(stream.read_value<uint32_t>(), 50);
}

TEST(io_stream_test, read_some_string_view_behavior)
{
    mc::io::io_stream stream(32);
    stream.write("abcdef");

    stream.seek_read(0);
    auto part1 = stream.read_some(3);
    EXPECT_EQ(part1, "abc");
    EXPECT_EQ(stream.get_read_pos(), 3U);

    auto part2 = stream.read_some(5);
    EXPECT_EQ(part2, "def");
    EXPECT_EQ(stream.get_read_pos(), 8U); // 根据实现，位置按照请求长度增加
}

TEST(io_stream_test, try_read_failure_keeps_position)
{
    mc::io::io_stream stream(16);
    stream.write("hello");

    stream.seek_read(stream.length());
    uint8_t buffer[4] = {0};
    auto    position  = stream.get_read_pos();
    EXPECT_FALSE(stream.try_read(buffer, sizeof(buffer)));
    EXPECT_EQ(stream.get_read_pos(), position);
}

TEST(io_stream_test, align_read_throws_on_insufficient_bytes)
{
    mc::io::io_stream stream(16);
    stream.write("abc");

    stream.seek_read(1);
    EXPECT_THROW(stream.align_read(4), mc::eof_exception);
}

TEST(io_stream_test, reserve_adjusts_head_and_tailroom)
{
    mc::io::io_stream stream(4);
    stream.reserve(8, 12);
    EXPECT_GE(stream.get_headroom(), 8U);
    EXPECT_GE(stream.get_tailroom(), 12U);
    stream.write("test");
    EXPECT_GE(stream.get_tailroom(), 12U - 4U);
}

TEST(io_stream_test, get_data_and_align_readAlreadyAligned)
{
    mc::io::io_stream stream(32);
    stream.write("payload");

    auto data_view = stream.get_data();
    EXPECT_EQ(data_view.size(), stream.length());
    EXPECT_EQ(data_view.substr(0, 7), "payload");

    stream.seek_read(4);
    EXPECT_NO_THROW({
        auto padding = stream.align_read(2);
        EXPECT_EQ(padding, 0U);
        EXPECT_EQ(stream.get_read_pos(), 4U);
    });
}

// 测试扩展写入位置超出当前长度
TEST(io_stream_test, expand_write_position)
{
    mc::io::io_stream stream(10);

    // 直接设置写入位置到更远处
    stream.seek_write(20);

    // 写入数据
    stream.write_value<uint32_t>(42);

    // 读取验证
    stream.seek_read(20);
    EXPECT_EQ(stream.read_value<uint32_t>(), 42);

    // 检查实际长度
    EXPECT_EQ(stream.length(), 24); // 20 + sizeof(uint32_t)
}

// 测试字节序
TEST(io_stream_test, endianness)
{
    mc::io::io_stream stream(100);

    // 默认字节序写入
    uint32_t value = 0x12345678;
    stream.write_value(value);

    // 显式指定相反字节序写入
    stream.write_value(value, !mc::is_little_endian());

    // 读取验证
    stream.seek_read(0);
    uint32_t read_value1 = stream.read_value<uint32_t>();
    uint32_t read_value2 = stream.read_value<uint32_t>(!mc::is_little_endian());

    EXPECT_EQ(read_value1, value);
    EXPECT_EQ(read_value2, value);
}

// 测试对齐功能
TEST(io_stream_test, alignment)
{
    mc::io::io_stream stream(100);

    // 写入一个字节的数据，使后续写入不对齐
    uint8_t byte_value = 0x01;
    stream.write_value(byte_value);

    // 获取当前写入位置
    std::size_t pos_before_align = stream.get_write_pos();
    EXPECT_EQ(pos_before_align, 1); // 应该是1字节

    // 进行4字节对齐
    std::size_t padding = stream.align(4);
    EXPECT_EQ(padding, 3); // 应该需要3字节的填充

    // 检查写入位置是否已正确对齐
    std::size_t pos_after_align = stream.get_write_pos();
    EXPECT_EQ(pos_after_align, 4);     // 应该是4字节
    EXPECT_EQ(pos_after_align % 4, 0); // 应该可以被4整除

    // 写入一个int32_t，应该能正常工作
    int32_t int_value = 0x12345678;
    stream.write_value(int_value);

    // 检查写入后的位置
    EXPECT_EQ(stream.get_write_pos(), 8); // 4 + 4 = 8

    // 再写入一个奇数字节
    stream.write_value<uint8_t>(0xFF);
    EXPECT_EQ(stream.get_write_pos(), 9);

    // 对齐到8字节边界
    padding = stream.align(8);
    EXPECT_EQ(padding, 7); // 需要7字节填充使总长度为16
    EXPECT_EQ(stream.get_write_pos(), 16);
    EXPECT_EQ(stream.get_write_pos() % 8, 0);

    // 测试对齐为1的情况
    padding = stream.align(1);
    EXPECT_EQ(padding, 0);                 // 应该不需要填充
    EXPECT_EQ(stream.get_write_pos(), 16); // 位置不变

    // 测试已经对齐的情况
    padding = stream.align(8);
    EXPECT_EQ(padding, 0);                 // 已经对齐，不需要填充
    EXPECT_EQ(stream.get_write_pos(), 16); // 位置不变

    // 测试对齐到奇数边界
    padding = stream.align(3);
    EXPECT_EQ(padding, 2); // 需要2字节填充
    EXPECT_EQ(stream.get_write_pos(), 18);
    EXPECT_EQ(stream.get_write_pos() % 3, 0); // 能被3整除
}

// 测试对齐功能，需要扩展缓冲区
TEST(io_stream_test, alignment_need_expand)
{
    {
        mc::io::io_stream stream;
        stream.reserve(0, 32);

        // 先写入一些数据让尾部空间变小
        mc::string payload(stream.get_tailroom() - 3, '0');
        stream.write(payload);
        std::size_t pos_before_align = stream.get_write_pos();
        EXPECT_EQ(pos_before_align, payload.size());

        std::size_t length = stream.length();
        EXPECT_EQ(length, payload.size());

        // 进行7字节对齐
        std::size_t alignment = 7;
        std::size_t padding   = stream.align(alignment);

        auto expected_padding = alignment - length % alignment;
        EXPECT_EQ(padding, expected_padding);

        // 检查写入位置是否已正确对齐
        std::size_t pos_after_align = stream.get_write_pos();
        EXPECT_EQ(pos_after_align % alignment, 0);
    }

    {
        mc::io::io_stream stream;
        stream.reserve(0, 32);

        // 先写入一些数据让尾部空间变小
        mc::string payload(stream.get_tailroom() - 2, '0');
        stream.write(payload);

        // 故意移动到非尾部，测试中间数据对齐
        stream.seek_write(-1, mc::io::seek_mode::end);
        std::size_t pos_before_align = stream.get_write_pos();

        // 进行7字节对齐
        std::size_t alignment = 7;
        std::size_t padding   = stream.align(alignment);

        auto expected_padding = alignment - pos_before_align % alignment;
        EXPECT_EQ(padding, expected_padding);

        std::size_t pos_after_align = stream.get_write_pos();
        EXPECT_EQ(pos_after_align % alignment, 0);
    }
}

// 测试 try_align_read 行为
TEST(io_stream_test, try_align_read_behavior)
{
    mc::io::io_stream stream(32);
    mc::string       payload = "ABCDEFGHIJ"; // 10 字节
    stream.write(payload);

    stream.seek_read(1);
    auto padding = stream.try_align_read(4);
    ASSERT_TRUE(padding.has_value());
    EXPECT_EQ(padding.value(), 3U);
    EXPECT_EQ(stream.get_read_pos(), 4U);

    stream.seek_read(stream.length());
    auto fail_padding = stream.try_align_read(4);
    EXPECT_FALSE(fail_padding.has_value());
}

// 测试头部预留空间功能
TEST(io_stream_test, front_space_operations)
{
    mc::io::io_stream stream(100);

    // 预留前向空间
    const size_t front_size = 16;
    stream.reserve(front_size, 0);
    EXPECT_EQ(stream.get_headroom(), front_size);

    // 写入一些数据
    const mc::string_view payload = "有效载荷数据";
    stream.write(payload);

    // 往 headroom 写入数据（front_size 还足够不需要扩容）
    uint32_t version = 0x01020304;
    stream.seek_write(-sizeof(version), mc::io::seek_mode::begin);
    stream.write_value(version);
    EXPECT_EQ(stream.get_write_pos(), sizeof(version));

    // 验证从头开始读取数据：version + payload
    stream.seek_read(0);
    EXPECT_EQ(stream.read_value<uint32_t>(), version);
    EXPECT_EQ(stream.read(payload.length()), payload);

    // 往 headroom 写入数据（front_size 不够需要扩容）
    const mc::string header(front_size, 'A');
    stream.seek_write(-header.size(), mc::io::seek_mode::begin);
    stream.write(header);
    EXPECT_EQ(stream.get_write_pos(), header.size());

    // 验证从头开始读取数据：header + version + payload
    stream.seek_read(0);
    EXPECT_EQ(stream.read(header.length()), header);
    EXPECT_EQ(stream.read_value<uint32_t>(), version);
    EXPECT_EQ(stream.read(payload.length()), payload);
}

// 测试 peek 与 get_writeable_data 行为
TEST(io_stream_test, peek_and_get_writeable_data)
{
    mc::io::io_stream stream(16);
    stream.write("hello");

    stream.seek_read(0);
    auto view = stream.peek(3);
    EXPECT_EQ(view, "hel");
    EXPECT_EQ(stream.get_read_pos(), 0U);

    auto initial_tail  = stream.get_tailroom();
    auto writable_span = stream.get_writeable_data(initial_tail + 10);
    EXPECT_GE(stream.get_tailroom(), initial_tail + 10);
    EXPECT_EQ(writable_span.size(), initial_tail);

    auto mutable_ptr = const_cast<char*>(writable_span.data());
    std::memcpy(mutable_ptr, "world", 5);
    stream.write(mc::string_view(mutable_ptr, 5));

    stream.seek_read(0);
    EXPECT_EQ(stream.read(stream.length()), "helloworld");
}

// 测试不可写流抛出异常
TEST(io_stream_test, ensure_writable_throws_when_disabled)
{
    auto              buffer = mc::io::io_buffer::create(16);
    mc::io::io_stream stream(buffer->clone(), false);

    EXPECT_THROW(stream.write("x"), mc::eof_exception);
    EXPECT_THROW(stream.align(4), mc::eof_exception);
}

TEST(io_stream_test, constructor_and_move_semantics)
{
    mc::io::io_stream original(32);
    original.write("move-test");

    mc::io::io_stream moved(std::move(original));
    EXPECT_EQ(moved.length(), 9U);
    EXPECT_EQ(moved.get_read_pos(), 0U);
    EXPECT_EQ(moved.get_write_pos(), 9U);
    EXPECT_EQ(original.length(), 0U);
    EXPECT_EQ(original.get_read_pos(), 0U);
    EXPECT_EQ(original.get_write_pos(), 0U);

    mc::io::io_stream assigned;
    assigned = std::move(moved);
    EXPECT_EQ(assigned.length(), 9U);
    EXPECT_EQ(assigned.get_write_pos(), 9U);
    EXPECT_EQ(moved.length(), 0U);
    EXPECT_EQ(moved.get_write_pos(), 0U);
    assigned.seek_read(0);
    EXPECT_EQ(assigned.read(assigned.length()), "move-test");
}

TEST(io_stream_test, reset_with_null_buffer_reinitialises_stream)
{
    mc::io::io_stream stream(16);
    stream.write("data");
    stream.reset(nullptr, false);
    EXPECT_EQ(stream.length(), 0U);
    EXPECT_EQ(stream.get_read_pos(), 0U);
    EXPECT_EQ(stream.get_write_pos(), 0U);
    EXPECT_THROW(stream.write("x"), mc::eof_exception);

    // 切换回可写并验证可以重新写入
    stream.reset(nullptr, true);
    EXPECT_NO_THROW(stream.write("ok"));
    EXPECT_EQ(stream.length(), 2U);
}

TEST(io_stream_test, skip_and_readable_bytes_behavior)
{
    mc::io::io_stream stream(32);
    stream.write("123456");
    stream.seek_read(0);
    EXPECT_EQ(stream.skip(3), 3U);
    EXPECT_EQ(stream.get_read_pos(), 3U);
    EXPECT_EQ(stream.skip(100), 3U);
    EXPECT_EQ(stream.get_read_pos(), stream.length());
    EXPECT_EQ(stream.skip(1), 0U);
}

TEST(io_stream_test, try_align_read_already_aligned)
{
    mc::io::io_stream stream(16);
    stream.write("ABCDEFG");
    stream.seek_read(0);
    auto padding = stream.try_align_read(4);
    ASSERT_TRUE(padding.has_value());
    EXPECT_EQ(padding.value(), 0U);
    EXPECT_EQ(stream.get_read_pos(), 0U);
}

TEST(io_stream_test, peek_empty_returns_empty_view)
{
    mc::io::io_stream stream(8);
    stream.write("12");
    stream.seek_read(2);
    EXPECT_TRUE(stream.peek(4).empty());
    EXPECT_EQ(stream.get_read_pos(), 2U);
}

TEST(io_stream_test, get_writeable_data_recycles_positions_when_consumed)
{
    mc::io::io_stream stream(16);
    stream.write("payload");
    stream.seek_read(stream.length());
    auto before_read = stream.get_read_pos();
    EXPECT_EQ(before_read, stream.get_write_pos());

    auto span = stream.get_writeable_data(4);
    EXPECT_EQ(stream.get_read_pos(), 0U);
    EXPECT_EQ(stream.get_write_pos(), 0U);
    EXPECT_TRUE(span.data() != nullptr);
    std::memcpy(const_cast<char*>(span.data()), "ping", 4);
    stream.write("pong");
    EXPECT_EQ(stream.get_write_pos(), 4U);
    EXPECT_GE(stream.length(), 4U);
    stream.seek_read(0);
    EXPECT_EQ(stream.read(4), "pong");
    auto remaining = stream.read(stream.length() - stream.get_read_pos());
    EXPECT_EQ(remaining, "oad");
}

TEST(io_stream_test, align_requires_reserve_when_in_middle)
{
    mc::io::io_stream stream(8);
    stream.write("ABCDEFGH");
    stream.seek_write(1);
    auto required_padding = 3U;
    auto padding          = stream.align(4);
    EXPECT_EQ(padding, required_padding);
    EXPECT_EQ(stream.get_write_pos(), 4U);
    stream.write("Z");
    stream.seek_read(4);
    EXPECT_EQ(stream.read(1), "Z");
}

TEST(io_stream_test, try_read_string_view_failure_returns_empty)
{
    mc::io::io_stream stream(8);
    stream.write("test");
    stream.seek_read(stream.length());
    auto view = stream.try_read(2);
    EXPECT_TRUE(view.empty());
    EXPECT_EQ(stream.get_read_pos(), stream.length());
}

// 测试构造函数传入空 buffer 的情况
TEST(io_stream_test, IOStreamConstructorWithNullBuffer)
{
    // 调用 io_stream(std::unique_ptr<io_buffer>(nullptr), true)
    mc::io::io_stream stream(std::unique_ptr<mc::io::io_buffer>(nullptr), true);

    // 验证 stream.get_buffer() 不为 nullptr
    EXPECT_NE(stream.get_buffer(), nullptr);
}

// 测试 get_buffer 函数
TEST(io_stream_test, IOStreamGetBuffer)
{
    // 创建一个 io_stream
    mc::io::io_stream stream(10);
    stream.write("test", 4);

    // 调用 get_buffer()
    auto buffer = stream.get_buffer();

    // 验证返回的指针不为 nullptr
    EXPECT_NE(buffer, nullptr);
}

// 测试 written_bytes 函数
TEST(io_stream_test, IOStreamWrittenBytes)
{
    // 创建一个 io_stream 并写入一些数据
    mc::io::io_stream stream(10);
    const char*       test_data = "test";
    std::size_t       data_len  = strlen(test_data);
    stream.write(test_data, data_len);

    // 调用 written_bytes()
    std::size_t written = stream.written_bytes();

    // 验证返回值等于写入的字节数
    EXPECT_EQ(written, data_len);
}

// 测试 seek_read 负位置的情况
TEST(io_stream_test, IOStreamSeekReadNegativePosition)
{
    // 创建一个 io_stream 并写入一些数据
    mc::io::io_stream stream(10);
    stream.write("test", 4);

    // 调用 seek_read(-100, seek_mode::current)（使 new_pos < 0）
    stream.seek_read(0); // 先设置到开始位置
    stream.seek_read(-100, mc::io::seek_mode::current);

    // 验证 read_pos 为 0
    EXPECT_EQ(stream.get_read_pos(), 0U);
}

// 测试 seek_read 超出长度的情况
TEST(io_stream_test, IOStreamSeekReadExceedsLength)
{
    // 创建一个 io_stream 并写入 10 字节数据
    mc::io::io_stream stream(20);
    stream.write("0123456789", 10);

    // 调用 seek_read(100, seek_mode::begin)（使 new_pos > length()）
    stream.seek_read(100, mc::io::seek_mode::begin);

    // 验证 read_pos 为 10
    EXPECT_EQ(stream.get_read_pos(), 10U);
}

// 测试 read_some(void* data, std::size_t length) 函数
TEST(io_stream_test, IOStreamReadSomeToBuffer)
{
    // 创建一个 io_stream 并写入一些数据
    mc::io::io_stream stream(10);
    const char*       test_data = "test";
    std::size_t       data_len  = strlen(test_data);
    stream.write(test_data, data_len);

    // 将读取位置移动到 0，准备从头读取
    stream.seek_read(0);

    // 调用 read_some(buffer, length)
    char        buffer[10] = {0};
    std::size_t read_len   = stream.read_some(buffer, data_len);

    // 验证返回的字节数正确
    EXPECT_EQ(read_len, data_len);
    EXPECT_STREQ(buffer, test_data);
}

// 测试 try_read 成功的情况
TEST(io_stream_test, IOStreamTryReadSuccess)
{
    // 创建一个 io_stream 并写入一些数据
    mc::io::io_stream stream(10);
    const char*       test_data = "test";
    std::size_t       data_len  = strlen(test_data);
    stream.write(test_data, data_len);

    // 重置读取位置，准备从头读取
    stream.seek_read(0);

    // 调用 try_read(buffer, length)
    char buffer[10] = {0};
    bool success    = stream.try_read(buffer, data_len);

    // 验证返回 true 且 read_pos 已更新
    EXPECT_TRUE(success);
    EXPECT_EQ(stream.get_read_pos(), data_len);
    EXPECT_STREQ(buffer, test_data);
}

// 测试 try_read(std::size_t length) 成功的情况
TEST(io_stream_test, IOStreamTryReadStringViewSuccess)
{
    // 创建一个 io_stream 并写入一些数据
    mc::io::io_stream stream(10);
    const char*       test_data = "test";
    std::size_t       data_len  = strlen(test_data);
    stream.write(test_data, data_len);

    // 重置读取位置
    stream.seek_read(0);

    // 调用 try_read(length)
    auto view = stream.try_read(data_len);

    // 验证返回非空 string_view 且 read_pos 已更新
    EXPECT_FALSE(view.empty());
    EXPECT_EQ(view.size(), data_len);
    EXPECT_EQ(view, test_data);
    EXPECT_EQ(stream.get_read_pos(), data_len);
}

// 测试 align_read 失败的情况
TEST(io_stream_test, IOStreamAlignReadFailure)
{
    // 创建一个 io_stream 并写入少量数据（如 1 字节）
    mc::io::io_stream stream(10);
    stream.write("a", 1);

    // 为了触发异常，强制将读取位置设置为 1（非对齐且剩余字节不足）
    stream.seek_read(1);

    // 调用 align_read(8)（需要 7 字节填充，但只有 1 字节可读）
    EXPECT_THROW(stream.align_read(8), mc::eof_exception);
}

// 测试 write_value 在不可写流上抛出异常的场景
TEST(io_stream_test, IOStreamWriteValueOnNonWritableStream)
{
    // 创建一个不可写的流（使用 io_stream(buffer->clone(), false)）
    auto buffer = mc::io::io_buffer::create(10);
    buffer->write("test", 4);
    mc::io::io_stream stream(buffer->clone(), false);

    // 调用 stream.write_value<uint32_t>(42)
    EXPECT_THROW(stream.write_value<uint32_t>(42), mc::eof_exception);
}
