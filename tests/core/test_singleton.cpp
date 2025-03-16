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

#include "mc/core/singleton.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <mutex>

namespace mc {
namespace test {

// 用于测试的简单类
class test_class {
public:
    test_class() : m_value(0), m_is_destroyed(nullptr) {}
    
    test_class(int value, bool* is_destroyed = nullptr) 
        : m_value(value), m_is_destroyed(is_destroyed) {}
    
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
    int m_value;
    bool* m_is_destroyed;
    mutable std::mutex m_mutex;  // 用于保护m_value的访问
};

// 用于测试多个标签的类
struct tag1 {};
struct tag2 {};

// 基本单例测试
TEST(SingletonTest, BasicFunctionality) {
    // 测试前重置所有单例
    reset_singletons_for_test();
    
    // 获取单例
    auto& instance = singleton<test_class>::instance(42);
    
    // 验证单例值
    EXPECT_EQ(instance.get_value(), 42);
    
    // 修改单例值
    instance.set_value(100);
    
    // 再次获取单例，验证是同一个实例
    auto& instance2 = singleton<test_class>::instance();
    EXPECT_EQ(instance2.get_value(), 100);
    
    // 验证两个实例是同一个地址
    EXPECT_EQ(&instance, &instance2);
}

// 测试自定义创建函数
TEST(SingletonTest, CustomCreator) {
    // 测试前重置所有单例
    reset_singletons_for_test();
    
    // 使用自定义创建函数获取单例
    auto creator = []() { return new test_class(123); };
    auto& instance = singleton<test_class>::instance_with_creator(creator);
    
    // 验证单例值
    EXPECT_EQ(instance.get_value(), 123);
}

// 测试多个标签的单例
TEST(SingletonTest, MultipleTaggedSingletons) {
    // 测试前重置所有单例
    reset_singletons_for_test();
    
    // 创建两个不同标签的单例
    auto& instance1 = singleton<test_class, tag1>::instance(1);
    auto& instance2 = singleton<test_class, tag2>::instance(2);
    
    // 验证它们是不同的实例
    EXPECT_NE(&instance1, &instance2);
    
    // 验证它们的值
    EXPECT_EQ(instance1.get_value(), 1);
    EXPECT_EQ(instance2.get_value(), 2);
}

// 测试非泄露模式单例销毁
TEST(SingletonTest, NonLeakySingleton) {
    // 测试前重置所有单例
    reset_singletons_for_test();
    
    // 跟踪销毁状态
    bool is_destroyed = false;
    
    // 创建非泄露模式单例
    auto creator = [&is_destroyed]() { return new test_class(42, &is_destroyed); };
    
    // 获取单例
    auto& instance = singleton<test_class>::instance_with_creator(creator);
    EXPECT_EQ(instance.get_value(), 42);
    
    // 销毁所有非泄露单例
    singleton_manager::instance().destroy_instances();
    
    // 验证单例已被销毁
    EXPECT_TRUE(is_destroyed);
    EXPECT_FALSE(singleton<test_class>::created());
}

// 测试泄露模式单例
TEST(SingletonTest, LeakySingleton) {
    // 测试前重置所有单例
    reset_singletons_for_test();
    
    // 跟踪销毁状态
    bool is_destroyed = false;
    
    // 创建泄露模式单例（使用新的singleton_leaky类）
    auto creator = [&is_destroyed]() { return new test_class(42, &is_destroyed); };
    
    // 获取单例
    auto& instance = singleton_leaky<test_class>::instance_with_creator(creator);
    EXPECT_EQ(instance.get_value(), 42);
    
    // 销毁所有非泄露单例
    singleton_manager::instance().destroy_instances();
    
    // 验证单例没有被销毁
    EXPECT_FALSE(is_destroyed);
    EXPECT_TRUE(singleton_leaky<test_class>::created());
    
    // 为了不内存泄漏，手动重置单例（仅用于测试）
    singleton_leaky<test_class>::reset_for_test();
    EXPECT_TRUE(is_destroyed);
}

// 测试并发访问
TEST(SingletonTest, ConcurrentAccess) {
    // 测试前重置所有单例
    reset_singletons_for_test();
    
    // 创建多个线程同时访问单例
    constexpr int num_threads = 10;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([]() {
            auto& instance = singleton<test_class>::instance(0);
            // 在单例上执行一些操作
            for (int j = 0; j < 100; ++j) {
                instance.increment();  // 使用原子递增操作
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证最终值
    auto& instance = singleton<test_class>::instance();
    EXPECT_EQ(instance.get_value(), num_threads * 100);
}

// 测试重置后重新创建
TEST(SingletonTest, RecreateAfterReset) {
    // 测试前重置所有单例
    reset_singletons_for_test();
    
    // 创建单例
    bool is_destroyed = false;
    {
        auto& instance1 = singleton<test_class>::instance(42, &is_destroyed);
        EXPECT_EQ(instance1.get_value(), 42);
    }
    
    // 重置单例
    reset_singletons_for_test();
    
    // 验证单例已被销毁
    EXPECT_TRUE(is_destroyed);
    EXPECT_FALSE(singleton<test_class>::created());
    
    // 重新创建单例
    auto& instance2 = singleton<test_class>::instance(84);
    
    // 验证是新的实例
    EXPECT_EQ(instance2.get_value(), 84);
}

// 测试 try_get 方法
TEST(SingletonTest, TryGetMethod) {
    // 测试前重置所有单例
    reset_singletons_for_test();
    
    // 未创建时，try_get 应返回 nullptr
    auto* ptr1 = singleton<test_class>::try_get();
    EXPECT_EQ(ptr1, nullptr);
    
    // 创建单例
    auto& instance = singleton<test_class>::instance(42);
    
    // 创建后，try_get 应返回有效指针
    auto* ptr2 = singleton<test_class>::try_get();
    EXPECT_EQ(ptr2, &instance);
}

// 演示正常使用和泄露模式的单例
TEST(SingletonTest, LeakyAndNonLeakySingletons) {
    // 测试前重置所有单例
    reset_singletons_for_test();
    
    // 测试非泄露单例（默认模式）
    bool db_destroyed = false;
    auto creator = [&db_destroyed]() { return new test_class(42, &db_destroyed); };
    
    {
        // 使用非泄露单例（直接使用singleton类）
        auto& instance = singleton<test_class>::instance_with_creator(creator);
        EXPECT_EQ(instance.get_value(), 42);
        
        // 重置所有单例
        reset_singletons_for_test();
        
        // 验证单例已被销毁
        EXPECT_TRUE(db_destroyed);
        EXPECT_FALSE(singleton<test_class>::created());
    }
    
    // 重置标记
    db_destroyed = false;
    
    // 测试泄露单例
    {
        // 使用泄露单例（使用singleton_leaky类）
        auto& instance = singleton_leaky<test_class>::instance_with_creator(creator);
        EXPECT_EQ(instance.get_value(), 42);
        
        // 重置所有单例
        reset_singletons_for_test();
        
        // 验证单例未被销毁（泄露模式）
        EXPECT_FALSE(db_destroyed);
        EXPECT_TRUE(singleton_leaky<test_class>::created());
        
        // 手动重置单例（仅用于测试）
        singleton_leaky<test_class>::reset_for_test();
        
        // 验证单例现在已被销毁
        EXPECT_TRUE(db_destroyed);
        EXPECT_FALSE(singleton_leaky<test_class>::created());
    }
}

// 测试使用多标签创建多个同类型单例
TEST(SingletonTest, MultipleTaggedInstances) {
    // 测试前重置所有单例
    reset_singletons_for_test();
    
    // 定义两个标签类型
    struct ErrorLogTag {};
    struct InfoLogTag {};
    
    // 使用不同标签创建不同的单例实例
    auto& instance1 = singleton<test_class, ErrorLogTag>::instance(10);
    auto& instance2 = singleton<test_class, InfoLogTag>::instance(20);
    
    // 验证是不同的实例
    EXPECT_NE(&instance1, &instance2);
    
    // 验证值正确
    EXPECT_EQ(instance1.get_value(), 10);
    EXPECT_EQ(instance2.get_value(), 20);
    
    // 修改其中一个实例的值，确认不影响另一个
    instance1.set_value(30);
    EXPECT_EQ(instance1.get_value(), 30);
    EXPECT_EQ(instance2.get_value(), 20);
}

// 测试使用自定义创建器
TEST(SingletonTest, CustomCreatorWithOptions) {
    // 测试前重置所有单例
    reset_singletons_for_test();
    
    // 自定义创建器，可以进行复杂的初始化
    auto creator = []() {
        auto* instance = new test_class(100);
        // 可以在此执行复杂初始化
        instance->set_value(instance->get_value() * 2);
        return instance;
    };
    
    // 获取单例实例
    auto& instance = singleton<test_class>::instance_with_creator(creator);
    
    // 验证复杂初始化正确执行
    EXPECT_EQ(instance.get_value(), 200);
}

} // namespace test
} // namespace mc 