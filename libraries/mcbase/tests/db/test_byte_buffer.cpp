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

#include <gtest/gtest.h>
#include <mc/db/byte_buffer.h>
#include <test_utilities/base.h>

namespace {

class byte_buffer_test : public mc::test::TestBase {};

// 测试 try_grow_by_reslice - 在容量内返回 true
TEST_F(byte_buffer_test, TryGrowByReslice)
{
    mc::db::byte_buffer buf;

    // 初始容量为 64 字节
    EXPECT_EQ(buf.cap(), 64);
    EXPECT_EQ(buf.len(), 0);

    // 在容量内调用 try_grow_by_reslice，应该返回 true
    auto [old_size1, success1] = buf.try_grow_by_reslice(32);
    EXPECT_EQ(old_size1, 0);
    EXPECT_TRUE(success1);

    // 写入一些数据
    std::vector<uint8_t> data1(32, 0xAA);
    buf.write(data1.data(), 32);
    EXPECT_EQ(buf.len(), 32);

    // 再次在容量内调用 try_grow_by_reslice
    auto [old_size2, success2] = buf.try_grow_by_reslice(16);
    EXPECT_EQ(old_size2, 32);
    EXPECT_TRUE(success2);

    // 写入更多数据，但仍在容量内
    std::vector<uint8_t> data2(16, 0xBB);
    buf.write(data2.data(), 16);
    EXPECT_EQ(buf.len(), 48);

    // 尝试超出容量，应该返回 false
    auto [old_size3, success3] = buf.try_grow_by_reslice(32); // 48 + 32 = 80 > 64
    EXPECT_EQ(old_size3, 48);
    EXPECT_FALSE(success3);
}

// 测试 grow - 从 bootstrap 切换到 heap
TEST_F(byte_buffer_test, GrowSwitchesToHeap)
{
    mc::db::byte_buffer buf;

    // 初始使用 bootstrap 缓冲区
    EXPECT_EQ(buf.cap(), 64);
    EXPECT_EQ(buf.len(), 0);

    // 写入超过 64 字节的数据，触发切换到 heap
    std::vector<uint8_t> large_data(100, 0xCC);
    buf.write(large_data.data(), 100);
    EXPECT_GT(buf.cap(), 64);
    EXPECT_EQ(buf.len(), 100);

    // 使用 data() 读取内容
    const uint8_t* data_ptr = buf.data();
    ASSERT_NE(data_ptr, nullptr);
    for (size_t i = 0; i < 100; ++i) {
        EXPECT_EQ(data_ptr[i], 0xCC);
    }

    // 再次 grow，验证容量翻倍
    size_t old_cap = buf.cap();
    buf.grow(32);
    EXPECT_GE(buf.cap(), old_cap * 2);
}

// 测试 truncate - 从 heap 回退到 bootstrap
TEST_F(byte_buffer_test, TruncateDownsizesToBootstrap)
{
    mc::db::byte_buffer buf;

    // 先写入 128 字节，触发切换到 heap
    std::vector<uint8_t> original_data(128);
    for (size_t i = 0; i < 128; ++i) {
        original_data[i] = static_cast<uint8_t>(i);
    }
    buf.write(original_data.data(), 128);
    EXPECT_EQ(buf.len(), 128);
    EXPECT_GT(buf.cap(), 64);

    // 验证原始数据
    auto bytes_view = buf.bytes();
    EXPECT_EQ(bytes_view.size(), 128);
    for (size_t i = 0; i < 128; ++i) {
        EXPECT_EQ(bytes_view[i], static_cast<uint8_t>(i));
    }

    // truncate 到 32 字节，应该回退到 bootstrap
    buf.truncate(32);
    EXPECT_EQ(buf.len(), 32);
    EXPECT_EQ(buf.cap(), 64); // 回退到 bootstrap 容量

    // 验证数据与原始前缀一致
    bytes_view = buf.bytes();
    EXPECT_EQ(bytes_view.size(), 32);
    for (size_t i = 0; i < 32; ++i) {
        EXPECT_EQ(bytes_view[i], static_cast<uint8_t>(i));
    }
}

// 测试 write_uint16/32/64 - 大端序写入
TEST_F(byte_buffer_test, WriteUintPrimitivesUseBigEndian)
{
    mc::db::byte_buffer buf;

    // 写入 uint16
    buf.write_uint16(0x1234);
    EXPECT_EQ(buf.len(), 2);
    const uint8_t* data = buf.data();
    EXPECT_EQ(data[0], 0x12);
    EXPECT_EQ(data[1], 0x34);

    // 写入 uint32
    buf.write_uint32(0x12345678);
    EXPECT_EQ(buf.len(), 6); // 2 + 4
    EXPECT_EQ(data[2], 0x12);
    EXPECT_EQ(data[3], 0x34);
    EXPECT_EQ(data[4], 0x56);
    EXPECT_EQ(data[5], 0x78);

    // 写入 uint64
    buf.write_uint64(0x0123456789ABCDEF);
    EXPECT_EQ(buf.len(), 14); // 6 + 8
    EXPECT_EQ(data[6], 0x01);
    EXPECT_EQ(data[7], 0x23);
    EXPECT_EQ(data[8], 0x45);
    EXPECT_EQ(data[9], 0x67);
    EXPECT_EQ(data[10], 0x89);
    EXPECT_EQ(data[11], 0xAB);
    EXPECT_EQ(data[12], 0xCD);
    EXPECT_EQ(data[13], 0xEF);
}

// 测试 cap() 方法
TEST_F(byte_buffer_test, CapMethod)
{
    mc::db::byte_buffer buf;

    // 初始容量
    EXPECT_EQ(buf.cap(), 64);

    // 写入超过 64 字节后，容量应该增加
    std::vector<uint8_t> data(100, 0xFF);
    buf.write(data.data(), 100);
    EXPECT_GT(buf.cap(), 64);
}

} // namespace
