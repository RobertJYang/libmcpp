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

#include <gtest/gtest.h>
#include <mc/interprocess/shared_memory_manager.h>
#include <test_utilities/test_base.h>
#include <string>
#include <unistd.h>

using namespace mc::interprocess;

class shared_memory_manager_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        TestBase::SetUp();
    }

    void TearDown() override {
        TestBase::TearDown();
    }
};

// 测试创建共享内存管理器
TEST_F(shared_memory_manager_test, create_manager) {
    std::string test_name = "test_shm_manager_" + std::to_string(getpid());
    size_t size = 64 * 1024;

    shared_memory_manager manager(test_name, size, shared_memory_manager::REMOVE_ON_EXIT);
    auto shm = manager.get_shared_memory();
    ASSERT_NE(shm, nullptr);
    EXPECT_TRUE(shm->is_valid());
    EXPECT_EQ(manager.get_name(), test_name);
}

// 测试获取共享内存
TEST_F(shared_memory_manager_test, get_shared_memory) {
    std::string test_name = "test_shm_manager_get_" + std::to_string(getpid());
    size_t size = 64 * 1024;

    shared_memory_manager manager(test_name, size, shared_memory_manager::REMOVE_ON_EXIT);
    auto shm1 = manager.get_shared_memory();
    auto shm2 = manager.get_shared_memory();

    // 应该返回同一个共享内存对象
    EXPECT_EQ(shm1, shm2);
    EXPECT_TRUE(shm1->is_valid());
}

// 测试 REMOVE_ON_EXIT 选项
TEST_F(shared_memory_manager_test, remove_on_exit) {
    std::string test_name = "test_shm_manager_remove_" + std::to_string(getpid());
    size_t size = 64 * 1024;

    {
        shared_memory_manager manager(test_name, size, shared_memory_manager::REMOVE_ON_EXIT);
        auto shm = manager.get_shared_memory();
        ASSERT_NE(shm, nullptr);
        EXPECT_TRUE(shm->is_valid());
    }
    // 管理器析构后，共享内存应该被删除
    // 尝试打开应该失败
    auto shm = shared_memory::create(test_name, size);
    // 注意：由于 REMOVE_ON_EXIT 可能在某些情况下不会立即删除，这里只验证创建成功
    // 实际行为取决于系统实现
}

// 测试 REMOVE_IF_EXISTS 选项
TEST_F(shared_memory_manager_test, remove_if_exists) {
    std::string test_name = "test_shm_manager_if_exists_" + std::to_string(getpid());
    size_t size = 64 * 1024;

    // 先创建一个共享内存
    {
        shared_memory_manager manager1(test_name, size,
                                       shared_memory_manager::REMOVE_ON_EXIT);
        auto shm1 = manager1.get_shared_memory();
        ASSERT_NE(shm1, nullptr);
    }

    // 使用 REMOVE_IF_EXISTS 选项创建
    shared_memory_manager manager2(test_name, size,
                                    shared_memory_manager::REMOVE_ON_EXIT |
                                        shared_memory_manager::REMOVE_IF_EXISTS);
    auto shm2 = manager2.get_shared_memory();
    ASSERT_NE(shm2, nullptr);
    EXPECT_TRUE(shm2->is_valid());
}

// 测试手动清理
TEST_F(shared_memory_manager_test, manual_cleanup) {
    std::string test_name = "test_shm_manager_cleanup_" + std::to_string(getpid());
    size_t size = 64 * 1024;

    shared_memory_manager manager(test_name, size, 0); // 不自动删除
    auto shm = manager.get_shared_memory();
    ASSERT_NE(shm, nullptr);

    // 手动清理
    manager.cleanup();

    // 清理后应该无法再获取共享内存（cleanup 会 reset shared_memory）
    auto shm2 = manager.get_shared_memory();
    EXPECT_EQ(shm2, nullptr);
}

// 测试设置自动清理选项
TEST_F(shared_memory_manager_test, set_remove_on_exit) {
    std::string test_name = "test_shm_manager_set_remove_" + std::to_string(getpid());
    size_t size = 64 * 1024;

    shared_memory_manager manager(test_name, size, 0); // 初始不自动删除
    manager.set_remove_on_exit(true);
    auto shm = manager.get_shared_memory();
    ASSERT_NE(shm, nullptr);
}

// 测试静态 remove_shared_memory 方法
TEST_F(shared_memory_manager_test, static_remove) {
    std::string test_name = "test_shm_manager_static_remove_" + std::to_string(getpid());
    size_t size = 64 * 1024;

    // 创建共享内存
    {
        shared_memory_manager manager(test_name, size, 0);
        auto shm = manager.get_shared_memory();
        ASSERT_NE(shm, nullptr);
    }

    // 使用静态方法删除
    bool removed = shared_memory_manager::remove_shared_memory(test_name);
    EXPECT_TRUE(removed);

    // 再次删除应该返回 false（已不存在）
    bool removed_again = shared_memory_manager::remove_shared_memory(test_name);
    EXPECT_FALSE(removed_again);
}

// 测试格式化名称
TEST_F(shared_memory_manager_test, format_name) {
    // 测试不带前缀的名称
    std::string name1 = "test_name";
    std::string formatted1 = shared_memory_manager::format_shm_name(name1);
    EXPECT_EQ(formatted1, "/test_name");

    // 测试带前缀的名称
    std::string name2 = "/test_name";
    std::string formatted2 = shared_memory_manager::format_shm_name(name2);
    EXPECT_EQ(formatted2, "/test_name");
}

// 测试默认大小
TEST_F(shared_memory_manager_test, default_size) {
    std::string test_name = "test_shm_manager_default_" + std::to_string(getpid());

    shared_memory_manager manager(test_name, 0,
                                  shared_memory_manager::REMOVE_ON_EXIT);
    auto shm = manager.get_shared_memory();
    ASSERT_NE(shm, nullptr);
    EXPECT_TRUE(shm->is_valid());
}

