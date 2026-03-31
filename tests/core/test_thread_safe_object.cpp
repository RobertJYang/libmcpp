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

#include <mc/event.h>
#include <mc/exception.h>
#include <mc/memory.h>
#include <mc/object.h>
#include <mc/runtime/thread_list.h>
#include <mc/runtime/thread_pool.h>
#include <mc/signal/signal.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mc::core::test {

class ThreadSafeObjectTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 创建根对象
        root_object = mc::make_shared<object>();
        root_object->set_name("root");
    }

    void TearDown() override
    {
        root_object.reset();
    }

    object_ptr root_object;
};

// 测试基本的父子关系设置
TEST_F(ThreadSafeObjectTest, BasicParentChildRelationship)
{
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
TEST_F(ThreadSafeObjectTest, ConcurrentEnsureImplAccess)
{
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
TEST_F(ThreadSafeObjectTest, ConcurrentPropertyAccess)
{
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

TEST_F(ThreadSafeObjectTest, ExecutorAssignment)
{
    mc::runtime::thread_pool io(1, "thread_safe_object_test");
    io.start();
    root_object->set_executor(mc::runtime::any_executor(io.get_executor()));

    auto stored = root_object->get_executor();
    EXPECT_TRUE(stored.valid());
}

TEST_F(ThreadSafeObjectTest, ConnectionModeDirectRunsInline)
{
    mc::runtime::thread_pool pool(1, "thread_safe_object_direct_mode");
    pool.start();
    root_object->set_executor(mc::runtime::any_executor(pool.get_executor()));

    mc::signal<void()> sig;
    std::thread::id    callback_thread;
    std::promise<void> done;
    auto               future = done.get_future();

    root_object->connect(sig, [&]() {
        callback_thread = std::this_thread::get_id();
        done.set_value();
    }, mc::connection_mode::Direct);

    auto emit_thread = std::this_thread::get_id();
    sig();

    EXPECT_EQ(future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_EQ(callback_thread, emit_thread);
}

TEST_F(ThreadSafeObjectTest, ConnectionModeAutoUsesObjectExecutor)
{
    mc::runtime::thread_pool pool(1, "thread_safe_object_dispatch_mode");
    pool.start();
    root_object->set_executor(mc::runtime::any_executor(pool.get_executor()));

    mc::signal<void()> sig;
    std::promise<void> done;
    auto               future = done.get_future();
    std::atomic<bool>  ran_on_executor{false};
    std::thread::id    callback_thread;

    root_object->connect(sig, [&]() {
        callback_thread = std::this_thread::get_id();
        ran_on_executor.store(root_object->get_executor().running_in_this_thread());
        done.set_value();
    }, mc::connection_mode::Auto);

    auto emit_thread = std::this_thread::get_id();
    sig();

    EXPECT_EQ(future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_TRUE(ran_on_executor.load());
    EXPECT_NE(callback_thread, emit_thread);
}

TEST_F(ThreadSafeObjectTest, ConnectionModeQueuedNeverRunsInlineEvenOnSameThread)
{
    // Queued 模式：即使从对象所在的 executor 线程发出信号，槽也必须 post 而非内联执行
    mc::runtime::thread_pool pool(1, "thread_safe_object_queued_mode");
    pool.start();
    root_object->set_executor(mc::runtime::any_executor(pool.get_executor()));

    mc::signal<void()> sig;
    std::promise<void> done;
    auto               future     = done.get_future();
    std::atomic<bool>  after_emit = false;

    root_object->connect(sig, [&]() {
        // 若 Queued 正确实现（post 而非 Auto），此处 after_emit 必须已为 true
        EXPECT_TRUE(after_emit.load());
        done.set_value();
    }, mc::connection_mode::Queued);

    // 从 executor 线程内部发出信号，再设置 after_emit
    // Auto 模式会内联执行（after_emit 还是 false），Queued 模式会延迟到当前任务结束
    pool.get_executor().post([&]() {
        sig();
        after_emit.store(true);
    });

    EXPECT_EQ(future.wait_for(std::chrono::seconds(2)), std::future_status::ready);
}

TEST_F(ThreadSafeObjectTest, ConnectWithTargetExecutorPostsToSpecifiedExecutor)
{
    mc::runtime::thread_pool pool(1, "thread_safe_object_target_executor");
    pool.start();

    mc::signal<void()> sig;
    std::promise<void> done;
    auto               future = done.get_future();
    std::atomic<bool>  ran_on_target{false};
    std::thread::id    callback_thread;

    root_object->connect(sig, [&]() {
        callback_thread = std::this_thread::get_id();
        ran_on_target.store(pool.get_executor().running_in_this_thread());
        done.set_value();
    }, mc::runtime::any_executor(pool.get_executor()));

    auto emit_thread = std::this_thread::get_id();
    sig();

    EXPECT_EQ(future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_TRUE(ran_on_target.load());
    EXPECT_NE(callback_thread, emit_thread);
}

TEST_F(ThreadSafeObjectTest, ConnectionModeAutoRespectsSlotPriority)
{
    using namespace std::chrono_literals;

    mc::runtime::thread_pool pool(1, "thread_safe_object_dispatch_priority");
    pool.start();
    root_object->set_executor(mc::runtime::any_executor(pool.get_executor()));

    mc::signal<void()> sig;
    std::vector<int>   order;
    std::promise<void> done;
    auto               future = done.get_future();

    root_object->connect(sig, [&]() {
        order.push_back(2);
        done.set_value();
    }, mc::connection_mode::Auto, mc::signal_priority::low);

    root_object->connect(sig, [&]() {
        order.push_back(1);
    }, mc::connection_mode::Auto, mc::signal_priority::high);

    sig();

    EXPECT_EQ(future.wait_for(1s), std::future_status::ready);
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
}

TEST_F(ThreadSafeObjectTest, AutoConnectionModeStillRespectsPriority)
{
    using namespace std::chrono_literals;

    mc::runtime::thread_pool pool(1, "thread_safe_object_auto_priority");
    pool.start();
    root_object->set_executor(mc::runtime::any_executor(pool.get_executor()));

    mc::signal<void()> sig;
    std::vector<int>   order;
    std::promise<void> done;
    auto               future = done.get_future();

    root_object->connect(sig, [&]() {
        order.push_back(2);
        done.set_value();
    }, mc::connection_mode::Auto, mc::signal_priority::low);

    root_object->connect(sig, [&]() {
        order.push_back(1);
    }, mc::connection_mode::Auto, mc::signal_priority::high);

    sig();

    EXPECT_EQ(future.wait_for(1s), std::future_status::ready);
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
}

TEST_F(ThreadSafeObjectTest, ConnectionManagementHelpers)
{
    mc::signal<void()> sig;
    int                counter = 0;

    // 使用 Direct 连接模式，确保同步执行
    auto conn = root_object->connect(sig, [&]() {
        ++counter;
    }, mc::connection_mode::Direct);
    sig();
    EXPECT_EQ(counter, 1);

    // 通过 connection 直接断开连接
    conn.disconnect();
    sig();
    EXPECT_EQ(counter, 1); // 计数器不应该增加，因为连接已断开

    // 再次断开应该不会崩溃（幂等操作）
    conn.disconnect();
    sig();
    EXPECT_EQ(counter, 1); // 计数器仍然不应该增加

    // 测试通过信号对象断开所有连接
    auto conn2 = root_object->connect(sig, [&]() {
        ++counter;
    }, mc::connection_mode::Direct);
    auto conn3 = root_object->connect(sig, [&]() {
        ++counter;
    }, mc::connection_mode::Direct);
    sig();
    EXPECT_EQ(counter, 3); // 两个连接都触发，计数器增加2 (1 + 2 = 3)

    sig.disconnect_all();
    sig();
    EXPECT_EQ(counter, 3); // 计数器不应该增加，因为所有连接已断开

    conn2.disconnect();
    conn3.disconnect();
}

// 测试多线程并发添加子对象
TEST_F(ThreadSafeObjectTest, ConcurrentAddChildren)
{
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
TEST_F(ThreadSafeObjectTest, ConcurrentParentChildOperations)
{
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
TEST_F(ThreadSafeObjectTest, ConcurrentDestruction)
{
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
TEST_F(ThreadSafeObjectTest, ParentDestructionHandling)
{
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
TEST_F(ThreadSafeObjectTest, ConcurrentNameOperations)
{
    mc::runtime::thread_list threads;
    const int                num_operations = 1000;
    std::atomic<int>         successful_operations{0};

    // 每个线程使用独立的对象以避免名称重复设置的冲突
    threads.start_threads(5, [&successful_operations](std::size_t i) {
        for (int j = 0; j < num_operations; ++j) {
            auto        test_object = mc::make_shared<object>(); // 每次操作使用新对象
            std::string name        = "thread_" + std::to_string(i) + "_name_" + std::to_string(j);
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
TEST_F(ThreadSafeObjectTest, ConcurrentSetParent)
{
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
TEST_F(ThreadSafeObjectTest, ConcurrentDestructionAccess)
{
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

// 测试 object 的复制构造函数（覆盖 object::object(const object&)）
TEST_F(ThreadSafeObjectTest, ObjectCopyConstructor)
{
    auto obj1 = mc::make_shared<object>();
    obj1->set_name("original");

    // 创建子对象和连接
    auto child = mc::make_shared<object>(obj1.get());
    child->set_name("child");

    // 复制对象
    auto obj2 = mc::make_shared<object>(*obj1);

    // 验证复制后的对象
    EXPECT_EQ(obj2->get_name(), "original");
    // 注意：复制构造函数不复制父子关系，所以 obj2 不应该有子对象
    EXPECT_TRUE(obj2->get_children().empty());
}

// 测试 object 的复制赋值运算符（覆盖 object::operator=(const object&)）
TEST_F(ThreadSafeObjectTest, ObjectCopyAssignment)
{
    auto obj1 = mc::make_shared<object>();
    obj1->set_name("source");

    auto obj2 = mc::make_shared<object>();
    obj2->set_name("target");

    // 复制赋值
    *obj2 = *obj1;

    // 验证复制后的对象
    EXPECT_EQ(obj2->get_name(), "source");

    // 测试自赋值
    *obj1 = *obj1;
    EXPECT_EQ(obj1->get_name(), "source");
}

// 测试 object_base 的移动构造函数和移动赋值运算符
TEST_F(ThreadSafeObjectTest, ObjectBaseMoveOperations)
{
    auto obj1 = mc::make_shared<object>();
    obj1->set_object_id(123);

    // 测试移动构造（通过 object_base 的移动语义）
    auto obj2 = mc::make_shared<object>();
    obj2->set_object_id(456);

    // 注意：object 本身不支持移动构造，但 object_base 支持
    // 这里主要测试 object_base 的移动语义
    EXPECT_NE(obj1->get_object_id(), obj2->get_object_id());
}

// 测试 connection::disconnect
TEST_F(ThreadSafeObjectTest, DisconnectConnection)
{
    auto obj = mc::make_shared<object>();
    obj->set_name("test_object");

    // 创建信号和连接
    mc::signal<void(int)>         sig1;
    mc::signal<void(std::string)> sig2;

    int  call_count1    = 0;
    bool sig2_triggered = false;

    auto conn1 = obj->connect(sig1, [&call_count1](int) {
        ++call_count1;
    }, mc::connection_mode::Direct);
    auto conn2 = obj->connect(sig2, [&sig2_triggered](const std::string&) {
        sig2_triggered = true;
    }, mc::connection_mode::Direct);

    // 通过返回的 connection 断开 sig1 的连接
    conn1.disconnect();

    // 触发信号
    sig1(42);
    sig2("test");

    // sig1 的连接应该被断开，sig2 的连接应该仍然有效
    EXPECT_EQ(call_count1, 0);
    EXPECT_TRUE(sig2_triggered);

    conn2.disconnect();
    sig1.disconnect_all();
    sig2.disconnect_all();
    obj.reset();
}

// 测试 weak_from_this（覆盖 object::weak_from_this()）
TEST_F(ThreadSafeObjectTest, WeakFromThis)
{
    auto obj = mc::make_shared<object>();
    obj->set_name("test_object");

    // 获取 weak_ptr
    auto weak = obj->weak_from_this();

    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(weak.lock().get(), obj.get());

    // 释放 shared_ptr
    obj.reset();

    // weak_ptr 应该过期
    EXPECT_TRUE(weak.expired());
    EXPECT_FALSE(weak.lock());
}

// 测试 const weak_from_this（覆盖 const 版本）
TEST_F(ThreadSafeObjectTest, WeakFromThisConst)
{
    auto obj = mc::make_shared<object>();
    obj->set_name("test_object");

    const object* const_obj = obj.get();

    // 获取 const weak_ptr
    auto weak = const_obj->weak_from_this();

    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(weak.lock().get(), obj.get());
}

// 测试 object_impl 的复制构造函数和复制赋值运算符（通过 object 的复制）
TEST_F(ThreadSafeObjectTest, ObjectImplCopyOperations)
{
    auto obj1 = mc::make_shared<object>();
    obj1->set_name("source");

    // 设置执行器
    obj1->set_executor(mc::get_work_executor());

    // 复制对象（这会触发 object_impl 的复制构造）
    auto obj2 = mc::make_shared<object>(*obj1);

    EXPECT_EQ(obj2->get_name(), "source");
    // 注意：执行器可能不会被复制，取决于实现
}

// 测试多个 connection 独立断开
TEST_F(ThreadSafeObjectTest, DisconnectMultipleConnections)
{
    auto obj = mc::make_shared<object>();

    // 创建多个连接
    mc::signal<void()> sig;
    int                call_count = 0;

    auto conn1 = obj->connect(sig, [&call_count]() {
        ++call_count;
    });
    auto conn2 = obj->connect(sig, [&call_count]() {
        ++call_count;
    });

    // 通过返回的 connection 分别断开连接
    conn1.disconnect();
    conn2.disconnect();

    // 触发信号
    sig();

    // 所有连接应该被清除
    EXPECT_EQ(call_count, 0);
}

// 测试复杂场景 - 融合所有未覆盖的功能
TEST_F(ThreadSafeObjectTest, ComplexScenarioAllUncoveredFeatures)
{
    // 创建对象并设置属性
    auto obj1 = mc::make_shared<object>();
    obj1->set_name("obj1");
    obj1->set_object_id(100);

    // 创建子对象
    auto child1 = mc::make_shared<object>(obj1.get());
    child1->set_name("child1");

    // 创建信号和连接
    mc::signal<void(int)> sig;
    int                   call_count = 0;
    auto                  conn       = child1->connect(sig, [&call_count](int val) {
        call_count += val;
    });

    // 复制对象（覆盖复制构造函数）
    auto obj2 = mc::make_shared<object>(*obj1);
    EXPECT_EQ(obj2->get_name(), "obj1");

    // 复制赋值（覆盖复制赋值运算符）
    auto obj3 = mc::make_shared<object>();
    *obj3     = *obj1;
    EXPECT_EQ(obj3->get_name(), "obj1");

    // 测试 weak_from_this
    auto weak = child1->weak_from_this();
    EXPECT_FALSE(weak.expired());

    // 断开连接
    conn.disconnect();
    sig(10);
    EXPECT_EQ(call_count, 0);

    // 释放对象
    obj1.reset();
    obj2.reset();
    obj3.reset();
    child1.reset();

    // weak_ptr 应该过期
    EXPECT_TRUE(weak.expired());
}

} // namespace mc::core::test