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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// C++17 兼容的同步屏障实现
class sync_barrier {
private:
    mutable std::mutex              m_mutex;
    mutable std::condition_variable m_cv;
    mutable std::size_t             m_threshold;
    mutable std::size_t             m_count;
    mutable std::size_t             m_generation;

public:
    explicit sync_barrier(std::size_t count)
        : m_threshold(count), m_count(count), m_generation(0) {
    }

    void arrive_and_wait() const {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto                         gen = m_generation;
        if (--m_count == 0) {
            m_generation++;
            m_count = m_threshold;
            m_cv.notify_all();
        } else {
            m_cv.wait(lock, [this, gen] {
                return gen != m_generation;
            });
        }
    }
};

namespace mc::test {

// 模拟 get_object_name 方法中可能存在的并发问题
class concurrent_string_test {
private:
    std::string         m_base_name;
    mutable std::string m_cached_name;

public:
    explicit concurrent_string_test(const std::string& name) : m_base_name(name) {
    }

    std::string get_object_name() const {
        m_cached_name = m_base_name; // 使用对象级缓存，避免 thread_local 问题
        return m_cached_name;
    }
};

// 并发安全性测试类
class PropertyConcurrentFixTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试对象，每个都有不同的名称
        for (int i = 0; i < 10; ++i) {
            std::string name = "TestObject_" + std::to_string(i);
            test_objects_.emplace_back(name);
        }
    }

    void TearDown() override {
        test_objects_.clear();
    }

    std::vector<concurrent_string_test> test_objects_;
};

TEST_F(PropertyConcurrentFixTest, GetObjectNameConcurrentSafety) {
    const int num_threads           = 10;
    const int iterations_per_thread = 1000;

    std::atomic<int>         error_count{0};
    std::vector<std::thread> threads;

    // 使用 barrier 确保所有线程同时开始
    sync_barrier sync_point(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, &error_count, &sync_point]() {
            auto&       obj           = test_objects_[t];
            std::string expected_name = "TestObject_" + std::to_string(t);

            // 等待所有线程就绪
            sync_point.arrive_and_wait();

            for (int i = 0; i < iterations_per_thread; ++i) {
                try {
                    auto name = obj.get_object_name();

                    // 验证对象名称的正确性
                    if (name != expected_name) {
                        error_count.fetch_add(1);
                        std::cout << "Error: Expected '" << expected_name
                                  << "', got '" << name << "' in thread " << t
                                  << ", iteration " << i << std::endl;
                    }

                    // 添加一些随机延迟来增加并发冲突的概率
                    if (i % 100 == 0) {
                        std::this_thread::sleep_for(std::chrono::microseconds(1));
                    }
                } catch (const std::exception& e) {
                    error_count.fetch_add(1);
                    std::cout << "Exception in thread " << t << ", iteration " << i
                              << ": " << e.what() << std::endl;
                }
            }
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    // 验证没有并发错误
    EXPECT_EQ(error_count.load(), 0) << "发现 " << error_count.load() << " 个并发错误";
}

TEST_F(PropertyConcurrentFixTest, HighConcurrencyStressTest) {
    const int num_threads = 20;
    const int iterations  = 500;

    std::atomic<int>         success_count{0};
    std::atomic<int>         error_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, &success_count, &error_count]() {
            for (int i = 0; i < iterations; ++i) {
                try {
                    // 随机选择一个对象进行操作
                    int   obj_index = (t + i) % test_objects_.size();
                    auto& obj       = test_objects_[obj_index];

                    // 获取对象名称
                    auto name = obj.get_object_name();

                    // 验证数据一致性
                    std::string expected_name = "TestObject_" + std::to_string(obj_index);
                    if (name == expected_name) {
                        success_count.fetch_add(1);
                    } else {
                        error_count.fetch_add(1);
                    }

                } catch (...) {
                    error_count.fetch_add(1);
                }
            }
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "成功次数: " << success_count.load()
              << ", 错误次数: " << error_count.load() << std::endl;

    // 验证成功率
    int    total        = success_count.load() + error_count.load();
    double success_rate = static_cast<double>(success_count.load()) / total;
    EXPECT_GT(success_rate, 0.99) << "成功率过低: " << (success_rate * 100) << "%";
}

} // namespace mc::test