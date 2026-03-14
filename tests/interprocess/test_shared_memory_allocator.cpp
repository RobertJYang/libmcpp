/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <mc/interprocess/shared_memory_allocator.h>
#include <mc/interprocess/shared_memory_manager.h>
#include <string>
#include <test_utilities/test_base.h>
#include <unistd.h>
#include <vector>

using namespace mc::interprocess;

class shared_memory_allocator_test : public mc::test::TestBase {
protected:
    void SetUp() override
    {
        TestBase::SetUp();
        std::string test_name = "test_allocator_" + std::to_string(getpid());
        m_manager =
            std::make_unique<shared_memory_manager>(test_name, 64 * 1024, shared_memory_manager::REMOVE_ON_EXIT);
        m_shm = m_manager->get_shared_memory();
        ASSERT_NE(m_shm, nullptr);
        m_allocator = &m_shm->get_allocator();
    }

    void TearDown() override
    {
        m_shm.reset();
        m_manager.reset();
        TestBase::TearDown();
    }

    std::unique_ptr<shared_memory_manager> m_manager;
    std::shared_ptr<shared_memory>         m_shm;
    shared_memory_allocator*               m_allocator;
};

// 测试基础内存分配和释放
TEST_F(shared_memory_allocator_test, basic_allocate_deallocate)
{
    size_t size = 1024;
    void*  ptr  = m_allocator->allocate(size);
    ASSERT_NE(ptr, nullptr);

    // 验证分配的内存可写
    std::memset(ptr, 0xAA, size);
    unsigned char* bytes = static_cast<unsigned char*>(ptr);
    EXPECT_EQ(bytes[0], 0xAA);

    m_allocator->deallocate(ptr);
}

// 测试类型安全的分配
TEST_F(shared_memory_allocator_test, typed_allocate)
{
    int* ptr = m_allocator->allocate<int>();
    ASSERT_NE(ptr, nullptr);
    *ptr = 42;
    EXPECT_EQ(*ptr, 42);
    m_allocator->deallocate(ptr);
}

// 测试数组分配
TEST_F(shared_memory_allocator_test, allocate_array)
{
    size_t count = 10;
    int*   arr   = m_allocator->allocate_array<int>(count);
    ASSERT_NE(arr, nullptr);

    for (size_t i = 0; i < count; ++i) {
        arr[i] = static_cast<int>(i);
    }

    for (size_t i = 0; i < count; ++i) {
        EXPECT_EQ(arr[i], static_cast<int>(i));
    }

    m_allocator->deallocate(arr);
}

// 测试对象构造和析构
TEST_F(shared_memory_allocator_test, construct_destroy)
{
    struct test_struct {
        int value;
        test_struct() : value(0)
        {}
        test_struct(int v) : value(v)
        {}
    };

    void* mem = m_allocator->allocate(sizeof(test_struct));
    ASSERT_NE(mem, nullptr);

    test_struct* obj = m_allocator->construct<test_struct>(mem, 42);
    EXPECT_EQ(obj->value, 42);

    m_allocator->destroy(obj);
    m_allocator->deallocate(mem);
}

// 测试 create 方法（分配并构造）
TEST_F(shared_memory_allocator_test, create)
{
    struct test_struct {
        int value;
        test_struct(int v) : value(v)
        {}
    };

    test_struct* obj = m_allocator->create<test_struct>(100);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value, 100);

    m_allocator->destroy_and_deallocate(obj);
}

// 测试 create_array 方法
TEST_F(shared_memory_allocator_test, create_array)
{
    struct test_struct {
        int value;
        test_struct() : value(0)
        {}
    };

    size_t       count = 5;
    test_struct* arr   = m_allocator->create_array<test_struct>(count);
    ASSERT_NE(arr, nullptr);

    for (size_t i = 0; i < count; ++i) {
        arr[i].value = static_cast<int>(i);
    }

    for (size_t i = 0; i < count; ++i) {
        EXPECT_EQ(arr[i].value, static_cast<int>(i));
    }

    m_allocator->destroy_and_deallocate_array(arr, count);
}

// 测试多次分配和释放
TEST_F(shared_memory_allocator_test, multiple_allocations)
{
    const int num_allocations = 10;
    void*     pointers[num_allocations];

    // 分配多个内存块
    for (int i = 0; i < num_allocations; ++i) {
        pointers[i] = m_allocator->allocate(100);
        ASSERT_NE(pointers[i], nullptr);
    }

    // 释放所有内存块
    for (int i = 0; i < num_allocations; ++i) {
        m_allocator->deallocate(pointers[i]);
    }
}

// 测试内存统计信息
TEST_F(shared_memory_allocator_test, memory_statistics)
{
    size_t initial_allocated = m_allocator->get_allocated_size();
    size_t initial_available = m_allocator->get_available_size();
    size_t total_size        = m_allocator->get_total_size();

    EXPECT_GE(total_size, initial_available);
    EXPECT_EQ(initial_allocated + initial_available, total_size);

    // 分配一些内存
    void* ptr1 = m_allocator->allocate(1024);
    void* ptr2 = m_allocator->allocate(2048);

    size_t after_alloc_allocated = m_allocator->get_allocated_size();
    size_t after_alloc_available = m_allocator->get_available_size();

    EXPECT_GT(after_alloc_allocated, initial_allocated);
    EXPECT_LT(after_alloc_available, initial_available);

    // 释放内存
    m_allocator->deallocate(ptr1);
    m_allocator->deallocate(ptr2);

    size_t after_dealloc_allocated = m_allocator->get_allocated_size();
    size_t after_dealloc_available = m_allocator->get_available_size();

    EXPECT_LT(after_dealloc_allocated, after_alloc_allocated);
    EXPECT_GT(after_dealloc_available, after_alloc_available);
}

// 测试内存不足时的分配
TEST_F(shared_memory_allocator_test, allocation_failure)
{
    // 尝试分配超过可用内存的大小
    size_t huge_size = m_allocator->get_total_size() * 2;
    void*  ptr       = m_allocator->allocate(huge_size);
    EXPECT_EQ(ptr, nullptr);
}

// 测试获取基地址
TEST_F(shared_memory_allocator_test, get_base_address)
{
    void* base = m_allocator->get_base_address();
    EXPECT_NE(base, nullptr);
    EXPECT_EQ(base, m_shm->get_data_address());
}

// 测试获取总大小
TEST_F(shared_memory_allocator_test, get_total_size)
{
    size_t total = m_allocator->get_total_size();
    EXPECT_GT(total, 0);
    EXPECT_EQ(total, m_shm->get_data_size());
}

// 测试释放空指针
TEST_F(shared_memory_allocator_test, deallocate_nullptr)
{
    // 释放空指针不应该崩溃
    m_allocator->deallocate(nullptr);
    // 验证状态没有改变
    EXPECT_EQ(m_allocator->get_allocated_size(), 0);
}

// 测试对齐分配
TEST_F(shared_memory_allocator_test, aligned_allocation)
{
    // 测试不同大小的分配，验证对齐
    for (size_t size = 1; size <= 100; size++) {
        void* ptr = m_allocator->allocate(size);
        if (ptr != nullptr) {
            // 验证指针是8字节对齐的
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
            EXPECT_EQ(addr % 8, 0) << "指针应该8字节对齐，大小: " << size;
            m_allocator->deallocate(ptr);
        }
    }
}

// 测试块分割和合并
TEST_F(shared_memory_allocator_test, block_split_and_merge)
{
    // 分配一个大块
    size_t large_size = m_allocator->get_total_size() / 4;
    void*  large_ptr  = m_allocator->allocate(large_size);
    ASSERT_NE(large_ptr, nullptr);

    size_t allocated_before = m_allocator->get_allocated_size();

    // 释放大块
    m_allocator->deallocate(large_ptr);

    // 分配多个小块，验证块被分割
    std::vector<void*> small_ptrs;
    for (int i = 0; i < 10; ++i) {
        void* ptr = m_allocator->allocate(100);
        if (ptr != nullptr) {
            small_ptrs.push_back(ptr);
        }
    }

    // 释放所有小块
    for (void* ptr : small_ptrs) {
        m_allocator->deallocate(ptr);
    }

    // 验证内存统计
    EXPECT_EQ(m_allocator->get_allocated_size(), 0);
    EXPECT_EQ(m_allocator->get_available_size(), m_allocator->get_total_size());
}

// 测试 allocate() 不满足分割条件的情况
TEST_F(shared_memory_allocator_test, AllocateWithoutSplit)
{
    // 分配一个大小刚好不满足分割条件的块
    // 分割条件：block->size > total_size + sizeof(memory_block_header) + 64
    // 所以如果 block->size <= total_size + sizeof(memory_block_header) + 64，就不会分割

    // 先分配一个大块，然后释放，再分配一个刚好不满足分割条件的大小
    size_t total_size  = m_allocator->get_total_size();
    size_t header_size = sizeof(shared_memory_allocator::memory_block_header);

    // 计算不满足分割条件的最大大小
    // 需要：block->size <= total_size + header_size + 64
    // 但 block->size = user_size + header_size
    // 所以：user_size + header_size <= total_size + header_size + 64
    // 即：user_size <= total_size + 64
    // 为了确保不分割，我们使用一个接近但不满足分割条件的大小
    size_t user_size = total_size / 2; // 使用一半大小，通常不会触发分割
    void*  ptr       = m_allocator->allocate(user_size);
    ASSERT_NE(ptr, nullptr);

    // 验证分配成功
    EXPECT_GT(m_allocator->get_allocated_size(), 0);

    m_allocator->deallocate(ptr);
}

// 测试 deallocate() 魔数不匹配的错误处理
TEST_F(shared_memory_allocator_test, DeallocateInvalidMagic)
{
    // 分配一块内存
    void* ptr = m_allocator->allocate(1024);
    ASSERT_NE(ptr, nullptr);

    // 获取块头部
    auto header = reinterpret_cast<shared_memory_allocator::memory_block_header*>(
        static_cast<char*>(ptr) - sizeof(shared_memory_allocator::memory_block_header));

    // 手动修改魔数
    constexpr uint32_t BLOCK_MAGIC = 0xB10C4;
    header->magic.store(0xDEADBEEF, std::memory_order_relaxed);

    // 尝试释放，应该输出错误日志但不崩溃
    m_allocator->deallocate(ptr);

    // 恢复魔数以便清理
    header->magic.store(BLOCK_MAGIC, std::memory_order_relaxed);
}

// 测试 find_free_block() 魔数不匹配的错误处理
TEST_F(shared_memory_allocator_test, FindFreeBlockInvalidMagic)
{
    // 分配一块内存
    void* ptr1 = m_allocator->allocate(1024);
    ASSERT_NE(ptr1, nullptr);

    // 释放它
    m_allocator->deallocate(ptr1);

    // 获取块头部
    auto header = reinterpret_cast<shared_memory_allocator::memory_block_header*>(
        static_cast<char*>(ptr1) - sizeof(shared_memory_allocator::memory_block_header));

    // 手动修改魔数
    constexpr uint32_t BLOCK_MAGIC = 0xB10C4;
    header->magic.store(0xDEADBEEF, std::memory_order_relaxed);

    // 尝试分配，应该输出错误日志并返回 nullptr
    void* ptr2 = m_allocator->allocate(512);
    // 由于魔数不匹配，find_free_block 会返回 nullptr
    // 但由于这是第一个块，可能会影响后续分配
    // 这里主要验证不会崩溃

    // 恢复魔数以便清理
    header->magic.store(BLOCK_MAGIC, std::memory_order_relaxed);
}

// 测试 merge_adjacent_blocks() 魔数不匹配的错误处理
TEST_F(shared_memory_allocator_test, MergeAdjacentBlocksInvalidMagic)
{
    // 分配两块内存
    void* ptr1 = m_allocator->allocate(1024);
    void* ptr2 = m_allocator->allocate(1024);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);

    // 获取第二块的头部
    auto header2 = reinterpret_cast<shared_memory_allocator::memory_block_header*>(
        static_cast<char*>(ptr2) - sizeof(shared_memory_allocator::memory_block_header));

    // 手动修改第二块的魔数
    constexpr uint32_t BLOCK_MAGIC = 0xB10C4;
    header2->magic.store(0xDEADBEEF, std::memory_order_relaxed);

    // 释放第一块，这会触发 merge_adjacent_blocks
    // 由于第二块的魔数不匹配，应该输出错误日志
    m_allocator->deallocate(ptr1);

    // 恢复魔数以便清理
    header2->magic.store(BLOCK_MAGIC, std::memory_order_relaxed);
    m_allocator->deallocate(ptr2);
}
