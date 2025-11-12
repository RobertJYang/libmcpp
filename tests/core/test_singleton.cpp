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

#include "mc/singleton.h"
#include <atomic>
#include <gtest/gtest.h>
#include <mutex>
#include <test_utilities/test_base.h>
#include <thread>
#include <vector>

namespace mc {
namespace test {

// 用于测试的简单类
class test_class {
public:
    test_class() : m_value(0), m_is_destroyed(nullptr) {
    }

    test_class(int value, bool* is_destroyed = nullptr)
        : m_value(value), m_is_destroyed(is_destroyed) {
    }

    ~test_class() {
        if (m_is_destroyed) {
            *m_is_destroyed = true;
        }
    }

    int get_value() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_value;
    }

    void set_value(int value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_value = value;
    }

    // 原子递增操作
    void increment() {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_value;
    }

private:
    int                m_value;
    bool*              m_is_destroyed;
    mutable std::mutex m_mutex; // 用于保护m_value的访问
};

// 用于测试多个标签的类
struct tag1 {};
struct tag2 {};

class destroy_test_class {
public:
    explicit destroy_test_class(bool* destroyed) : m_destroyed(destroyed) {
    }

    ~destroy_test_class() {
        if (m_destroyed) {
            *m_destroyed = true;
        }
    }

private:
    bool* m_destroyed;
};

class leaky_test_class {
public:
    explicit leaky_test_class(bool* destroyed) : m_destroyed(destroyed) {
    }

    ~leaky_test_class() {
        if (m_destroyed) {
            *m_destroyed = true;
        }
    }

private:
    bool* m_destroyed;
};

class singleton_test : public mc::test::TestBase {
public:
    void SetUp() override {
        singleton<test_class>::reset_for_test();
        singleton<test_class, tag1>::reset_for_test();
        singleton<test_class, tag2>::reset_for_test();
    }
};

TEST_F(singleton_test, LifecycleAndTryGet) {
    EXPECT_EQ(singleton<test_class>::try_get(), nullptr);

    auto& instance = singleton<test_class>::instance(42);
    EXPECT_EQ(singleton<test_class>::try_get(), &instance);
    EXPECT_EQ(instance.get_value(), 42);

    instance.set_value(100);
    EXPECT_EQ(singleton<test_class>::instance().get_value(), 100);

    bool destroyed = false;
    singleton<test_class>::reset_for_test();
    EXPECT_FALSE(singleton<test_class>::created());
    EXPECT_EQ(singleton<test_class>::try_get(), nullptr);

    auto tracked_creator = [&destroyed]() {
        return new test_class(55, &destroyed);
    };
    auto& tracked = singleton<test_class>::instance_with_creator(tracked_creator);
    EXPECT_EQ(singleton<test_class>::try_get(), &tracked);
    singleton<test_class>::reset_for_test();
    EXPECT_TRUE(destroyed);

    auto& recreated = singleton<test_class>::instance(84);
    EXPECT_EQ(recreated.get_value(), 84);
}

TEST_F(singleton_test, TaggedSingletonIsolation) {
    auto& tag1_instance = singleton<test_class, tag1>::instance(10);
    auto& tag2_instance = singleton<test_class, tag2>::instance(20);

    EXPECT_NE(&tag1_instance, &tag2_instance);
    EXPECT_EQ(tag1_instance.get_value(), 10);
    EXPECT_EQ(tag2_instance.get_value(), 20);

    tag1_instance.set_value(11);
    EXPECT_EQ(tag1_instance.get_value(), 11);
    EXPECT_EQ(tag2_instance.get_value(), 20);

    singleton<test_class, tag1>::reset_for_test();
    EXPECT_FALSE((singleton<test_class, tag1>::created()));
    EXPECT_TRUE((singleton<test_class, tag2>::created()));

    auto& new_tag1_instance = singleton<test_class, tag1>::instance(12);
    EXPECT_EQ(new_tag1_instance.get_value(), 12);
    EXPECT_EQ(tag2_instance.get_value(), 20);
}

TEST_F(singleton_test, CustomCreatorScenarios) {
    int  init_value      = 0;
    auto complex_creator = [&]() {
        auto* inst = new test_class(++init_value * 10);
        inst->set_value(inst->get_value() * 2);
        return inst;
    };

    auto& complex = singleton<test_class>::instance_with_creator(complex_creator);
    EXPECT_EQ(complex.get_value(), 20);
    singleton<test_class>::reset_for_test();

    auto simple_creator = []() {
        return new test_class(123);
    };
    auto& simple = singleton<test_class>::instance_with_creator(simple_creator);
    EXPECT_EQ(simple.get_value(), 123);
    singleton<test_class>::reset_for_test();
}

// 测试非泄露模式单例销毁
TEST_F(singleton_test, NonLeakySingleton) {
    // 跟踪销毁状态
    bool is_destroyed = false;

    // 创建非泄露模式单例
    auto creator = [&is_destroyed]() {
        return new test_class(42, &is_destroyed);
    };

    // 获取单例
    auto& instance = singleton<test_class>::instance_with_creator(creator);
    EXPECT_EQ(instance.get_value(), 42);

    // 销毁所有非泄露单例
    singleton<test_class>::reset_for_test();

    // 验证单例已被销毁
    EXPECT_TRUE(is_destroyed);
    EXPECT_FALSE(singleton<test_class>::created());
}

// 测试泄露模式单例
TEST_F(singleton_test, LeakySingleton) {
    // 跟踪销毁状态
    bool is_destroyed = false;

    // 创建泄露模式单例（使用新的singleton_leaky类）
    auto creator = [&is_destroyed]() {
        return new test_class(42, &is_destroyed);
    };

    // 获取单例
    auto& instance = singleton_leaky<test_class>::instance_with_creator(creator);
    EXPECT_EQ(instance.get_value(), 42);

    // 为了不内存泄漏，手动重置单例（仅用于测试）
    singleton_leaky<test_class>::reset_for_test();
    EXPECT_TRUE(is_destroyed);
}

// 复杂场景：并发环境下的单例创建和访问
TEST_F(singleton_test, ComplexConcurrentSingletonAccess) {
    const int num_threads           = 10;
    const int operations_per_thread = 100;

    std::vector<std::thread> threads;
    std::atomic<int>         total_increments{0};

    // 多个线程同时创建和访问单例
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&total_increments]() {
            // 所有线程共享同一个单例实例
            auto& instance = singleton<test_class>::instance(0);
            for (int j = 0; j < operations_per_thread; ++j) {
                instance.increment();
                total_increments++;
            }
        });
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    // 验证单例的值是正确的（所有线程的增量总和）
    auto& final_instance = singleton<test_class>::instance();
    EXPECT_EQ(final_instance.get_value(), total_increments.load());
    EXPECT_EQ(total_increments.load(), num_threads * operations_per_thread);
}

TEST_F(singleton_test, DestroyInstancesRunsDestructors) {
    bool destroyed = false;

    singleton<destroy_test_class>::instance(&destroyed);

    EXPECT_FALSE(destroyed);

    mc::singleton_manager::instance().destroy_instances();
    EXPECT_TRUE(destroyed);

    singleton<destroy_test_class>::reset_for_test();
}

TEST_F(singleton_test, LeakySingletonNotDestroyedByManager) {
    bool destroyed = false;

    singleton_leaky<leaky_test_class>::instance(&destroyed);

    mc::singleton_manager::instance().destroy_instances();
    EXPECT_FALSE(destroyed); // 泄露模式不会被 destroy_instances 销毁

    singleton_leaky<leaky_test_class>::reset_for_test();
    EXPECT_TRUE(destroyed); // reset_for_test 会真正销毁实例
}

} // namespace test
} // namespace mc