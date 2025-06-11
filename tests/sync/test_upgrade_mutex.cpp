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
#include <future>
#include <gtest/gtest.h>
#include <mc/sync/shared_mutex.h>
#include <shared_mutex>
#include <thread>
#include <vector>

using namespace mc::sync;

template <typename Mutex>
class UpgradeMutexTest : public ::testing::Test {
public:
    using MutexType = Mutex;
};

using MutexTypes = ::testing::Types<shared_mutex, reader_priority_shared_mutex>;
TYPED_TEST_SUITE(UpgradeMutexTest, MutexTypes);

// 基本升级锁功能测试
TYPED_TEST(UpgradeMutexTest, BasicUpgradeLock) {
    TypeParam m;

    // 获取升级锁
    m.lock_upgrade();
    m.unlock_upgrade();
}

TYPED_TEST(UpgradeMutexTest, TryUpgradeLock) {
    TypeParam m;

    ASSERT_TRUE(m.try_lock_upgrade());
    m.unlock_upgrade();

    // 升级锁与升级锁互斥
    m.lock_upgrade();
    ASSERT_FALSE(m.try_lock_upgrade());
    m.unlock_upgrade();
}

TYPED_TEST(UpgradeMutexTest, UpgradeLockWithTimeout) {
    TypeParam m;

    ASSERT_TRUE(m.try_lock_upgrade_for(std::chrono::milliseconds(10)));
    m.unlock_upgrade();
}

// 升级锁与读锁兼容
TYPED_TEST(UpgradeMutexTest, UpgradeLockCompatibleWithSharedLock) {
    TypeParam          m;
    std::promise<void> upgrade_locked_promise;
    auto               upgrade_locked_future = upgrade_locked_promise.get_future();
    std::promise<void> shared_locked_promise;
    auto               shared_locked_future = shared_locked_promise.get_future();
    std::atomic<bool>  both_locked{false};

    std::thread upgrade_thread([&] {
        m.lock_upgrade();
        upgrade_locked_promise.set_value();

        // 等待共享锁也获取成功
        shared_locked_future.wait();
        both_locked = true;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        m.unlock_upgrade();
    });

    std::thread shared_thread([&] {
        upgrade_locked_future.wait(); // 等待升级锁先获取

        m.lock_shared();
        shared_locked_promise.set_value();

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        m.unlock_shared();
    });

    upgrade_thread.join();
    shared_thread.join();

    ASSERT_TRUE(both_locked.load());
}

// 升级锁与写锁互斥
TYPED_TEST(UpgradeMutexTest, UpgradeLockMutuallyExclusiveWithWriteLock) {
    TypeParam          m;
    std::promise<void> upgrade_locked_promise;
    auto               upgrade_locked_future = upgrade_locked_promise.get_future();
    std::atomic<bool>  write_attempted{false};
    std::atomic<bool>  write_succeeded{false};

    std::thread upgrade_thread([&] {
        m.lock_upgrade();
        upgrade_locked_promise.set_value();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m.unlock_upgrade();
    });

    std::thread write_thread([&] {
        upgrade_locked_future.wait(); // 等待升级锁先获取
        write_attempted = true;

        if (m.try_lock_for(std::chrono::milliseconds(10))) {
            write_succeeded = true;
            m.unlock();
        }
    });

    upgrade_thread.join();
    write_thread.join();

    ASSERT_TRUE(write_attempted.load());
    ASSERT_FALSE(write_succeeded.load()); // 写锁应该失败
}

// 测试从升级锁升级到写锁
TYPED_TEST(UpgradeMutexTest, UpgradeToWriteLock) {
    TypeParam m;

    m.lock_upgrade();
    m.unlock_upgrade_and_lock(); // 升级到写锁
    m.unlock();
}

// 测试从升级锁升级到写锁的超时功能
TYPED_TEST(UpgradeMutexTest, UpgradeToWriteLockWithTimeout) {
    TypeParam          m;
    std::promise<void> reader_ready_promise;
    auto               reader_ready_future = reader_ready_promise.get_future();
    std::atomic<bool>  upgrade_attempted{false};
    std::atomic<bool>  upgrade_succeeded{false};

    // 启动一个读线程持有读锁
    std::thread reader_thread([&] {
        m.lock_shared();
        reader_ready_promise.set_value();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m.unlock_shared();
    });

    // 在另一个线程中尝试从升级锁升级到写锁
    std::thread upgrade_thread([&] {
        reader_ready_future.wait();

        m.lock_upgrade(); // 这应该成功，因为升级锁与读锁兼容
        upgrade_attempted = true;

        // 尝试升级到写锁，但由于读锁存在应该超时
        if (m.try_unlock_upgrade_and_lock_for(std::chrono::milliseconds(20))) {
            upgrade_succeeded = true;
            m.unlock();
        } else {
            m.unlock_upgrade(); // 超时后释放升级锁
        }
    });

    reader_thread.join();
    upgrade_thread.join();

    ASSERT_TRUE(upgrade_attempted.load());
    ASSERT_FALSE(upgrade_succeeded.load()); // 升级应该超时失败
}

// 测试锁降级功能
TYPED_TEST(UpgradeMutexTest, DowngradeWriteToUpgrade) {
    TypeParam m;

    m.lock();
    m.unlock_and_lock_upgrade(); // 从写锁降级到升级锁
    m.unlock_upgrade();
}

TYPED_TEST(UpgradeMutexTest, DowngradeWriteToShared) {
    TypeParam m;

    m.lock();
    m.unlock_and_lock_shared(); // 从写锁降级到读锁
    m.unlock_shared();
}

TYPED_TEST(UpgradeMutexTest, DowngradeUpgradeToShared) {
    TypeParam m;

    m.lock_upgrade();
    m.unlock_upgrade_and_lock_shared(); // 从升级锁降级到读锁
    m.unlock_shared();
}

// 复杂的并发场景测试
TYPED_TEST(UpgradeMutexTest, ComplexConcurrentScenario) {
    TypeParam m;

    struct SharedData {
        long long value      = 0;
        long long validation = 0;
    };

    SharedData        data;
    std::atomic<bool> running{true};
    constexpr int     num_readers   = 3;
    constexpr int     num_writers   = 1;
    constexpr int     num_upgraders = 1;

    // 读线程
    auto reader_func = [&]() {
        while (running) {
            std::shared_lock<TypeParam> lock(m);
            ASSERT_EQ(data.value, data.validation);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    // 写线程
    auto writer_func = [&]() {
        for (int i = 0; i < 100; ++i) {
            std::lock_guard<TypeParam> lock(m);
            data.value++;
            data.validation = data.value;
        }
    };

    // 升级锁线程 - 修改逻辑确保会执行升级操作
    auto upgrader_func = [&]() {
        for (int i = 0; i < 50; ++i) {
            m.lock_upgrade();

            // 以升级锁模式读取
            long long current_value = data.value;

            // 每隔几次就执行一次升级，确保有升级操作发生
            if (i % 5 == 0 || current_value % 3 == 0) {
                // 升级到写锁进行修改
                m.unlock_upgrade_and_lock();
                data.value += 10; // 恢复原来的修改量
                data.validation = data.value;
                m.unlock();
            } else {
                // 不需要修改，直接释放升级锁
                m.unlock_upgrade();
            }
        }
    };

    std::vector<std::thread> threads;

    // 启动读线程
    for (int i = 0; i < num_readers; ++i) {
        threads.emplace_back(reader_func);
    }

    // 启动写线程
    for (int i = 0; i < num_writers; ++i) {
        threads.emplace_back(writer_func);
    }

    // 启动升级锁线程
    for (int i = 0; i < num_upgraders; ++i) {
        threads.emplace_back(upgrader_func);
    }

    // 等待写线程和升级锁线程完成
    for (int i = num_readers; i < threads.size(); ++i) {
        threads[i].join();
    }

    running = false;

    // 等待读线程完成
    for (int i = 0; i < num_readers; ++i) {
        threads[i].join();
    }

    // 验证最终状态一致性
    ASSERT_EQ(data.value, data.validation);
    // 在写优先策略下，升级锁可能因为写等待而无法获取，所以值可能只是100
    // 在读优先策略下，升级锁更容易获取，所以值应该大于100
    if constexpr (std::is_same_v<TypeParam, reader_priority_shared_mutex>) {
        ASSERT_GT(data.value, 100); // 读优先策略应该包含升级锁线程的修改
    } else {
        ASSERT_GE(data.value, 100); // 写优先策略至少包含写线程的修改
    }
}
