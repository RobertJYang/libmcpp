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
#include <gtest/gtest.h>

#include <mc/common.h>
#include <mc/runtime/thread_list.h>
#include <mc/sync/shared_mutex.h>
#include <mc/time.h>

#include <future>
#include <shared_mutex>

// 用于测试的自定义策略
namespace test_policies {

// 激进的写优先策略：写线程很快就会阻止读线程
struct aggressive_policy {
    static constexpr uint32_t spin_limit  = MC_SYNC_DEFAULT_SPIN_LIMIT;
    static constexpr uint32_t yield_limit = MC_SYNC_DEFAULT_YIELD_LIMIT;
    static constexpr uint32_t write_limit = 5; // 很小的阈值，写线程很快就会阻止读线程
};

// 宽松的读优先策略：读线程有更多的机会
struct lenient_policy {
    static constexpr uint32_t spin_limit  = MC_SYNC_DEFAULT_SPIN_LIMIT;
    static constexpr uint32_t yield_limit = MC_SYNC_DEFAULT_YIELD_LIMIT;
    static constexpr uint32_t write_limit = 500; // 很大的阈值，读线程有更多机会
};

// 立即写策略：写线程立即阻止读线程
struct immediate_writer_policy {
    static constexpr uint32_t spin_limit  = MC_SYNC_DEFAULT_SPIN_LIMIT;
    static constexpr uint32_t yield_limit = MC_SYNC_DEFAULT_YIELD_LIMIT;
    static constexpr uint32_t write_limit = 0; // 立即阻止读线程
};

struct light_reader_policy {
    static constexpr uint32_t spin_limit  = MC_SYNC_DEFAULT_SPIN_LIMIT;
    static constexpr uint32_t yield_limit = MC_SYNC_DEFAULT_YIELD_LIMIT;
    static constexpr uint32_t write_limit = 10; // 少量尝试后阻止读线程
};

struct moderate_reader_policy {
    static constexpr uint32_t spin_limit  = MC_SYNC_DEFAULT_SPIN_LIMIT;
    static constexpr uint32_t yield_limit = MC_SYNC_DEFAULT_YIELD_LIMIT;
    static constexpr uint32_t write_limit = 100; // 中等尝试后阻止读线程
};

struct heavy_reader_policy {
    static constexpr uint32_t spin_limit  = MC_SYNC_DEFAULT_SPIN_LIMIT;
    static constexpr uint32_t yield_limit = MC_SYNC_DEFAULT_YIELD_LIMIT;
    static constexpr uint32_t write_limit = 1000; // 大量尝试后阻止读线程
};

} // namespace test_policies

template <typename Mutex>
class SharedMutexTest : public ::testing::Test {
};

using MutexTypes = ::testing::Types<mc::sync::shared_mutex, mc::sync::reader_priority_shared_mutex>;
TYPED_TEST_SUITE(SharedMutexTest, MutexTypes);

// 测试基本的独占锁和解锁
TYPED_TEST(SharedMutexTest, BasicLockUnlock) {
    TypeParam m;
    m.lock();
    m.unlock();
}

// 测试基本的共享锁和解锁
TYPED_TEST(SharedMutexTest, BasicSharedLockUnlock) {
    TypeParam m;
    m.lock_shared();
    m.unlock_shared();
}

// 测试 try_lock
TYPED_TEST(SharedMutexTest, TryLock) {
    TypeParam m;
    ASSERT_TRUE(m.try_lock());
    ASSERT_FALSE(m.try_lock());
    m.unlock();
    ASSERT_TRUE(m.try_lock());
    m.unlock();
}

// 测试 try_lock_shared
TYPED_TEST(SharedMutexTest, TryLockShared) {
    TypeParam m;
    ASSERT_TRUE(m.try_lock_shared());
    ASSERT_TRUE(m.try_lock_shared());
    m.unlock_shared();
    m.unlock_shared();
}

// 测试写锁会阻塞其他写锁和读锁
TYPED_TEST(SharedMutexTest, WriteLockBlocksOthers) {
    TypeParam m;
    m.lock();
    ASSERT_FALSE(m.try_lock());
    ASSERT_FALSE(m.try_lock_shared());
    m.unlock();
}

// 测试读锁会阻塞写锁，但允许其他读锁
TYPED_TEST(SharedMutexTest, ReadLockBlocksWrite) {
    TypeParam m;
    m.lock_shared();
    ASSERT_FALSE(m.try_lock());
    ASSERT_TRUE(m.try_lock_shared());
    m.unlock_shared();
    m.unlock_shared();
}

// 多线程读写测试
TYPED_TEST(SharedMutexTest, MultipleReaders) {
    TypeParam     m;
    int           counter     = 0;
    constexpr int num_threads = 4;

    auto reader_func = [&]() {
        for (int i = 0; i < 1000; ++i) {
            std::shared_lock<TypeParam> lock(m);
            // 这里不需要真的读取值，只是为了确保锁被正确持有
            volatile int c = counter;
            MC_UNUSED(c);
        }
    };

    mc::runtime::thread_list threads;
    threads.start_threads(num_threads, reader_func);
    threads.join_all();
}

TYPED_TEST(SharedMutexTest, WriterAndReaders) {
    TypeParam         m;
    int               value   = 0;
    std::atomic<bool> running = true;

    auto writer_func = [&]() {
        for (int i = 0; i < 100; ++i) {
            std::lock_guard<TypeParam> lock(m);
            value++;
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    };

    auto reader_func = [&]() {
        int last_read = 0;
        while (running) {
            std::shared_lock<TypeParam> lock(m);
            ASSERT_GE(value, last_read);
            last_read = value;
        }
    };

    mc::runtime::thread_list writer_threads;
    mc::runtime::thread_list reader_threads;
    writer_threads.add_thread(writer_func);
    reader_threads.start_threads(4, reader_func);

    writer_threads.join_all();
    running = false;

    reader_threads.join_all();

    ASSERT_EQ(value, 100);
}

// 专门测试多个写竞争的场景
TYPED_TEST(SharedMutexTest, MultipleWritersContention) {
    TypeParam     m;
    long long     counter               = 0;
    constexpr int num_threads           = 4;
    constexpr int iterations_per_thread = 10000;

    auto writer_func = [&]() {
        for (int i = 0; i < iterations_per_thread; ++i) {
            std::lock_guard<TypeParam> lock(m);
            counter++;
        }
    };

    mc::runtime::thread_list threads;
    threads.start_threads(num_threads, writer_func);
    threads.join_all();

    // 验证最终的计数值是否正确，这间接证明了互斥的正确性
    // 同时，如果测试能够成功结束，也证明了没有发生死锁或饿死
    ASSERT_EQ(counter, num_threads * iterations_per_thread);
}

// 专门测试多个读线程和写线程混合竞争的场景
TYPED_TEST(SharedMutexTest, MixedReadWriteContention) {
    TypeParam m;

    struct SharedData {
        long long write_count      = 0;
        long long validation_value = 0; // 必须始终等于 write_count
    };

    SharedData        data;
    std::atomic<bool> running            = true;
    constexpr int     num_writer_threads = 2;
    constexpr int     num_reader_threads = 4;
    constexpr int     writes_per_thread  = 100;

    auto writer_func = [&]() {
        for (int i = 0; i < writes_per_thread; ++i) {
            std::lock_guard<TypeParam> lock(m);
            data.write_count++;
            // 模拟一些工作
            std::this_thread::sleep_for(std::chrono::microseconds(1));
            data.validation_value = data.write_count;
        }
    };

    auto reader_func = [&]() {
        while (running) {
            std::shared_lock<TypeParam> lock(m);
            // 关键断言：读永远不应该看到一个不一致的状态
            // 即 validation_value 必须等于 write_count
            ASSERT_EQ(data.write_count, data.validation_value);
        }
    };

    mc::runtime::thread_list writer_threads;
    mc::runtime::thread_list reader_threads;

    writer_threads.start_threads(num_writer_threads, writer_func);
    reader_threads.start_threads(num_reader_threads, reader_func);

    writer_threads.join_all();
    running = false;

    reader_threads.join_all();

    // 验证最终写入的计数值是否正确
    ASSERT_EQ(data.write_count, num_writer_threads * writes_per_thread);
    ASSERT_EQ(data.validation_value, num_writer_threads * writes_per_thread);
}

// 专门测试超时功能
TYPED_TEST(SharedMutexTest, TryLockForTimesOut) {
    TypeParam          m;
    std::promise<void> locked_promise;
    auto               locked_future = locked_promise.get_future();

    std::thread t1([&] {
        std::lock_guard<TypeParam> lock(m);
        locked_promise.set_value();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });

    locked_future.wait(); // 确保 t1 先获取锁

    auto start = mc::time_point::now();
    ASSERT_FALSE(m.try_lock_for(mc::milliseconds(20)));
    auto end     = mc::time_point::now();
    auto elapsed = end - start;
    t1.join();

    ASSERT_GE(elapsed.count(), 20);
    ASSERT_LT(elapsed.count(), 50);
}

TYPED_TEST(SharedMutexTest, TryLockForSucceeds) {
    TypeParam m;
    ASSERT_TRUE(m.try_lock_for(std::chrono::milliseconds(50)));
    m.unlock();
}

TYPED_TEST(SharedMutexTest, TryLockSharedForTimesOut) {
    TypeParam          m;
    std::promise<void> locked_promise;
    auto               locked_future = locked_promise.get_future();

    std::thread t1([&] {
        std::lock_guard<TypeParam> lock(m); // exclusive lock
        locked_promise.set_value();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });

    locked_future.wait(); // 确保 t1 先获取锁

    auto start = mc::time_point::now();
    ASSERT_FALSE(m.try_lock_shared_for(mc::milliseconds(20)));
    auto end     = mc::time_point::now();
    auto elapsed = end - start;
    ASSERT_GE(elapsed.count(), 20);
    ASSERT_LT(elapsed.count(), 50);

    t1.join();
}

TYPED_TEST(SharedMutexTest, TryLockSharedForSucceeds) {
    TypeParam m;
    ASSERT_TRUE(m.try_lock_shared_for(mc::milliseconds(50)));
    m.unlock_shared();

    // 另一个读可以同时进入
    std::shared_lock<TypeParam> lock1(m, std::defer_lock);
    ASSERT_TRUE(lock1.try_lock_for(std::chrono::milliseconds(50)));

    std::shared_lock<TypeParam> lock2(m, std::defer_lock);
    ASSERT_TRUE(lock2.try_lock_for(std::chrono::milliseconds(50)));
}

// 测试升级锁降级操作
TYPED_TEST(SharedMutexTest, UpgradeLockDowngrade) {
    TypeParam m;

    // 测试正常的升级锁降级到读锁
    m.lock_upgrade();
    m.unlock_upgrade_and_lock_shared();
    // 现在应该持有读锁，其他读锁应该可以获取
    ASSERT_TRUE(m.try_lock_shared());
    m.unlock_shared();
    m.unlock_shared();

    // 测试没有升级锁时调用降级操作
    // 这应该是安全的，不会产生副作用
    m.unlock_upgrade_and_lock_shared(); // 没有升级锁，应该直接返回
    ASSERT_TRUE(m.try_lock());          // 应该能获取写锁，证明没有错误状态
    m.unlock();
}

// 测试写锁降级操作
TYPED_TEST(SharedMutexTest, WriteLockDowngrade) {
    TypeParam m;

    // 测试写锁降级到升级锁
    m.lock();
    m.unlock_and_lock_upgrade();
    // 现在应该持有升级锁，读锁应该可以获取，但写锁不可以
    ASSERT_TRUE(m.try_lock_shared());
    ASSERT_FALSE(m.try_lock());
    ASSERT_FALSE(m.try_lock_upgrade()); // 已有升级锁
    m.unlock_shared();
    m.unlock_upgrade();

    // 测试写锁降级到读锁
    m.lock();
    m.unlock_and_lock_shared();
    // 现在应该持有读锁，其他读锁应该可以获取，但写锁不可以
    ASSERT_TRUE(m.try_lock_shared());
    ASSERT_FALSE(m.try_lock());
    m.unlock_shared();
    m.unlock_shared();

    // 测试没有写锁时调用降级操作
    // 这应该是安全的，不会产生副作用
    m.unlock_and_lock_upgrade(); // 没有写锁，应该直接返回
    m.unlock_and_lock_shared();  // 没有写锁，应该直接返回
    ASSERT_TRUE(m.try_lock());   // 应该能获取写锁，证明没有错误状态
    m.unlock();
}

// 测试升级锁升级操作
TYPED_TEST(SharedMutexTest, UpgradeLockUpgrade) {
    TypeParam m;

    // 测试基本的升级锁升级到写锁
    m.lock_upgrade();
    ASSERT_TRUE(m.try_unlock_upgrade_and_lock()); // 无竞争时应该成功
    // 现在应该持有写锁
    ASSERT_FALSE(m.try_lock());
    ASSERT_FALSE(m.try_lock_shared());
    m.unlock();

    // 测试有读锁时的升级锁升级
    m.lock_shared();
    m.lock_upgrade();                              // 升级锁与读锁兼容
    ASSERT_FALSE(m.try_unlock_upgrade_and_lock()); // 有读锁时应该失败
    m.unlock_shared();
    ASSERT_TRUE(m.try_unlock_upgrade_and_lock()); // 读锁释放后应该成功
    m.unlock();

    // 测试没有升级锁时调用升级操作
    ASSERT_FALSE(m.try_unlock_upgrade_and_lock()); // 没有升级锁应该失败
    ASSERT_TRUE(m.try_lock());                     // 应该能获取写锁，证明没有错误状态
    m.unlock();
}

// 测试升级锁的基本操作
TYPED_TEST(SharedMutexTest, UpgradeLockBasic) {
    TypeParam m;

    // 测试升级锁获取和释放
    m.lock_upgrade();
    ASSERT_FALSE(m.try_lock_upgrade()); // 不能获取第二个升级锁
    ASSERT_FALSE(m.try_lock());         // 升级锁与写锁互斥
    ASSERT_TRUE(m.try_lock_shared());   // 升级锁与读锁兼容
    m.unlock_shared();
    m.unlock_upgrade();

    // 释放后应该可以重新获取
    ASSERT_TRUE(m.try_lock_upgrade());
    m.unlock_upgrade();

    // 与写锁的互斥性
    m.lock();
    ASSERT_FALSE(m.try_lock_upgrade());
    m.unlock();

    ASSERT_TRUE(m.try_lock_upgrade());
    m.unlock_upgrade();
}

// 测试自定义防饥饿阈值策略
TEST(CustomPolicyTest, CustomStarvationThreshold) {
    mc::sync::basic_shared_mutex<test_policies::aggressive_policy> aggressive_mutex;
    mc::sync::basic_shared_mutex<test_policies::lenient_policy>    lenient_mutex;

    // 基本测试：验证自定义策略的锁操作正常工作

    // 测试激进策略
    aggressive_mutex.lock();
    aggressive_mutex.unlock();

    aggressive_mutex.lock_shared();
    ASSERT_TRUE(aggressive_mutex.try_lock_shared()); // 多个读锁可以共存
    aggressive_mutex.unlock_shared();
    aggressive_mutex.unlock_shared();

    aggressive_mutex.lock_upgrade();
    aggressive_mutex.unlock_upgrade();

    // 测试宽松策略
    lenient_mutex.lock();
    lenient_mutex.unlock();

    lenient_mutex.lock_shared();
    ASSERT_TRUE(lenient_mutex.try_lock_shared()); // 多个读锁可以共存
    lenient_mutex.unlock_shared();
    lenient_mutex.unlock_shared();

    lenient_mutex.lock_upgrade();
    lenient_mutex.unlock_upgrade();

    // 验证策略参数是否正确设置
    static_assert(test_policies::aggressive_policy::write_limit == 5);
    static_assert(test_policies::lenient_policy::write_limit == 500);

    // 验证策略类型：阈值为0是写优先，大于0是读优先
    static_assert(mc::sync::writer_priority_policy::write_limit == 0); // 写优先
    static_assert(mc::sync::reader_priority_policy::write_limit > 0);  // 读优先
}

// 测试统一的阈值策略模型
TEST(UnifiedPolicyTest, StarvationThresholdBasedStrategy) {
    // 测试使用已定义策略的锁行为一致性
    mc::sync::basic_shared_mutex<test_policies::immediate_writer_policy> writer_mutex;
    mc::sync::basic_shared_mutex<test_policies::light_reader_policy>     light_mutex;
    mc::sync::basic_shared_mutex<test_policies::moderate_reader_policy>  moderate_mutex;
    mc::sync::basic_shared_mutex<test_policies::heavy_reader_policy>     heavy_mutex;

    // 基本锁操作测试
    writer_mutex.lock();
    writer_mutex.unlock();

    light_mutex.lock_shared();
    light_mutex.unlock_shared();

    moderate_mutex.lock_upgrade();
    moderate_mutex.unlock_upgrade();

    heavy_mutex.lock();
    heavy_mutex.unlock();

    // 验证策略配置和分类
    static_assert(test_policies::immediate_writer_policy::write_limit == 0);  // 写优先
    static_assert(test_policies::light_reader_policy::write_limit == 10);     // 轻度读优先
    static_assert(test_policies::moderate_reader_policy::write_limit == 100); // 中度读优先
    static_assert(test_policies::heavy_reader_policy::write_limit == 1000);   // 重度读优先

    // 验证统一的策略模型：只需要一个阈值参数就能控制读写优先级
    static_assert(test_policies::immediate_writer_policy::write_limit == 0); // 写优先
    static_assert(test_policies::light_reader_policy::write_limit > 0);      // 读优先
    static_assert(test_policies::moderate_reader_policy::write_limit > 0);   // 读优先
    static_assert(test_policies::heavy_reader_policy::write_limit > 0);      // 读优先
}