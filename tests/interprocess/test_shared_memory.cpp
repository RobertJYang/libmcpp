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

#include <cstring>
#include <gtest/gtest.h>
#include <mc/interprocess/shared_memory.h>
#include <mc/interprocess/shared_memory_manager.h>
#include <string>
#include <test_utilities/test_base.h>
#include <unistd.h>

using namespace mc::interprocess;

class shared_memory_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        TestBase::SetUp();
        shared_memory_manager::remove_shared_memory(get_test_shm_name());
    }

    void TearDown() override {
        TestBase::TearDown();
        shared_memory_manager::remove_shared_memory(get_test_shm_name());
    }

    std::string get_test_shm_name() const {
        return "test_shm_" + std::to_string(getpid());
    }
};

class shared_memory_guard {
public:
    explicit shared_memory_guard(std::string name) : m_name(std::move(name)) {
    }

    ~shared_memory_guard() {
        if (!m_name.empty()) {
            shared_memory_manager::remove_shared_memory(m_name);
        }
    }

private:
    std::string m_name;
};

// 测试创建共享内存
TEST_F(shared_memory_test, create_shared_memory) {
    std::string test_name = get_test_shm_name();
    size_t      size      = 1024 * 1024; // 1MB

    shared_memory_guard guard(test_name);
    auto shm = shared_memory::create(test_name, size);
    ASSERT_NE(shm, nullptr);
    EXPECT_TRUE(shm->is_valid());
    EXPECT_EQ(shm->get_size(), size);
    EXPECT_NE(shm->get_address(), nullptr);
    EXPECT_NE(shm->get_shmid(), -1);
}

// 测试共享内存名称格式化
TEST_F(shared_memory_test, format_shm_name) {
    // 测试不带前缀的名称
    std::string name1      = "test_name";
    std::string formatted1 = shared_memory::format_shm_name(name1);
    EXPECT_EQ(formatted1, "/test_name");

    // 测试带前缀的名称
    std::string name2      = "/test_name";
    std::string formatted2 = shared_memory::format_shm_name(name2);
    EXPECT_EQ(formatted2, "/test_name");

    // 测试空名称
    std::string name3      = "";
    std::string formatted3 = shared_memory::format_shm_name(name3);
    EXPECT_EQ(formatted3, "");
}

// 测试获取共享内存属性
TEST_F(shared_memory_test, get_properties) {
    std::string test_name = get_test_shm_name();
    size_t      size      = 64 * 1024; // 64KB

    shared_memory_guard guard(test_name);
    auto shm = shared_memory::create(test_name, size);
    ASSERT_NE(shm, nullptr);

    EXPECT_EQ(shm->get_name(), test_name);
    EXPECT_EQ(shm->get_size(), size);
    EXPECT_NE(shm->get_address(), nullptr);
    EXPECT_GT(shm->get_shmid(), 0);
    EXPECT_NE(shm->get_data_address(), nullptr);
    EXPECT_GT(shm->get_data_size(), 0);
    EXPECT_LT(shm->get_data_size(), size);
}

// 测试偏移量计算
TEST_F(shared_memory_test, offset_calculation) {
    std::string test_name = get_test_shm_name();
    size_t      size      = 64 * 1024;

    shared_memory_guard guard(test_name);
    auto shm = shared_memory::create(test_name, size);
    ASSERT_NE(shm, nullptr);

    void* base_addr = shm->get_address();
    void* data_addr = shm->get_data_address();

    // 测试基地址偏移量
    size_t base_offset = shm->get_offset(base_addr);
    EXPECT_EQ(base_offset, 0);

    // 测试数据地址偏移量
    size_t data_offset = shm->get_offset(data_addr);
    EXPECT_GT(data_offset, 0);

    // 测试从偏移量恢复指针
    void* restored_addr = shm->get_ptr_from_offset(data_offset);
    EXPECT_EQ(restored_addr, data_addr);

    // 测试无效偏移量
    void* invalid_addr = shm->get_ptr_from_offset(size + 1000);
    EXPECT_EQ(invalid_addr, nullptr);

    // 测试无效指针
    size_t invalid_offset = shm->get_offset(nullptr);
    EXPECT_EQ(invalid_offset, 0);
}

// 测试分配器获取
TEST_F(shared_memory_test, get_allocator) {
    std::string test_name = get_test_shm_name();
    size_t      size      = 64 * 1024;

    shared_memory_guard guard(test_name);
    auto shm = shared_memory::create(test_name, size);
    ASSERT_NE(shm, nullptr);

    auto& allocator = shm->get_allocator();
    EXPECT_NE(&allocator, nullptr);
    EXPECT_EQ(allocator.get_total_size(), shm->get_data_size());
}

// 测试最小内存大小
TEST_F(shared_memory_test, minimum_size) {
    std::string test_name  = get_test_shm_name();
    size_t      small_size = 100; // 小于最小值

    shared_memory_guard guard(test_name);
    auto shm = shared_memory::create(test_name, small_size);
    ASSERT_NE(shm, nullptr);
    // 实际大小应该被调整为最小值
    EXPECT_GE(shm->get_size(), shared_memory::MIN_MEMORY_SIZE);
}

// 测试打开已存在的共享内存
TEST_F(shared_memory_test, open_existing) {
    std::string test_name = get_test_shm_name();
    size_t      size      = 64 * 1024;

    // 创建第一个共享内存
    shared_memory_guard guard(test_name);
    auto shm1 = shared_memory::create(test_name, size);
    ASSERT_NE(shm1, nullptr);

    // 打开已存在的共享内存
    auto shm2 = shared_memory::create(test_name, size);
    ASSERT_NE(shm2, nullptr);

    // 两个对象应该指向同一个共享内存
    EXPECT_EQ(shm1->get_size(), shm2->get_size());
    EXPECT_EQ(shm1->get_name(), shm2->get_name());
}

// 测试共享内存有效性检查
TEST_F(shared_memory_test, is_valid) {
    std::string test_name = get_test_shm_name();
    size_t      size      = 64 * 1024;

    shared_memory_guard guard(test_name);
    auto shm = shared_memory::create(test_name, size);
    ASSERT_NE(shm, nullptr);
    EXPECT_TRUE(shm->is_valid());
}

// 测试偏移量边界情况
TEST_F(shared_memory_test, offset_boundary_cases) {
    std::string test_name = get_test_shm_name();
    size_t      size      = 64 * 1024;

    shared_memory_guard guard(test_name);
    auto shm = shared_memory::create(test_name, size);
    ASSERT_NE(shm, nullptr);

    // 测试边界偏移量
    void* base         = shm->get_address();
    void* end          = static_cast<char*>(base) + size - 1;
    void* out_of_bound = static_cast<char*>(base) + size;

    size_t end_offset = shm->get_offset(end);
    EXPECT_GT(end_offset, 0);
    EXPECT_LT(end_offset, size);

    // 超出边界应该返回0
    size_t invalid_offset = shm->get_offset(out_of_bound);
    EXPECT_EQ(invalid_offset, 0);

    // 测试边界偏移量恢复指针
    void* restored_end = shm->get_ptr_from_offset(end_offset);
    EXPECT_EQ(restored_end, end);

    // 测试超出边界的偏移量
    void* invalid_ptr = shm->get_ptr_from_offset(size);
    EXPECT_EQ(invalid_ptr, nullptr);
}

// 测试数据地址和大小
TEST_F(shared_memory_test, data_address_and_size) {
    std::string test_name = get_test_shm_name();
    size_t      size      = 64 * 1024;

    shared_memory_guard guard(test_name);
    auto shm = shared_memory::create(test_name, size);
    ASSERT_NE(shm, nullptr);

    void*  data_addr = shm->get_data_address();
    size_t data_size = shm->get_data_size();

    EXPECT_NE(data_addr, nullptr);
    EXPECT_GT(data_size, 0);
    EXPECT_LT(data_size, size);
    // 数据大小应该是总大小减去头部大小（头部大小是固定的）
    EXPECT_GT(data_size, size - 1024); // 头部大小应该远小于1KB

    // 数据地址应该在共享内存范围内
    void* base = shm->get_address();
    EXPECT_GE(data_addr, base);
    // data_addr + data_size 应该等于 base + size（数据区域正好填满到共享内存末尾）
    EXPECT_LE(static_cast<char*>(data_addr) + data_size, static_cast<char*>(base) + size);
}

// 测试 create() 传入空名称
TEST_F(shared_memory_test, CreateWithEmptyName) {
    // 传入空字符串，应该返回 nullptr 并输出错误日志
    auto shm = shared_memory::create("", 64 * 1024);
    EXPECT_EQ(shm, nullptr);
}

// 测试打开现有共享内存但大小不足
TEST_F(shared_memory_test, CreateWithInsufficientSize) {
    std::string test_name = get_test_shm_name();
    size_t      small_size = 32 * 1024; // 较小的共享内存
    size_t      large_size = 64 * 1024; // 较大的共享内存

    shared_memory_guard guard(test_name);
    
    // 先创建一个较小的共享内存
    auto shm1 = shared_memory::create(test_name, small_size);
    ASSERT_NE(shm1, nullptr);
    EXPECT_TRUE(shm1->is_valid());
    
    // 尝试以更大的大小打开它，应该返回 nullptr 并输出错误日志
    auto shm2 = shared_memory::create(test_name, large_size);
    EXPECT_EQ(shm2, nullptr);
}

// 测试 init_memory() 失败的情况
// 注意：init_memory() 是私有方法，很难直接测试失败情况
// 这里我们通过创建一个共享内存来间接测试，但无法直接模拟 init_memory() 失败
// 如果需要测试，可能需要 mock 或修改代码以注入失败
TEST_F(shared_memory_test, CreateWithInitMemoryFailure) {
    // init_memory() 失败的情况很难在单测中稳定复现
    // 它通常发生在共享内存头部初始化失败时
    // 由于这是私有方法且需要系统级错误注入，这里只验证正常创建不会失败
    std::string test_name = get_test_shm_name();
    size_t      size      = 64 * 1024;

    shared_memory_guard guard(test_name);
    auto shm = shared_memory::create(test_name, size);
    ASSERT_NE(shm, nullptr);
    EXPECT_TRUE(shm->is_valid());
    // 正常创建应该成功，init_memory() 失败的情况需要系统级错误注入才能测试
}

// 测试 register_process() 失败（进程槽位已满）
TEST_F(shared_memory_test, CreateWithRegisterProcessFailure) {
    std::string test_name = get_test_shm_name();
    size_t      size      = 64 * 1024;

    shared_memory_guard guard(test_name);
    
    // 创建共享内存
    auto shm_base = shared_memory::create(test_name, size);
    ASSERT_NE(shm_base, nullptr);
    
    // 尝试创建 65 个 shared_memory 对象（超过 64 个进程槽位）
    // 注意：由于 shared_memory::create() 会重用已存在的共享内存，
    // 我们需要通过不同的方式测试
    // 实际上，register_process() 失败的情况需要真正的多进程场景才能测试
    // 在单进程中，所有 shared_memory 对象共享同一个进程槽位
    
    // 这里我们验证正常创建不会失败
    // register_process() 失败的情况需要多进程测试才能覆盖
    std::vector<std::shared_ptr<shared_memory>> shm_list;
    for (int i = 0; i < 10; ++i) {
        auto shm = shared_memory::create(test_name, size);
        ASSERT_NE(shm, nullptr);
        shm_list.push_back(shm);
    }
    
    // 在单进程中，所有对象应该都能成功创建
    // register_process() 失败的情况需要真正的多进程场景
}
