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

#include <gtest/gtest.h>

#include <mc/future.h>
#include <mc/runtime/thread_list.h>

class state_pool_test : public ::testing::Test {
protected:
    void SetUp() override {
        // 重置池配置
        mc::futures::state_pool_config config;
        config.max_count_per_pool = 10;
        config.max_pool_count     = 5;
        config.max_cacheable_size = 256;

        mc::futures::state_pool::instance().set_config(config);
    }

    void TearDown() override {
        // 清理池
        mc::futures::state_pool::instance().clear_all_pools();
    }

    boost::asio::io_context io_context_;
};

// 基本功能测试
TEST_F(state_pool_test, basic_pool_functionality) {
    auto& pool = mc::futures::state_pool::instance();

    // 获取初始统计
    auto initial_stats = pool.get_stats();
    EXPECT_EQ(initial_stats.total_global_states, 0);
    EXPECT_EQ(initial_stats.total_pools, 0);

    // 创建一些 promise/future 对
    std::vector<mc::promise<int, boost::asio::io_context::executor_type>> promises;
    std::vector<mc::future<int, boost::asio::io_context::executor_type>>  futures;

    for (int i = 0; i < 5; ++i) {
        auto promise = mc::make_promise<int>(io_context_);
        auto future  = promise.get_future();

        promises.push_back(std::move(promise));
        futures.push_back(std::move(future));
    }

    // 设置值并完成 future
    for (int i = 0; i < 5; ++i) {
        promises[i].set_value(i * 10);
        EXPECT_EQ(futures[i].get(), i * 10);
    }

    // 清理引用，让 State 回到池中
    promises.clear();
    futures.clear();

    // 检查池统计
    auto final_stats = pool.get_stats();
    EXPECT_EQ(final_stats.total_global_states, 5) << "应该有状态被缓存";
    EXPECT_GT(final_stats.total_pools, 0) << "应该创建了缓存池";
}

// 池重用测试
TEST_F(state_pool_test, pool_reuse) {
    auto& pool = mc::futures::state_pool::instance();

    // 创建并完成一个 future
    {
        auto promise = mc::make_promise<std::string>(io_context_);
        auto future  = promise.get_future();
        promise.set_value("test");
        EXPECT_EQ(future.get(), "test");
    }

    // 再次创建相同类型的 future，应该重用池中的 State
    {
        auto promise = mc::make_promise<std::string>(io_context_);
        auto future  = promise.get_future();
        promise.set_value("reused");
        EXPECT_EQ(future.get(), "reused");
    }

    // 验证池中有缓存的 State
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_pools, 1) << "应该只有一个类型的池";
    EXPECT_EQ(stats.total_global_states, 1) << "应该有状态被缓存";
}

// 池大小限制测试
TEST_F(state_pool_test, pool_size_limit) {
    auto& pool = mc::futures::state_pool::instance();

    // 设置较小的池大小
    mc::futures::state_pool_config config;
    config.max_count_per_pool = 2;
    pool.set_config(config);

    // 创建超过池大小的 future
    std::vector<mc::promise<int, boost::asio::io_context::executor_type>> promises;
    std::vector<mc::future<int, boost::asio::io_context::executor_type>>  futures;

    for (int i = 0; i < 5; ++i) {
        auto promise = mc::make_promise<int>(io_context_);
        auto future  = promise.get_future();

        promises.push_back(std::move(promise));
        futures.push_back(std::move(future));
    }

    // 完成所有 future
    for (int i = 0; i < 5; ++i) {
        promises[i].set_value(i);
        EXPECT_EQ(futures[i].get(), i);
    }

    // 清理引用
    promises.clear();
    futures.clear();

    // 检查池大小不超过限制
    auto stats = pool.get_stats();
    EXPECT_LE(stats.total_global_states, 2) << "缓存的状态数不应超过池大小限制";
}

// 不同大小类型测试
TEST_F(state_pool_test, different_size_types) {
    auto& pool = mc::futures::state_pool::instance();

    // 清理初始状态
    pool.clear_all_pools();

    // 创建不同大小的类型的 future
    {
        auto char_promise = mc::make_promise<char>(io_context_); // 1 字节
        auto char_future  = char_promise.get_future();
        char_promise.set_value('A');
        EXPECT_EQ(char_future.get(), 'A');
    }

    {
        auto int64_promise = mc::make_promise<std::int64_t>(io_context_); // 8 字节
        auto int64_future  = int64_promise.get_future();
        int64_promise.set_value(123456789L);
        EXPECT_EQ(int64_future.get(), 123456789L);
    }

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_pools, 1) << "缓存池按8字节对齐，应该只有一个";
}

// 相同大小类型共享池测试
TEST_F(state_pool_test, same_size_types_share_pool) {
    auto& pool = mc::futures::state_pool::instance();

    auto config = pool.get_config();
    pool.set_config(config);

    // 清理初始状态
    pool.clear_all_pools();

    static_assert(sizeof(int64_t) == sizeof(double), "int64_t 和 double 必须同样大小才能测试");

    // 创建并销毁 int future
    {
        auto int_promise = mc::make_promise<int64_t>(io_context_);
        auto int_future  = int_promise.get_future();
        int_promise.set_value(42);
        EXPECT_EQ(int_future.get(), 42);
    }

    auto stats_after_int = pool.get_stats();
    EXPECT_GT(stats_after_int.total_global_states, 0) << "int future 应该创建状态";

    // 创建 double future，应该重用 int64_t 的缓存
    {
        auto double_promise = mc::make_promise<double>(io_context_);
        auto double_future  = double_promise.get_future();
        double_promise.set_value(3.14);
        EXPECT_FLOAT_EQ(double_future.get(), 3.14);
    }

    auto stats_after_float = pool.get_stats();
    EXPECT_EQ(stats_after_float.total_pools, 1) << "int64_t 和 double 应该共享同一个池";
    EXPECT_EQ(stats_after_float.total_global_states, 1) << "int64_t 和 double 应该共享同一个状态";
}

// 配置测试
TEST_F(state_pool_test, pool_config) {
    auto& pool = mc::futures::state_pool::instance();

    mc::futures::state_pool_config config;
    config.max_count_per_pool = 500;
    config.max_pool_count     = 20;
    config.max_cacheable_size = 1024;

    pool.set_config(config);

    const auto& retrieved_config = pool.get_config();
    EXPECT_EQ(retrieved_config.max_count_per_pool, 500);
    EXPECT_EQ(retrieved_config.max_pool_count, 20);
    EXPECT_EQ(retrieved_config.max_cacheable_size, 1024);
}

// 清理测试
TEST_F(state_pool_test, pool_clear) {
    auto& pool = mc::futures::state_pool::instance();

    // 创建一些 future 来填充池
    {
        auto promise = mc::make_promise<int>(io_context_);
        auto future  = promise.get_future();
        promise.set_value(123);
        EXPECT_EQ(future.get(), 123);
    }

    // 清理池
    pool.clear_all_pools();

    // 检查池被清空
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_global_states, 0);
    EXPECT_EQ(stats.total_pools, 0);
}

// 大size状态不缓存测试
TEST_F(state_pool_test, large_state_not_cached) {
    auto& pool = mc::futures::state_pool::instance();

    // 设置较小的max_cacheable_size
    mc::futures::state_pool_config config;
    config.max_cacheable_size = 32;
    pool.set_config(config);

    // 创建一个大的结构体
    struct large_state {
        char data[64]; // 大于max_cacheable_size
    };

    // 创建并完成一个large_state future
    {
        auto        promise = mc::make_promise<large_state>(io_context_);
        auto        future  = promise.get_future();
        large_state ls{};
        promise.set_value(ls);
        future.get();
    }

    // 检查池统计，不应该有缓存的状态
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_global_states, 0) << "大size的状态不应该被缓存";
    EXPECT_EQ(stats.total_pools, 0) << "不应该为大size状态创建池";
}

// 池数量限制测试
TEST_F(state_pool_test, pool_count_limit) {
    auto& pool = mc::futures::state_pool::instance();

    // 设置较小的max_pool_count
    mc::futures::state_pool_config config;
    config.max_pool_count = 2;
    pool.set_config(config);

    // 创建不同大小的状态来触发新池创建
    struct state_size_1 {
        char data[16];
    };
    struct state_size_2 {
        char data[32];
    };
    struct state_size_3 {
        char data[48];
    };

    // 创建并完成future
    {
        auto promise1 = mc::make_promise<state_size_1>(io_context_);
        auto future1  = promise1.get_future();
        promise1.set_value(state_size_1{});
        future1.get();
    }

    {
        auto promise2 = mc::make_promise<state_size_2>(io_context_);
        auto future2  = promise2.get_future();
        promise2.set_value(state_size_2{});
        future2.get();
    }

    {
        auto promise3 = mc::make_promise<state_size_3>(io_context_);
        auto future3  = promise3.get_future();
        promise3.set_value(state_size_3{});
        future3.get();
    }

    // 检查池数量不超过限制
    auto stats = pool.get_stats();
    EXPECT_LE(stats.total_pools, 2) << "池数量不应超过限制";
}

// 多线程并发测试
TEST_F(state_pool_test, concurrent_access) {
    auto& pool = mc::futures::state_pool::instance();

    const int num_threads = 4;
    const int iterations  = 1000;

    mc::runtime::thread_list threads;
    std::atomic<int>         completed{0};

    // 多个线程并发访问池
    threads.start_threads(num_threads, [&completed]() {
        boost::asio::io_context local_io;

        for (int i = 0; i < iterations; ++i) {
            auto promise = mc::make_promise<int>(local_io);
            auto future  = promise.get_future();
            promise.set_value(i);
            future.get();
        }

        completed++;
    });

    threads.join_all();

    EXPECT_EQ(completed.load(), num_threads);

    // 验证池状态
    auto stats = pool.get_stats();
    EXPECT_GT(stats.total_global_states, 0) << "应该有状态被缓存";
    EXPECT_GT(stats.total_pools, 0) << "应该创建了缓存池";
}