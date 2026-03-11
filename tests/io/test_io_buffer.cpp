/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>
#include <mc/common.h>
#include <mc/io/io_buffer.h>
#include <mc/variant.h>
#include <string>
#include <vector>

using namespace mc;

// 字符串操作测试
TEST(IOBufferTest, StringOperations)
{
    auto buffer = io::io_buffer::create(100);

    // 写入字符串
    std::string test_str      = "测试字符串";
    auto        bytes_written = buffer->write(test_str.data(), test_str.size());
    EXPECT_EQ(bytes_written, test_str.size());
    EXPECT_EQ(buffer->length(), test_str.size());

    // 读取为字符串视图
    std::string_view sv(reinterpret_cast<const char*>(buffer->data()), buffer->length());
    EXPECT_EQ(sv, test_str);

    // 使用 normalize 合并缓冲区并获取字符串视图
    auto normalize_sv = buffer->normalize();
    EXPECT_EQ(normalize_sv, test_str);
}

// 异常测试
TEST(IOBufferTest, ExceptionHandling)
{
    auto buffer = io::io_buffer::create(4);

    // 直接写入4字节
    uint32_t value = 0x12345678;
    buffer->append(4);
    *reinterpret_cast<uint32_t*>(buffer->mutable_data()) = value;

    // 尝试读取超出缓冲区的数据
    EXPECT_THROW(buffer->read_value<uint32_t>(1, true), std::exception);
}

// 容量管理测试
TEST(IOBufferTest, CapacityManagement)
{
    auto buffer = io::io_buffer::create(10);
    // 容量可能会被对齐到更大的值(例如4096)，测试时使用大于等于
    EXPECT_GE(buffer->capacity(), 10);

    // 预留空间
    buffer->reserve(5, 15);
    EXPECT_GE(buffer->headroom(), 5);
    EXPECT_GE(buffer->tailroom(), 15);

    // 确保有足够的尾部空间
    EXPECT_TRUE(buffer->ensure_tailroom(10));
    EXPECT_GE(buffer->tailroom(), 10);

    // 对齐操作 - 先做一些操作确保数据位置变化
    buffer->clear(); // 清空数据
    // 写入8字节数据并确保对齐
    uint64_t test_value = 0x1122334455667788;
    buffer->append(8);
    std::memcpy(buffer->mutable_data(), &test_value, 8);

    size_t alignment = 8;
    buffer->align(alignment);
    // 使用模除法检查对齐
    uintptr_t addr = reinterpret_cast<uintptr_t>(buffer->data() + buffer->length());
    EXPECT_EQ(addr % alignment, 0);
}

// 链表操作测试
TEST(IOBufferTest, ChainOperations)
{
    auto buffer1 = io::io_buffer::create(10);
    auto buffer2 = io::io_buffer::create(20);
    auto buffer3 = io::io_buffer::create(30);

    // 写入测试数据
    uint8_t data1[] = {0x01, 0x02, 0x03};
    uint8_t data2[] = {0x04, 0x05, 0x06, 0x07};
    uint8_t data3[] = {0x08, 0x09, 0x0A, 0x0B, 0x0C};

    buffer1->write(data1, sizeof(data1));
    buffer2->write(data2, sizeof(data2));
    buffer3->write(data3, sizeof(data3));

    // 添加到链表
    buffer1->append_to_chain(std::move(buffer2));
    EXPECT_TRUE(buffer1->is_chained());

    // 继续添加到链表
    buffer1->append_to_chain(std::move(buffer3));

    // 计算链表总长度
    EXPECT_EQ(buffer1->compute_chain_length(), sizeof(data1) + sizeof(data2) + sizeof(data3));

    // 合并所有缓冲区
    auto combined_view = buffer1->normalize();
    EXPECT_EQ(combined_view.size(), sizeof(data1) + sizeof(data2) + sizeof(data3));

    // 验证合并后的数据
    uint8_t expected[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C};
    for (size_t i = 0; i < sizeof(expected); ++i) {
        EXPECT_EQ(static_cast<uint8_t>(combined_view[i]), expected[i]);
    }
}

// 共享和克隆测试
TEST(IOBufferTest, SharedAndClone)
{
    auto original = io::io_buffer::create(100);

    // 写入测试数据
    uint8_t test_data[] = {0x10, 0x20, 0x30, 0x40, 0x50};
    original->write(test_data, sizeof(test_data));

    {
        // 复制构造共享同一缓冲区
        io::io_buffer shared_buffer(*original);
        EXPECT_TRUE(original->is_shared());
        EXPECT_TRUE(shared_buffer.is_shared());

        // 验证共享数据相同
        EXPECT_EQ(shared_buffer.length(), original->length());
        for (size_t i = 0; i < original->length(); ++i) {
            EXPECT_EQ(shared_buffer.data()[i], original->data()[i]);
        }

        // 克隆创建新的独立副本
        auto clone = original->clone();

        // 修改原始缓冲区
        if (original->length() > 0) {
            original->mutable_data()[0] = 0xFF;
        }

        // 验证克隆数据不变 (使用原始值检查)
        EXPECT_EQ(clone->length(), sizeof(test_data));
        EXPECT_EQ(clone->data()[0], 0x10); // 应该仍是原始值，未被修改

        // 确保共享缓冲区解除共享
        shared_buffer.unshare();
        EXPECT_FALSE(shared_buffer.is_shared());
        EXPECT_FALSE(original->is_shared());
    }

    original.reset();
}

// 写入偏移扩展测试
TEST(IOBufferTest, WriteAtOffsetExtendsLength)
{
    auto buffer = io::io_buffer::create(8);

    const uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    // 在偏移 5 位置写入 3 字节，应自动扩展长度
    std::size_t written = buffer->write_at_offset(5, payload, sizeof(payload));

    EXPECT_EQ(written, sizeof(payload));
    EXPECT_EQ(buffer->length(), 8); // 5 + 3

    // 前 5 字节自动填充默认值（未定义），只断言写入区域正确
    for (std::size_t i = 0; i < sizeof(payload); ++i) {
        EXPECT_EQ(buffer->data()[5 + i], payload[i]);
    }
}

// try_read 与 read_some 行为测试
TEST(IOBufferTest, TryReadAndReadSome)
{
    auto          buffer    = io::io_buffer::create(16);
    const uint8_t payload[] = {1, 2, 3, 4, 5};
    buffer->write(payload, sizeof(payload));

    uint8_t out[4] = {0};
    EXPECT_TRUE(buffer->try_read(1, out, sizeof(out)));
    EXPECT_EQ(out[0], 2);
    EXPECT_FALSE(buffer->try_read(buffer->length(), out, sizeof(out)));

    std::vector<uint8_t> partial(10, 0);
    auto                 read_len = buffer->read_some(2, partial.data(), partial.size());
    EXPECT_EQ(read_len, 3U);
    EXPECT_EQ(partial[0], 3);
    EXPECT_EQ(partial[2], 5);
}

TEST(IOBufferTest, ReadSomeOffsetBeyondLength)
{
    const uint8_t initial[] = {'a', 'b', 'c'};
    auto          buffer    = io::io_buffer::copy_buffer(initial, sizeof(initial));

    auto empty_view = buffer->read_some(buffer->length(), 2);
    EXPECT_TRUE(empty_view.empty());

    uint8_t tmp[4] = {0};
    EXPECT_EQ(buffer->read_some(buffer->length(), tmp, sizeof(tmp)), 0U);
}

TEST(IOBufferTest, ReadSomeNullDataThrows)
{
    auto buffer = io::io_buffer::copy_buffer("data", 4);
    EXPECT_THROW(buffer->read_some(0, nullptr, 2), mc::invalid_arg_exception);
}

// 链表插入与移动语义测试
TEST(IOBufferTest, InsertAfterAndMoveChain)
{
    auto head = io::io_buffer::create(8);
    head->write("ab", 2);

    auto node1 = io::io_buffer::create(4);
    node1->write("cd", 2);
    head->append_to_chain(std::move(node1));

    auto node2 = io::io_buffer::create(4);
    node2->write("ef", 2);
    head->insert_after(std::move(node2));

    EXPECT_TRUE(head->is_chained());
    EXPECT_EQ(head->compute_chain_length(), 6U);

    // 复制链表并测试移动构造保持链结构
    io::io_buffer moved(std::move(*head));
    EXPECT_TRUE(moved.is_chained());
    auto normalized = moved.normalize();
    EXPECT_EQ(normalized, "abefcd");
}

// 测试 take_ownership 函数
TEST(IOBufferTest, TakeOwnership)
{
    // 分配外部内存
    const size_t test_capacity = 200;
    uint8_t*     raw_buffer    = new uint8_t[test_capacity];

    // 写入一些测试数据
    const uint8_t test_pattern[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    std::memcpy(raw_buffer, test_pattern, sizeof(test_pattern));

    // 自定义释放函数
    bool was_freed   = false;
    auto custom_free = [](void* buf, void* user_data) {
        delete[] static_cast<uint8_t*>(buf);
        *static_cast<bool*>(user_data) = true;
    };

    // 创建接管外部缓冲区的 io_buffer
    {
        auto buffer = io::io_buffer::take_ownership(raw_buffer, test_capacity, sizeof(test_pattern),
                                                    custom_free, &was_freed);

        // 验证基本属性
        ASSERT_TRUE(buffer != nullptr);
        EXPECT_EQ(buffer->capacity(), test_capacity);
        EXPECT_EQ(buffer->length(), sizeof(test_pattern));

        // 验证数据
        for (size_t i = 0; i < sizeof(test_pattern); ++i) {
            EXPECT_EQ(buffer->data()[i], test_pattern[i]);
        }

        // buffer 销毁前，未释放
        EXPECT_FALSE(was_freed);
    }

    // buffer 超出作用域后，应调用自定义释放函数
    EXPECT_TRUE(was_freed);
}

// 测试 wrap 函数
TEST(IOBufferTest, WrapBuffer)
{
    // 创建不会被 io_buffer 释放的静态数据
    static const uint8_t external_data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    const size_t         data_size       = sizeof(external_data);

    // 包装外部缓冲区
    auto buffer = io::io_buffer::wrap(external_data, data_size);

    // 验证基本属性
    ASSERT_TRUE(buffer != nullptr);
    EXPECT_EQ(buffer->capacity(), data_size);
    EXPECT_EQ(buffer->length(), data_size);
    EXPECT_FALSE(buffer->empty());

    // 验证数据引用
    EXPECT_EQ(buffer->data(), external_data);

    // 验证数据内容
    for (size_t i = 0; i < data_size; ++i) {
        EXPECT_EQ(buffer->data()[i], external_data[i]);
    }

    // 复制包装的缓冲区
    io::io_buffer copied_buffer(*buffer);
    EXPECT_TRUE(buffer->is_shared());
    EXPECT_TRUE(copied_buffer.is_shared());
}

// 测试 copy_buffer 函数
TEST(IOBufferTest, CopyBuffer)
{
    // 准备测试数据
    const uint8_t source_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    const size_t  data_size     = sizeof(source_data);

    // 测试带空间预留的复制
    const size_t headroom = 10;
    const size_t tailroom = 20;

    auto buffer = io::io_buffer::copy_buffer(source_data, data_size, headroom, tailroom);

    // 验证基本属性
    ASSERT_TRUE(buffer != nullptr);
    EXPECT_GE(buffer->capacity(), data_size + headroom + tailroom);
    EXPECT_EQ(buffer->length(), data_size);
    EXPECT_GE(buffer->headroom(), headroom);
    EXPECT_GE(buffer->tailroom(), tailroom);

    // 验证数据内容
    for (size_t i = 0; i < data_size; ++i) {
        EXPECT_EQ(buffer->data()[i], source_data[i]);
    }

    // 测试使用预留的空间

    // 使用头部空间
    const uint8_t header[] = {0xAA, 0xBB, 0xCC};
    buffer->prepend(sizeof(header));
    std::memcpy(buffer->mutable_data(), header, sizeof(header));

    // 使用尾部空间
    const uint8_t trailer[]  = {0xDD, 0xEE, 0xFF};
    const size_t  old_length = buffer->length();
    buffer->append(sizeof(trailer));
    std::memcpy(buffer->mutable_tail() - sizeof(trailer), trailer, sizeof(trailer));

    // 验证完整数据
    EXPECT_EQ(buffer->length(), sizeof(header) + data_size + sizeof(trailer));

    // 验证头部数据
    for (size_t i = 0; i < sizeof(header); ++i) {
        EXPECT_EQ(buffer->data()[i], header[i]);
    }

    // 验证中间数据
    for (size_t i = 0; i < data_size; ++i) {
        EXPECT_EQ(buffer->data()[i + sizeof(header)], source_data[i]);
    }

    // 验证尾部数据
    for (size_t i = 0; i < sizeof(trailer); ++i) {
        EXPECT_EQ(buffer->data()[i + sizeof(header) + data_size], trailer[i]);
    }
}

// 测试边界条件和错误处理
TEST(IOBufferTest, EdgeCases)
{
    // 测试零长度缓冲区
    auto empty_buffer = io::io_buffer::create(0);
    EXPECT_EQ(empty_buffer->capacity(), 0);
    EXPECT_EQ(empty_buffer->length(), 0);
    EXPECT_TRUE(empty_buffer->empty());

    // 测试零长度复制
    auto empty_copy = io::io_buffer::copy_buffer(nullptr, 0);
    EXPECT_EQ(empty_copy->length(), 0);
    EXPECT_TRUE(empty_copy->empty());

    // 测试包装nullptr但长度为0的缓冲区
    auto null_wrapped = io::io_buffer::wrap(nullptr, 0);
    EXPECT_EQ(null_wrapped->length(), 0);
    EXPECT_TRUE(null_wrapped->empty());

    // 测试无效参数错误处理
    EXPECT_THROW(io::io_buffer::wrap(nullptr, 10), mc::exception); // nullptr但长度不为0

    EXPECT_THROW(io::io_buffer::take_ownership(nullptr, 100), mc::exception); // nullptr缓冲区

    uint8_t data[] = {0x01, 0x02, 0x03};
    EXPECT_THROW(io::io_buffer::take_ownership(data, 2, 3), mc::exception); // 长度超出容量

    EXPECT_THROW(io::io_buffer::copy_buffer(nullptr, 10),
                 mc::exception); // 数据指针为空但长度不为0
}

TEST(IOBufferTest, WriteAtOffsetWithoutExtendingLength)
{
    const uint8_t initial[] = {0x01, 0x02, 0x03, 0x04};
    auto          buffer    = io::io_buffer::copy_buffer(initial, sizeof(initial));

    const uint8_t patch[]       = {0xAA, 0xBB};
    auto          before_length = buffer->length();
    auto          written       = buffer->write_at_offset(1, patch, sizeof(patch));

    EXPECT_EQ(written, sizeof(patch));
    EXPECT_EQ(buffer->length(), before_length);
    EXPECT_EQ(buffer->data()[0], initial[0]);
    EXPECT_EQ(buffer->data()[1], patch[0]);
    EXPECT_EQ(buffer->data()[2], patch[1]);
    EXPECT_EQ(buffer->data()[3], initial[3]);
}

TEST(IOBufferTest, TrimOperationsAdjustPointers)
{
    const uint8_t initial[] = {0x10, 0x20, 0x30, 0x40, 0x50};
    auto          buffer    = io::io_buffer::copy_buffer(initial, sizeof(initial));

    auto* base_ptr = buffer->mutable_data();
    buffer->trim_start(2);
    EXPECT_EQ(buffer->length(), sizeof(initial) - 2);
    EXPECT_EQ(buffer->mutable_data(), base_ptr + 2);
    EXPECT_EQ(buffer->data()[0], initial[2]);

    buffer->trim_end(1);
    EXPECT_EQ(buffer->length(), sizeof(initial) - 3);
    EXPECT_EQ(buffer->data()[buffer->length() - 1], initial[sizeof(initial) - 2]);

    // 超过剩余长度会清空数据
    buffer->trim_start(100);
    EXPECT_TRUE(buffer->empty());
}

TEST(IOBufferTest, TryReadViewAndFailure)
{
    const uint8_t initial[] = {'a', 'b', 'c', 'd', 'e'};
    auto          buffer    = io::io_buffer::copy_buffer(initial, sizeof(initial));

    auto view = buffer->try_read(1, 3);
    EXPECT_EQ(view, "bcd");

    auto empty_view = buffer->try_read(buffer->length(), 2);
    EXPECT_TRUE(empty_view.empty());

    uint8_t tmp[4] = {0};
    EXPECT_FALSE(buffer->try_read(buffer->length() + 1, tmp, sizeof(tmp)));
}

TEST(IOBufferTest, NormalizeOnEmptyChain)
{
    auto buffer = io::io_buffer::create(0);
    auto view   = buffer->normalize();
    EXPECT_TRUE(view.empty());
    EXPECT_TRUE(buffer->empty());
}

// 测试移动赋值时包含链表节点的情况
TEST(IOBufferTest, IOBufferMoveAssignmentWithChain)
{
    // 创建一个带链表的 io_buffer
    auto buf1 = io::io_buffer::create(10);
    buf1->write("abc", 3);

    auto buf2 = io::io_buffer::create(10);
    buf2->write("def", 3);
    buf1->append_to_chain(std::move(buf2));

    EXPECT_TRUE(buf1->is_chained());
    EXPECT_EQ(buf1->compute_chain_length(), 6U);

    // 创建另一个 io_buffer
    auto buf3 = io::io_buffer::create(10);
    buf3->write("xyz", 3);

    // 执行移动赋值
    *buf3 = std::move(*buf1);

    // 验证 buf3 包含所有链表节点
    EXPECT_TRUE(buf3->is_chained());
    EXPECT_EQ(buf3->compute_chain_length(), 6U);

    auto normalized = buf3->normalize();
    EXPECT_EQ(normalized, "abcdef");
}

// 测试拷贝构造时包含链表节点的情况
TEST(IOBufferTest, IOBufferCopyConstructorWithChain)
{
    // 创建一个带链表的 io_buffer
    auto buf1 = io::io_buffer::create(10);
    buf1->write("abc", 3);

    auto buf2 = io::io_buffer::create(10);
    buf2->write("def", 3);
    buf1->append_to_chain(std::move(buf2));

    EXPECT_TRUE(buf1->is_chained());
    EXPECT_EQ(buf1->compute_chain_length(), 6U);

    // 使用拷贝构造函数创建新对象
    io::io_buffer buf3(*buf1);

    // 验证新对象包含所有链表节点的拷贝
    EXPECT_TRUE(buf3.is_chained());
    EXPECT_EQ(buf3.compute_chain_length(), 6U);

    auto normalized = buf3.normalize();
    EXPECT_EQ(normalized, "abcdef");
}

// 测试拷贝赋值运算符
TEST(IOBufferTest, IOBufferCopyAssignment)
{
    // 创建两个 io_buffer 对象
    auto buf1 = io::io_buffer::create(10);
    buf1->write("test", 4);

    auto buf2 = io::io_buffer::create(10);
    buf2->write("xyz", 3);

    // 执行拷贝赋值
    *buf2 = *buf1;

    // 验证 buf2 的内容与 buf1 相同
    EXPECT_EQ(buf2->length(), buf1->length());
    EXPECT_EQ(buf2->compute_chain_length(), buf1->compute_chain_length());

    auto view1 = buf1->normalize();
    auto view2 = buf2->normalize();
    EXPECT_EQ(view2, view1);
}

// 测试 append 的边界情况
TEST(IOBufferTest, IOBufferAppendZeroOrInsufficientRoom)
{
    auto buffer = io::io_buffer::create(10);
    buffer->write("test", 4);

    size_t initial_length = buffer->length();

    // 调用 append(0)，验证 length 不变
    buffer->append(0);
    EXPECT_EQ(buffer->length(), initial_length);

    // 调用 append(very_large_amount)，验证 length 不变（因为 tailroom() < amount）
    size_t very_large_amount = buffer->tailroom() + 1000;
    buffer->append(very_large_amount);
    EXPECT_EQ(buffer->length(), initial_length);
}

// 测试 prepend 的边界情况
TEST(IOBufferTest, IOBufferPrependZeroOrInsufficientRoom)
{
    auto buffer = io::io_buffer::create(10);
    buffer->write("test", 4);

    size_t initial_length = buffer->length();

    // 调用 prepend(0)，验证 length 不变
    buffer->prepend(0);
    EXPECT_EQ(buffer->length(), initial_length);

    // 调用 prepend(very_large_amount)，验证 length 不变（因为 headroom() < amount）
    size_t very_large_amount = buffer->headroom() + 1000;
    buffer->prepend(very_large_amount);
    EXPECT_EQ(buffer->length(), initial_length);
}

// 测试 trim_end 超出长度的情况
TEST(IOBufferTest, IOBufferTrimEndExceedsLength)
{
    auto buffer = io::io_buffer::create(10);
    buffer->write("test", 4); // 写入 4 字节数据

    // 调用 trim_end(20)（超过长度）
    buffer->trim_end(20);

    // 验证 length 为 0
    EXPECT_EQ(buffer->length(), 0U);
}

// 测试 reserve 已有足够空间的情况
TEST(IOBufferTest, IOBufferReserveAlreadySufficient)
{
    // 创建一个足够大的 buffer
    auto buffer = io::io_buffer::create(100);
    buffer->write("test", 4);

    // 获取当前的 headroom 和 tailroom
    size_t current_headroom = buffer->headroom();
    size_t current_tailroom = buffer->tailroom();

    // 调用 reserve 请求小于当前 headroom 和 tailroom 的值
    size_t min_headroom = current_headroom / 2;
    size_t min_tailroom = current_tailroom / 2;
    buffer->reserve(min_headroom, min_tailroom);

    // 验证 headroom 和 tailroom 不变（或至少满足要求）
    EXPECT_GE(buffer->headroom(), min_headroom);
    EXPECT_GE(buffer->tailroom(), min_tailroom);
}

// 测试 align 无效对齐值的情况
TEST(IOBufferTest, IOBufferAlignInvalidAlignment)
{
    auto buffer = io::io_buffer::create(10);
    buffer->write("test", 4);

    size_t initial_length = buffer->length();

    // 调用 align(0) 和 align(1)
    size_t result0 = buffer->align(0);
    EXPECT_EQ(result0, 0U);
    EXPECT_EQ(buffer->length(), initial_length);

    size_t result1 = buffer->align(1);
    EXPECT_EQ(result1, 0U);
    EXPECT_EQ(buffer->length(), initial_length);
}

// 测试 write_at_offset 零长度的情况
TEST(IOBufferTest, IOBufferWriteAtOffsetZeroLength)
{
    auto buffer = io::io_buffer::create(10);
    buffer->write("test", 4);

    // 调用 write_at_offset(0, nullptr, 0)
    size_t written = buffer->write_at_offset(0, nullptr, 0);
    EXPECT_EQ(written, 0U);
}

// 测试 write_at_offset 共享缓冲区的情况
TEST(IOBufferTest, IOBufferWriteAtOffsetSharedBuffer)
{
    // 创建一个 buffer 并写入数据
    auto buf1 = io::io_buffer::create(10);
    buf1->write("test", 4);

    // 使用拷贝构造函数创建共享的 buffer
    io::io_buffer buf2(*buf1);
    EXPECT_TRUE(buf2.is_shared());

    // 在共享的 buffer 上调用 write_at_offset
    uint8_t patch[] = {0xAA, 0xBB};
    buf2.write_at_offset(1, patch, sizeof(patch));

    // 验证 unshare() 被调用（通过检查 is_shared() 变为 false）
    EXPECT_FALSE(buf2.is_shared());
}

// 测试 write 零长度的情况
TEST(IOBufferTest, IOBufferWriteZeroLength)
{
    auto buffer = io::io_buffer::create(10);
    buffer->write("test", 4);

    // 调用 write(nullptr, 0)
    size_t written = buffer->write(nullptr, 0);
    EXPECT_EQ(written, 0U);
}

// 测试 write 共享缓冲区的情况
TEST(IOBufferTest, IOBufferWriteSharedBuffer)
{
    // 创建一个 buffer 并写入数据
    auto buf1 = io::io_buffer::create(10);
    buf1->write("test", 4);

    // 使用拷贝构造函数创建共享的 buffer
    io::io_buffer buf2(*buf1);
    EXPECT_TRUE(buf2.is_shared());

    // 在共享的 buffer 上调用 write
    uint8_t patch[] = {0xAA, 0xBB};
    buf2.write(patch, sizeof(patch));

    // 验证 unshare() 被调用（通过检查 is_shared() 变为 false）
    EXPECT_FALSE(buf2.is_shared());
}

// 测试 read 空指针的情况
TEST(IOBufferTest, IOBufferReadNullPointer)
{
    auto buffer = io::io_buffer::create(10);
    buffer->write("test", 4);

    // 调用 read(0, nullptr, 10)
    EXPECT_THROW(buffer->read(0, nullptr, 10), mc::invalid_arg_exception);
}

// 测试 clone 包含链表节点的情况
TEST(IOBufferTest, IOBufferCloneWithChain)
{
    // 创建一个带链表的 io_buffer
    auto buf1 = io::io_buffer::create(10);
    buf1->write("abc", 3);

    auto buf2 = io::io_buffer::create(10);
    buf2->write("def", 3);
    buf1->append_to_chain(std::move(buf2));

    EXPECT_TRUE(buf1->is_chained());
    EXPECT_EQ(buf1->compute_chain_length(), 6U);

    // 调用 clone()
    auto cloned = buf1->clone();

    // 验证克隆的对象包含所有链表节点的拷贝
    EXPECT_TRUE(cloned->is_chained());
    EXPECT_EQ(cloned->compute_chain_length(), 6U);

    auto normalized = cloned->normalize();
    EXPECT_EQ(normalized, "abcdef");
}

// 测试 unshare 未共享缓冲区的情况
TEST(IOBufferTest, IOBufferUnshareNotShared)
{
    // 创建一个独立的 buffer（未共享）
    auto buffer = io::io_buffer::create(10);
    buffer->write("test", 4);

    EXPECT_FALSE(buffer->is_shared());

    // 调用 unshare()
    buffer->unshare();

    // 验证方法直接返回且 buffer 状态不变
    EXPECT_FALSE(buffer->is_shared());
    EXPECT_EQ(buffer->length(), 4U);
}

// 测试 compute_chain_length 单节点的情况
TEST(IOBufferTest, IOBufferComputeChainLengthSingleNode)
{
    // 创建一个独立的 buffer（无链表节点）
    auto buffer = io::io_buffer::create(10);
    buffer->write("test", 4);

    EXPECT_FALSE(buffer->is_chained());

    // 调用 compute_chain_length()
    size_t chain_length = buffer->compute_chain_length();

    // 验证返回值等于 length()
    EXPECT_EQ(chain_length, buffer->length());
}

// 测试 append_to_chain 空指针的情况
TEST(IOBufferTest, IOBufferAppendToChainNullPointer)
{
    auto buffer = io::io_buffer::create(10);
    buffer->write("test", 4);

    size_t initial_length = buffer->length();

    // 调用 append_to_chain(std::unique_ptr<io_buffer>(nullptr))
    buffer->append_to_chain(std::unique_ptr<io::io_buffer>(nullptr));

    // 验证方法直接返回且 buffer 状态不变
    EXPECT_EQ(buffer->length(), initial_length);
    EXPECT_FALSE(buffer->is_chained());
}

// 测试 insert_after 空指针的情况
TEST(IOBufferTest, IOBufferInsertAfterNullPointer)
{
    auto buffer = io::io_buffer::create(10);
    buffer->write("test", 4);

    size_t initial_length = buffer->length();

    // 调用 insert_after(std::unique_ptr<io_buffer>(nullptr))
    buffer->insert_after(std::unique_ptr<io::io_buffer>(nullptr));

    // 验证方法直接返回且 buffer 状态不变
    EXPECT_EQ(buffer->length(), initial_length);
    EXPECT_FALSE(buffer->is_chained());
}

// 测试 shard_buffer 拷贝赋值运算符
TEST(IOBufferTest, ShardBufferCopyAssignment)
{
    // 创建两个 io_buffer 对象（它们内部使用 shard_buffer）
    auto buf1 = io::io_buffer::create(10);
    buf1->write("test", 4);

    auto buf2 = io::io_buffer::create(10);
    buf2->write("xyz", 3);

    // 执行拷贝赋值 buf2 = buf1
    *buf2 = *buf1;

    // 验证 buf2 的内容与 buf1 相同，且 buf2.is_shared() 为 true
    EXPECT_EQ(buf2->length(), buf1->length());
    EXPECT_TRUE(buf2->is_shared());
    EXPECT_TRUE(buf1->is_shared());

    auto view1 = buf1->normalize();
    auto view2 = buf2->normalize();
    EXPECT_EQ(view2, view1);
}

// 测试 read_value 带 offset 参数且字节序不匹配的场景
TEST(IOBufferTest, IOBufferReadValueWithOffsetAndEndianness)
{
    // 创建一个 buffer 并写入大端字节序的 uint32_t 值
    auto     buffer = io::io_buffer::create(10);
    uint32_t value  = 0x12345678;

    // 以大端字节序写入
    uint8_t big_endian_bytes[] = {0x12, 0x34, 0x56, 0x78};
    buffer->write(big_endian_bytes, sizeof(big_endian_bytes));

    // 在小端系统上使用 read_value<uint32_t>(offset, true) 读取（指定小端，但数据是大端）
    // 这会触发字节交换分支
    uint32_t read_value = buffer->read_value<uint32_t>(0, true);

    // 验证读取的值正确（在小端系统上，大端数据 0x12345678 读取为小端应该是 0x78563412）
    if (mc::is_little_endian()) {
        EXPECT_EQ(read_value, 0x78563412U);
    } else {
        // 在大端系统上，大端数据读取为小端应该是 0x12345678（不需要交换）
        EXPECT_EQ(read_value, 0x12345678U);
    }
}