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

#include <mc/core/object.h>
#include <mc/memory.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace mc::core::test {

class ThreadSafeObjectTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建根对象
        root_object = mc::make_shared<object>();
        root_object->set_name("root");
    }

    void TearDown() override {
        root_object.reset();
    }

    mc::shared_ptr<object> root_object;
};

// 测试基本的父子关系设置
TEST_F(ThreadSafeObjectTest, BasicParentChildRelationship) {
    auto child1 = mc::make_shared<object>(root_object.get());
    child1->set_name("child1");

    auto child2 = mc::make_shared<object>(root_object.get());
    child2->set_name("child2");

    // 验证父子关系
    EXPECT_EQ(child1->get_parent(), root_object.get());
    EXPECT_EQ(child2->get_parent(), root_object.get());

    // 验证子对象列表
    auto children = root_object->get_children();
    EXPECT_EQ(children.size(), 2);

    // 验证查找功能
    EXPECT_EQ(root_object->find_child("child1"), child1.get());
    EXPECT_EQ(root_object->find_child("child2"), child2.get());
    EXPECT_EQ(root_object->find_child("nonexistent"), nullptr);
}

// 测试多线程并发添加子对象
TEST_F(ThreadSafeObjectTest, ConcurrentAddChildren) {
    const int                num_threads         = 10;
    const int                children_per_thread = 50;
    std::vector<std::thread> threads;
    std::atomic<int>         created_count{0};

    // 启动多个线程同时添加子对象
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, &created_count]() {
            for (int i = 0; i < children_per_thread; ++i) {
                auto child = mc::make_shared<object>(root_object.get());
                child->set_name("thread_" + std::to_string(t) + "_child_" + std::to_string(i));
                created_count.fetch_add(1);
            }
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    // 验证所有子对象都被正确添加
    auto children = root_object->get_children();
    EXPECT_EQ(children.size(), num_threads * children_per_thread);
    EXPECT_EQ(created_count.load(), num_threads * children_per_thread);

    // 验证所有子对象的父指针都正确
    for (const auto& child : children) {
        EXPECT_EQ(child->get_parent(), root_object.get());
    }
}

// 测试并发销毁对象时的安全性
TEST_F(ThreadSafeObjectTest, ConcurrentDestruction) {
    const int                           num_children = 100;
    std::vector<mc::shared_ptr<object>> children;

    // 创建多个子对象
    for (int i = 0; i < num_children; ++i) {
        auto child = mc::make_shared<object>(root_object.get());
        child->set_name("child_" + std::to_string(i));
        children.push_back(child);
    }

    // 验证初始状态
    EXPECT_EQ(root_object->get_children().size(), num_children);

    std::vector<std::thread> threads;
    std::atomic<int>         removed_count{0};

    // 启动多个线程同时从父对象中移除子对象
    for (int t = 0; t < 5; ++t) {
        threads.emplace_back([&children, &removed_count, t]() {
            int start = (t * num_children) / 5;
            int end   = ((t + 1) * num_children) / 5;

            for (int i = start; i < end; ++i) {
                // 先从父对象中移除，然后释放引用
                children[i]->set_parent(nullptr);
                children[i].reset();
                removed_count.fetch_add(1);
            }
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    // 给一点时间让操作完成
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_EQ(removed_count.load(), num_children);

    // 验证根对象的子对象列表为空
    auto remaining_children = root_object->get_children();
    EXPECT_EQ(remaining_children.size(), 0);
}

// 测试父对象销毁时子对象的处理
TEST_F(ThreadSafeObjectTest, ParentDestructionHandling) {
    std::vector<mc::shared_ptr<object>> orphaned_children;

    {
        // 创建一个临时父对象
        auto temp_parent = mc::make_shared<object>();
        temp_parent->set_name("temp_parent");

        // 创建子对象
        for (int i = 0; i < 10; ++i) {
            auto child = mc::make_shared<object>(temp_parent.get());
            child->set_name("child_" + std::to_string(i));
            orphaned_children.push_back(child);
        }

        // 验证父子关系
        EXPECT_EQ(temp_parent->get_children().size(), 10);
        for (const auto& child : orphaned_children) {
            EXPECT_EQ(child->get_parent(), temp_parent.get());
        }

        // temp_parent 在此处析构
    }

    // 给一点时间让析构函数完成
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 验证所有子对象的父指针都被设置为nullptr
    for (const auto& child : orphaned_children) {
        EXPECT_EQ(child->get_parent(), nullptr);
    }
}

// 测试设置和获取名称的线程安全性
TEST_F(ThreadSafeObjectTest, ConcurrentNameOperations) {
    auto                     test_object = mc::make_shared<object>();
    std::vector<std::thread> threads;
    const int                num_operations = 1000;

    // 启动多个线程同时设置名称
    for (int t = 0; t < 5; ++t) {
        threads.emplace_back([test_object, t]() {
            for (int i = 0; i < num_operations; ++i) {
                std::string name = "thread_" + std::to_string(t) + "_name_" + std::to_string(i);
                test_object->set_name(name);

                // 同时读取名称
                auto current_name = test_object->get_name();
                EXPECT_FALSE(current_name.empty());
            }
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    // 最终名称应该是有效的
    auto final_name = test_object->get_name();
    EXPECT_FALSE(final_name.empty());
}

// 测试多线程设置父对象
TEST_F(ThreadSafeObjectTest, ConcurrentSetParent) {
    const int                           num_children = 100;
    std::vector<mc::shared_ptr<object>> children;
    std::vector<mc::shared_ptr<object>> parents;

    // 创建多个父对象
    for (int i = 0; i < 5; ++i) {
        auto parent = mc::make_shared<object>();
        parent->set_name("parent_" + std::to_string(i));
        parents.push_back(parent);
    }

    // 创建子对象（初始时没有父对象）
    for (int i = 0; i < num_children; ++i) {
        auto child = mc::make_shared<object>();
        child->set_name("child_" + std::to_string(i));
        children.push_back(child);
    }

    std::vector<std::thread> threads;

    // 启动多个线程同时设置父对象
    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([&children, &parents, t]() {
            int start = (t * num_children) / 10;
            int end   = ((t + 1) * num_children) / 10;

            for (int i = start; i < end; ++i) {
                // 随机选择一个父对象
                int parent_idx = i % parents.size();
                children[i]->set_parent(parents[parent_idx].get());
            }
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    // 验证所有子对象都有正确的父对象
    int total_children_count = 0;
    for (const auto& parent : parents) {
        auto parent_children = parent->get_children();
        total_children_count += parent_children.size();

        // 验证每个子对象的父指针
        for (const auto& child : parent_children) {
            EXPECT_EQ(child->get_parent(), parent.get());
        }
    }

    EXPECT_EQ(total_children_count, num_children);
}

} // namespace mc::core::test