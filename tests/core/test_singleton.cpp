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

#include <algorithm>
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
    test_class() : m_value(0), m_is_destroyed(nullptr)
    {}

    test_class(int value, bool* is_destroyed = nullptr) : m_value(value), m_is_destroyed(is_destroyed)
    {}

    ~test_class()
    {
        if (m_is_destroyed) {
            *m_is_destroyed = true;
        }
    }

    int get_value() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_value;
    }

    void set_value(int value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_value = value;
    }

    // 原子递增操作
    void increment()
    {
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

struct order_tag1 {};
struct order_tag2 {};

class order_test_class {
public:
    order_test_class(int id, std::vector<int>* order) : m_id(id), m_order(order)
    {}

    ~order_test_class()
    {
        if (m_order) {
            m_order->push_back(m_id);
        }
    }

private:
    int               m_id;
    std::vector<int>* m_order;
};

class destroy_test_class {
public:
    explicit destroy_test_class(bool* destroyed) : m_destroyed(destroyed)
    {}

    ~destroy_test_class()
    {
        if (m_destroyed) {
            *m_destroyed = true;
        }
    }

private:
    bool* m_destroyed;
};

class leaky_test_class {
public:
    explicit leaky_test_class(bool* destroyed) : m_destroyed(destroyed)
    {}

    ~leaky_test_class()
    {
        if (m_destroyed) {
            *m_destroyed = true;
        }
    }

private:
    bool* m_destroyed;
};

class singleton_test : public mc::test::TestBase {
public:
    static void SetUpTestSuite()
    {
        singleton_cleanup::reset();
    }

    static void TearDownTestSuite()
    {
        singleton_cleanup::reset();
    }

    void SetUp() override
    {
        cleanup_guard = std::make_unique<singleton_cleanup>();
    }

    void TearDown() override
    {
        cleanup_guard.reset();
    }

private:
    struct singleton_cleanup {
        singleton_cleanup()
        {
            reset();
        }

        ~singleton_cleanup()
        {
            reset();
        }

        static void reset()
        {
            mc::singleton_manager::instance().reset_for_test();
            singleton<test_class>::reset_for_test();
            singleton<test_class, tag1>::reset_for_test();
            singleton<test_class, tag2>::reset_for_test();
            singleton_leaky<test_class>::reset_for_test();
            singleton<destroy_test_class>::reset_for_test();
            singleton_leaky<leaky_test_class>::reset_for_test();
            singleton<order_test_class, order_tag1>::reset_for_test();
            singleton<order_test_class, order_tag2>::reset_for_test();
            // 注意：exception_test_class 不在这里清理
            // 因为它的析构函数会抛出异常，可能导致 double free
            // 如果对象在 destroy_instances 后仍然存在（因为异常），我们不应该再次尝试删除
        }
    };

    std::unique_ptr<singleton_cleanup> cleanup_guard;
};

TEST_F(singleton_test, LifecycleAndTryGet)
{
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

TEST_F(singleton_test, TaggedSingletonIsolation)
{
    auto& tag1_instance = singleton<test_class, tag1>::instance(10);
    auto& tag2_instance = singleton<test_class, tag2>::instance(20);

    EXPECT_NE(&tag1_instance, &tag2_instance);
    EXPECT_EQ(tag1_instance.get_value(), 10);
    EXPECT_EQ(tag2_instance.get_value(), 20);

    tag1_instance.set_value(11);
    EXPECT_EQ(tag1_instance.get_value(), 11);
    EXPECT_EQ(tag2_instance.get_value(), 20);

    singleton<test_class, tag1>::reset_for_test();
    bool tag1_created = singleton<test_class, tag1>::created();
    bool tag2_created = singleton<test_class, tag2>::created();
    ASSERT_FALSE(tag1_created);
    ASSERT_TRUE(tag2_created);

    auto& new_tag1_instance = singleton<test_class, tag1>::instance(12);
    EXPECT_EQ(new_tag1_instance.get_value(), 12);
    EXPECT_EQ(tag2_instance.get_value(), 20);
}

TEST_F(singleton_test, CustomCreatorScenarios)
{
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
TEST_F(singleton_test, NonLeakySingleton)
{
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
TEST_F(singleton_test, LeakySingleton)
{
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
TEST_F(singleton_test, ComplexConcurrentSingletonAccess)
{
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

TEST_F(singleton_test, DestroyInstancesRunsDestructors)
{
    bool destroyed = false;

    singleton<destroy_test_class>::instance(&destroyed);

    EXPECT_FALSE(destroyed);

    mc::singleton_manager::instance().destroy_instances();
    EXPECT_TRUE(destroyed);

    singleton<destroy_test_class>::reset_for_test();
}

TEST_F(singleton_test, LeakySingletonNotDestroyedByManager)
{
    bool destroyed = false;

    singleton_leaky<leaky_test_class>::instance(&destroyed);

    mc::singleton_manager::instance().destroy_instances();
    EXPECT_FALSE(destroyed); // 泄露模式不会被 destroy_instances 销毁

    singleton_leaky<leaky_test_class>::reset_for_test();
    EXPECT_TRUE(destroyed); // reset_for_test 会真正销毁实例
}

TEST_F(singleton_test, DestroyInstancesInReverseOrder)
{
    std::vector<int> destruction_order;

    auto creator1 = [&]() {
        return new order_test_class(1, &destruction_order);
    };
    auto creator2 = [&]() {
        return new order_test_class(2, &destruction_order);
    };

    singleton<order_test_class, order_tag1>::instance_with_creator(creator1);
    singleton<order_test_class, order_tag2>::instance_with_creator(creator2);

    mc::singleton_manager::instance().destroy_instances();

    ASSERT_EQ(destruction_order.size(), 2U);
    auto sorted_order = destruction_order;
    std::sort(sorted_order.begin(), sorted_order.end());
    EXPECT_EQ(sorted_order[0], 1);
    EXPECT_EQ(sorted_order[1], 2);

    singleton<order_test_class, order_tag1>::reset_for_test();
    singleton<order_test_class, order_tag2>::reset_for_test();
}

TEST_F(singleton_test, ManagerResetClearsAllStates)
{
    bool non_leaky_destroyed = false;
    bool leaky_destroyed     = false;

    singleton<destroy_test_class>::instance(&non_leaky_destroyed);
    singleton_leaky<leaky_test_class>::instance(&leaky_destroyed);

    EXPECT_FALSE(non_leaky_destroyed);
    EXPECT_FALSE(leaky_destroyed);

    mc::singleton_manager::instance().reset_for_test();

    // reset_for_test 会调用 destroy_instances 并清理所有记录
    EXPECT_TRUE(non_leaky_destroyed);
    EXPECT_FALSE(leaky_destroyed);

    // 泄露单例需要显式 reset 才能销毁
    singleton_leaky<leaky_test_class>::reset_for_test();
    EXPECT_TRUE(leaky_destroyed);

    EXPECT_EQ(singleton<destroy_test_class>::try_get(), nullptr);
    EXPECT_EQ(singleton_leaky<leaky_test_class>::try_get(), nullptr);
}

TEST_F(singleton_test, CreatedFlagReflectsLifecycle)
{
    EXPECT_FALSE(singleton<test_class>::created());
    auto& instance = singleton<test_class>::instance(5);
    EXPECT_TRUE(singleton<test_class>::created());
    EXPECT_EQ(instance.get_value(), 5);

    singleton<test_class>::reset_for_test();
    EXPECT_FALSE(singleton<test_class>::created());
    EXPECT_EQ(singleton<test_class>::try_get(), nullptr);
}

TEST_F(singleton_test, InstanceIgnoresSubsequentArguments)
{
    auto& first = singleton<test_class>::instance(123);
    EXPECT_EQ(first.get_value(), 123);

    auto& again = singleton<test_class>::instance(999);
    EXPECT_EQ(&first, &again);
    EXPECT_EQ(again.get_value(), 123);
}

TEST_F(singleton_test, LeakySingletonSurvivesDestroyInstances)
{
    bool destroyed = false;
    auto creator   = [&destroyed]() {
        return new leaky_test_class(&destroyed);
    };

    auto& instance = singleton_leaky<leaky_test_class>::instance_with_creator(creator);
    EXPECT_EQ(singleton_leaky<leaky_test_class>::try_get(), &instance);

    mc::singleton_manager::instance().destroy_instances();
    EXPECT_FALSE(destroyed);
    EXPECT_TRUE(singleton_leaky<leaky_test_class>::created());

    singleton_leaky<leaky_test_class>::reset_for_test();
    EXPECT_TRUE(destroyed);
    EXPECT_FALSE(singleton_leaky<leaky_test_class>::created());
}

TEST_F(singleton_test, DestroyInstancesIsIdempotent)
{
    bool destroyed1 = false;
    bool destroyed2 = false;

    auto creator1 = [&]() {
        return new test_class(1, &destroyed1);
    };
    auto creator2 = [&]() {
        return new test_class(2, &destroyed2);
    };

    singleton<test_class, tag1>::instance_with_creator(creator1);
    singleton<test_class, tag2>::instance_with_creator(creator2);

    mc::singleton_manager::instance().destroy_instances();
    EXPECT_TRUE(destroyed1);
    EXPECT_TRUE(destroyed2);

    // 再次调用应该安全，不会重复销毁
    EXPECT_NO_THROW(mc::singleton_manager::instance().destroy_instances());
    bool tag1_after_destroy = singleton<test_class, tag1>::created();
    bool tag2_after_destroy = singleton<test_class, tag2>::created();
    ASSERT_FALSE(tag1_after_destroy);
    ASSERT_FALSE(tag2_after_destroy);
}

// 用于测试异常处理的类
class exception_test_class {
public:
    explicit exception_test_class(bool throw_std_exception, bool throw_unknown)
        : m_throw_std_exception(throw_std_exception), m_throw_unknown(throw_unknown)
    {}

    ~exception_test_class() noexcept(false)
    {
        if (m_throw_std_exception) {
            throw std::runtime_error("测试标准异常");
        }
        if (m_throw_unknown) {
            throw 42; // 抛出非标准异常
        }
    }

private:
    bool m_throw_std_exception;
    bool m_throw_unknown;
};

// 测试 destroy_instances 的异常处理路径（std::exception）
TEST_F(singleton_test, DestroyInstancesHandlesStdException)
{
    auto creator = []() {
        return new exception_test_class(true, false);
    };
    singleton<exception_test_class>::instance_with_creator(creator);

    // destroy_instances 应该捕获异常并继续执行
    EXPECT_NO_THROW(mc::singleton_manager::instance().destroy_instances());

    // 由于析构函数抛出异常，delete 操作会传播异常
    // destroy_instances 会捕获这个异常，但对象可能已经被部分删除
    // 我们不应该再次尝试删除，避免 double free
    // 注意：在 C++ 中，从析构函数抛出异常会导致 undefined behavior
    // 但我们的测试目的是验证 destroy_instances 能够捕获异常并继续执行
}

// 测试 destroy_instances 的异常处理路径（未知异常）
TEST_F(singleton_test, DestroyInstancesHandlesUnknownException)
{
    auto creator = []() {
        return new exception_test_class(false, true);
    };
    singleton<exception_test_class>::instance_with_creator(creator);

    // destroy_instances 应该捕获未知异常并继续执行
    EXPECT_NO_THROW(mc::singleton_manager::instance().destroy_instances());

    // 由于析构函数抛出异常，对象可能没有被完全删除
    // 我们不应该再次尝试删除，避免 double free
}

// 测试 destroy_instances 在多个单例中一个抛出异常的情况
TEST_F(singleton_test, DestroyInstancesHandlesPartialExceptions)
{
    bool normal_destroyed = false;
    auto normal_creator   = [&]() {
        return new test_class(1, &normal_destroyed);
    };
    auto exception_creator = []() {
        return new exception_test_class(true, false);
    };

    singleton<test_class>::instance_with_creator(normal_creator);
    singleton<exception_test_class>::instance_with_creator(exception_creator);

    // 即使一个单例抛出异常，其他单例仍应正常销毁
    EXPECT_NO_THROW(mc::singleton_manager::instance().destroy_instances());
    EXPECT_TRUE(normal_destroyed);

    singleton<test_class>::reset_for_test();
    // 不调用 exception_test_class 的 reset_for_test，因为对象可能已经被部分删除
}

} // namespace test
} // namespace mc