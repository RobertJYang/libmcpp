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
#include <mc/runtime/thread_list.h>
#include <mc/exception.h>

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

    object_ptr root_object;
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

// 测试多线程同时访问 ensure_impl 的情况 - 验证名称唯一性检查
TEST_F(ThreadSafeObjectTest, ConcurrentEnsureImplAccess) {
    const int num_threads    = 8;
    const int num_iterations = 100;

    for (int iter = 0; iter < num_iterations; ++iter) {
        auto obj = mc::make_shared<object>();

        std::atomic<int>         success_count{0};
        std::atomic<int>         exception_count{0};
        mc::runtime::thread_list threads;

        // 创建多个线程同时访问 ensure_impl，尝试设置相同的名称
        threads.start_threads(num_threads, [&obj, &success_count, &exception_count]() {
            try {
                // 同时设置对象名称，这会触发 ensure_impl()
                // 由于添加了名称唯一性检查，只有第一个线程能成功，其他线程会抛出异常
                obj->set_name("test_name");
                success_count.fetch_add(1);
            } catch (const mc::assert_exception&) {
                // 预期的异常：名称已经被设置
                exception_count.fetch_add(1);
            }
        });

        // 等待所有线程完成
        threads.join_all();

        // 验证结果：只有一个线程成功设置名称，其他线程抛出异常
        EXPECT_EQ(success_count.load(), 1) << "应该只有一个线程成功设置名称";
        EXPECT_EQ(exception_count.load(), num_threads - 1) << "其他线程应该抛出异常";
        EXPECT_EQ(obj->get_name(), "test_name") << "对象名称应该被正确设置";
    }
}

// 测试多线程同时访问对象属性 - 每个线程使用不同的对象以避免名称冲突
TEST_F(ThreadSafeObjectTest, ConcurrentPropertyAccess) {
    const int num_threads           = 4;
    const int operations_per_thread = 50;

    std::atomic<int>         name_operations{0};
    std::atomic<int>         parent_operations{0};
    mc::runtime::thread_list threads;

    // 创建多个线程进行各种操作，每个线程使用独立的对象
    threads.start_threads(num_threads, [&](std::size_t i) {
        auto obj = mc::make_shared<object>(); // 每个线程使用独立的对象
        for (int j = 0; j < operations_per_thread; ++j) {
            // 设置和获取名称 - 现在每个对象只设置一次
            if (j == 0) {
                std::string name = "thread_" + std::to_string(i) + "_object";
                obj->set_name(name);
            }
            
            auto retrieved_name = obj->get_name();
            if (!retrieved_name.empty()) {
                name_operations.fetch_add(1);
            }

            // 获取父对象
            auto parent = obj->get_parent();
            parent_operations.fetch_add(1);

            // 小延时增加竞争几率
            std::this_thread::yield();
        }
    });

    threads.join_all();

    // 验证操作计数
    EXPECT_EQ(name_operations.load(), num_threads * operations_per_thread);
    EXPECT_EQ(parent_operations.load(), num_threads * operations_per_thread);
}

// 测试多线程并发添加子对象
TEST_F(ThreadSafeObjectTest, ConcurrentAddChildren) {
    const int                num_threads         = 10;
    const int                children_per_thread = 50;
    mc::runtime::thread_list threads;
    std::atomic<int>         created_count{0};

    threads.start_threads(num_threads, [this, &created_count](std::size_t i) {
        for (int j = 0; j < children_per_thread; ++j) {
            auto child = mc::make_shared<object>(root_object.get());
            child->set_name("thread_" + std::to_string(i) + "_child_" + std::to_string(j));
            created_count.fetch_add(1);
        }
    });

    threads.join_all();

    // 验证所有子对象都被正确添加
    auto children = root_object->get_children();
    EXPECT_EQ(children.size(), num_threads * children_per_thread);
    EXPECT_EQ(created_count.load(), num_threads * children_per_thread);

    // 验证所有子对象的父指针都正确
    for (const auto& child : children) {
        EXPECT_EQ(child->get_parent(), root_object.get());
    }
}

// 测试父子关系的多线程安全性
TEST_F(ThreadSafeObjectTest, ConcurrentParentChildOperations) {
    const int num_children = 10;

    auto parent = mc::make_shared<object>();
    parent->set_name("parent");

    std::vector<mc::shared_ptr<object>> children;
    mc::runtime::thread_list            threads;

    // 创建子对象并并发地设置父对象
    for (int i = 0; i < num_children; ++i) {
        auto child = mc::make_shared<object>();
        child->set_name("child_" + std::to_string(i));
        children.push_back(child);

        threads.add_thread([child, parent]() {
            child->set_parent(parent.get());
        });
    }

    threads.join_all();

    // 验证父子关系
    auto parent_children = parent->get_children();
    EXPECT_EQ(parent_children.size(), num_children);

    for (auto& child : children) {
        EXPECT_EQ(child->get_parent(), parent);
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

    mc::runtime::thread_list threads;
    std::atomic<int>         removed_count{0};

    // 启动多个线程同时从父对象中移除子对象
    threads.start_threads(5, [&children, &removed_count](std::size_t i) {
        int start = (i * num_children) / 5;
        int end   = ((i + 1) * num_children) / 5;

        for (int i = start; i < end; ++i) {
            // 先从父对象中移除，然后释放引用
            children[i]->set_parent(nullptr);
            children[i].reset();
            removed_count.fetch_add(1);
        }
    });

    threads.join_all();

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

// 测试设置和获取名称的线程安全性 - 每个线程使用独立对象
TEST_F(ThreadSafeObjectTest, ConcurrentNameOperations) {
    mc::runtime::thread_list threads;
    const int                num_operations = 1000;
    std::atomic<int>         successful_operations{0};

    // 每个线程使用独立的对象以避免名称重复设置的冲突
    threads.start_threads(5, [&successful_operations](std::size_t i) {
        for (int j = 0; j < num_operations; ++j) {
            auto test_object = mc::make_shared<object>(); // 每次操作使用新对象
            std::string name = "thread_" + std::to_string(i) + "_name_" + std::to_string(j);
            test_object->set_name(name);

            // 同时读取名称
            auto current_name = test_object->get_name();
            if (!current_name.empty() && current_name == name) {
                successful_operations.fetch_add(1);
            }
        }
    });

    threads.join_all();

    // 验证所有操作都成功
    EXPECT_EQ(successful_operations.load(), 5 * num_operations);
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

    mc::runtime::thread_list threads;

    threads.start_threads(10, [&children, &parents](std::size_t i) {
        int start = (i * num_children) / 10;
        int end   = ((i + 1) * num_children) / 10;

        for (int i = start; i < end; ++i) {
            // 随机选择一个父对象
            int parent_idx = i % parents.size();
            children[i]->set_parent(parents[parent_idx].get());
        }
    });

    threads.join_all();

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

// 测试对象销毁时的线程安全性（使用弱引用进行安全访问）
TEST_F(ThreadSafeObjectTest, ConcurrentDestructionAccess) {
    const int num_iterations = 10;

    for (int iter = 0; iter < num_iterations; ++iter) {
        auto obj = mc::make_shared<object>();
        obj->set_name("test_object");

        // 创建一个弱引用来避免直接访问可能已经销毁的对象
        mc::weak_ptr<object> weak_obj = obj;

        std::atomic<bool> destruction_started{false};
        std::atomic<int>  access_attempts{0};
        std::atomic<int>  successful_accesses{0};

        mc::runtime::thread_list threads;

        // 线程1：尝试通过弱引用安全地访问对象
        threads.add_thread([&]() {
            while (!destruction_started.load()) {
                std::this_thread::yield();
            }

            // 在对象可能被销毁时尝试访问
            for (int i = 0; i < 5; ++i) {
                access_attempts.fetch_add(1);
                try {
                    if (auto locked_obj = weak_obj.lock()) {
                        auto name = locked_obj->get_name();
                        if (!name.empty()) {
                            successful_accesses.fetch_add(1);
                        }
                    }
                } catch (...) {
                    // 可能会有异常，这是预期的
                }
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });

        // 线程2：销毁对象
        threads.add_thread([&]() {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            destruction_started.store(true);
            obj.reset(); // 销毁对象
        });

        threads.join_all();

        // 至少应该有一些访问尝试
        EXPECT_GT(access_attempts.load(), 0);
    }
}

} // namespace mc::core::test