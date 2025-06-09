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

#include <chrono>
#include <gtest/gtest.h>

#include <mc/sync/mutex_box.h>
#include <mc/sync/shared_mutex.h>

#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace mc::sync;

class mutex_box_test : public ::testing::Test {
protected:
    void SetUp() override {
    }
    void TearDown() override {
    }
};

// 测试基本的构造和析构
TEST_F(mutex_box_test, basic_construction) {
    // 默认构造
    mutex_box<int> sync_int;
    EXPECT_EQ(*sync_int.lock(), 0);

    // 拷贝构造
    mutex_box<int> sync_int2(42);
    EXPECT_EQ(*sync_int2.lock(), 42);

    // 移动构造
    mutex_box<std::string> sync_str(std::string("hello"));
    EXPECT_EQ(*sync_str.lock(), "hello");

    // 原地构造
    mutex_box<std::vector<int>> sync_vec(std::in_place, 3, 100);
    EXPECT_EQ(sync_vec.lock()->size(), 3);
    EXPECT_EQ((*sync_vec.lock())[0], 100);
}

// 测试独占锁类型的互斥锁
TEST_F(mutex_box_test, exclusive_mutex) {
    mutex_box<int, std::mutex> sync_data(0);

    // 测试基本锁定
    {
        auto locked = sync_data.lock();
        EXPECT_TRUE(locked);
        *locked = 42;
    }

    EXPECT_EQ(*sync_data.lock(), 42);

    // 测试尝试锁定
    auto try_locked = sync_data.try_lock();
    EXPECT_TRUE(try_locked);
    *try_locked = 100;
    EXPECT_EQ(*try_locked, 100);
}

// 测试共享锁类型的互斥锁
TEST_F(mutex_box_test, shared_mutex) {
    mutex_box<std::string, std::shared_mutex> sync_data("initial");

    // 测试写锁
    {
        auto write_locked = sync_data.wlock();
        EXPECT_TRUE(write_locked);
        *write_locked = "modified";
    }

    // 测试读锁
    {
        auto read_locked = sync_data.rlock();
        EXPECT_TRUE(read_locked);
        EXPECT_EQ(*read_locked, "modified");
    }

    // 测试尝试写锁
    {
        auto try_write_locked = sync_data.try_wlock();
        EXPECT_TRUE(try_write_locked);
        *try_write_locked = "try_modified";
    }

    // 测试尝试读锁
    {
        auto try_read_locked = sync_data.try_rlock();
        EXPECT_TRUE(try_read_locked);
        EXPECT_EQ(*try_read_locked, "try_modified");
    }
}

// 测试with_*lock函数
TEST_F(mutex_box_test, with_lock_functions) {
    mutex_box<int, std::shared_mutex> sync_data(10);

    // 测试with_wlock
    auto result = sync_data.with_wlock([](int& data) {
        data += 5;
        return data * 2;
    });
    EXPECT_EQ(result, 30);
    EXPECT_EQ(*sync_data.rlock(), 15);

    // 测试with_rlock
    auto read_result = sync_data.with_rlock([](const int& data) {
        return data + 10;
    });
    EXPECT_EQ(read_result, 25);

    // 测试with_wlock_ptr
    sync_data.with_wlock_ptr([](auto locked_ptr) {
        *locked_ptr = 100;
        // 可以在这里调用scoped_unlock等
    });
    EXPECT_EQ(*sync_data.rlock(), 100);

    // 测试with_rlock_ptr
    auto ptr_result = sync_data.with_rlock_ptr([](auto locked_ptr) {
        return *locked_ptr * 3;
    });
    EXPECT_EQ(ptr_result, 300);
}

// 测试作用域解锁
TEST_F(mutex_box_test, scoped_unlock) {
    mutex_box<int> sync_data(42);

    auto locked = sync_data.lock();
    EXPECT_EQ(*locked, 42);

    {
        auto unlocker = locked.scoped_unlock();
        // 在这个作用域内，锁被临时释放
        // 可以在另一个线程中获取锁
    }
    // 锁重新获取
    EXPECT_EQ(*locked, 42);
}

// 测试基本的超时互斥锁
TEST_F(mutex_box_test, timeout_mutex_basic) {
    using timeout_mutex = mc::sync::shared_mutex;
    mutex_box<int, timeout_mutex> sync_data(0);

    // 测试写锁
    auto wlock = sync_data.try_wlock_for(mc::milliseconds(10));
    EXPECT_TRUE(wlock);
    *wlock = 123;

    // 测试读锁
    wlock.unlock();
    auto rlock = sync_data.rlock();
    EXPECT_TRUE(rlock);
    EXPECT_EQ(*rlock, 123);

    // 测试获取写锁超时，因为前面的写锁还未释放
    auto wlock2 = sync_data.try_wlock_for(mc::milliseconds(10));
    EXPECT_FALSE(wlock2);
}

// 测试数据操作
TEST_F(mutex_box_test, data_operations) {
    mutex_box<std::vector<int>> sync_vec;

    // 测试拷贝
    std::vector<int> copy_vec;
    sync_vec.copy(copy_vec);
    EXPECT_TRUE(copy_vec.empty());

    auto copied_vec = sync_vec.copy();
    EXPECT_TRUE(copied_vec.empty());

    // 修改数据
    {
        auto locked = sync_vec.lock();
        locked->push_back(1);
        locked->push_back(2);
        locked->push_back(3);
    }

    // 再次测试拷贝
    auto new_copied_vec = sync_vec.copy();
    EXPECT_EQ(new_copied_vec.size(), 3);
    EXPECT_EQ(new_copied_vec[0], 1);
    EXPECT_EQ(new_copied_vec[2], 3);

    // 测试交换
    std::vector<int> external_vec{10, 20, 30};
    sync_vec.swap(external_vec);

    EXPECT_EQ(external_vec.size(), 3);
    EXPECT_EQ(external_vec[0], 1);

    EXPECT_EQ(sync_vec.copy().size(), 3);
    EXPECT_EQ(sync_vec.copy()[0], 10);
}

// 测试赋值操作
TEST_F(mutex_box_test, assignment_operations) {
    mutex_box<int> sync_int(42);

    // 测试从值赋值
    sync_int = 100;
    EXPECT_EQ(*sync_int.lock(), 100);

    // 测试移动赋值
    sync_int = std::move(200);
    EXPECT_EQ(*sync_int.lock(), 200);

    // 测试从另一个 mutex_box 对象拷贝赋值
    mutex_box<int> sync_int2(300);
    sync_int = sync_int2;
    EXPECT_EQ(*sync_int.lock(), 300);

    // 测试exchange
    auto old_value = sync_int.exchange(400);
    EXPECT_EQ(old_value, 300);
    EXPECT_EQ(*sync_int.lock(), 400);
}

// 测试多线程访问
TEST_F(mutex_box_test, multithreaded_access) {
    mutex_box<int, std::shared_mutex> sync_counter(0);
    constexpr int                     num_threads           = 10;
    constexpr int                     increments_per_thread = 100;

    std::vector<std::thread> threads;

    // 创建多个写线程
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&sync_counter]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                auto locked = sync_counter.wlock();
                (*locked)++;
            }
        });
    }

    // 创建多个读线程
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&sync_counter]() {
            for (int j = 0; j < 50; ++j) {
                auto         locked = sync_counter.rlock();
                volatile int value  = *locked; // 读取值
                (void)value;                   // 避免未使用变量警告
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    // 验证最终结果
    EXPECT_EQ(*sync_counter.rlock(), num_threads * increments_per_thread);
}

// 测试不安全访问方法
TEST_F(mutex_box_test, unsafe_access) {
    mutex_box<std::string> sync_str("test");

    // 测试不安全获取数据
    auto& unsafe_ref = sync_str.unsafe_get_unlocked();
    EXPECT_EQ(unsafe_ref, "test");

    unsafe_ref = "modified_unsafe";
    EXPECT_EQ(*sync_str.lock(), "modified_unsafe");

    // 测试不安全获取互斥锁
    auto& mutex = sync_str.unsafe_get_mutex();
    mutex.lock();
    sync_str.unsafe_get_unlocked() = "mutex_locked";
    mutex.unlock();

    EXPECT_EQ(*sync_str.lock(), "mutex_locked");
}

// 测试自定义互斥锁
TEST_F(mutex_box_test, custom_mutex) {
    using custom_mutex = mc::shared_mutex;
    mutex_box<int, custom_mutex> sync_data(42);

    // 测试写锁
    {
        auto write_lock = sync_data.wlock();
        EXPECT_TRUE(write_lock);
        *write_lock = 100;
    }

    // 测试读锁
    {
        auto read_lock = sync_data.rlock();
        EXPECT_TRUE(read_lock);
        EXPECT_EQ(*read_lock, 100);
    }
}

// 性能测试（轻量级）
TEST_F(mutex_box_test, performance_light) {
    mutex_box<int, std::shared_mutex> sync_counter(0);
    constexpr int                     iterations = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    // 测试读写混合性能
    std::vector<std::thread> threads;

    // 一个写线程
    threads.emplace_back([&sync_counter]() {
        for (int i = 0; i < iterations; ++i) {
            auto locked = sync_counter.wlock();
            (*locked)++;
        }
    });

    // 两个读线程
    for (int i = 0; i < 2; ++i) {
        threads.emplace_back([&sync_counter]() {
            for (int j = 0; j < iterations / 2; ++j) {
                auto         locked = sync_counter.rlock();
                volatile int value  = *locked;
                (void)value;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(*sync_counter.rlock(), iterations);
    EXPECT_LT(duration.count(), 1000); // 应该在1秒内完成
}

// 测试locked_ptr的各种操作
TEST_F(mutex_box_test, locked_ptr_operations) {
    mutex_box<std::vector<int>, std::shared_mutex> sync_vec;

    // 测试locked_ptr的基本操作
    {
        auto locked = sync_vec.wlock();
        EXPECT_TRUE(locked);
        EXPECT_FALSE(locked.is_null());

        // 测试指针访问
        locked->push_back(1);
        locked->push_back(2);

        // 测试解引用访问
        EXPECT_EQ((*locked).size(), 2);

        // 测试手动解锁
        locked.unlock();
        EXPECT_TRUE(locked.is_null());
    }

    // 验证数据已写入
    {
        auto read_locked = sync_vec.rlock();
        EXPECT_EQ(read_locked->size(), 2);
        EXPECT_EQ((*read_locked)[0], 1);
        EXPECT_EQ((*read_locked)[1], 2);
    }
}

// 测试类型特性
TEST_F(mutex_box_test, type_traits) {
    // 测试不同类型的 mutex_box 对象
    mutex_box<int, std::mutex>        unique_sync;
    mutex_box<int, std::shared_mutex> shared_sync;
    mutex_box<int, mc::shared_mutex>  custom_shared_sync;

    // 确保可以正确获取锁（分别测试，避免死锁）
    {
        auto unique_lock = unique_sync.lock();
        EXPECT_TRUE(unique_lock);
    }

    {
        auto shared_write_lock = unique_sync.wlock();
        EXPECT_TRUE(shared_write_lock);
    }

    {
        auto shared_read_lock = shared_sync.rlock();
        EXPECT_TRUE(shared_read_lock);
    }

    {
        auto custom_write_lock = custom_shared_sync.wlock();
        EXPECT_TRUE(custom_write_lock);
    }

    {
        auto custom_read_lock = custom_shared_sync.rlock();
        EXPECT_TRUE(custom_read_lock);
    }
}

/**
 * @brief 测试双对象锁定
 */
TEST_F(mutex_box_test, lock_pair) {
    mutex_box<int> box1(10);
    mutex_box<int> box2(20);

    // 验证双对象死锁的场景，主线程锁定顺序：box2 -> box1，t1 线程锁定顺序：box1 -> box2
    {
        std::promise<void> wait_promise1;
        std::promise<void> wait_promise2;
        std::thread        t1([&]() {
            // 先锁定 box1，再锁定 box2
            auto locked1 = box1.lock();
            wait_promise1.set_value(); // 通知主线程，t1 线程已经锁定 box1
            auto locked2 = box2.try_lock();

            // 锁定 box2 应该失败，因为 box2 已被主线程锁定
            EXPECT_FALSE(locked2);
            wait_promise2.get_future().wait(); // 通知主线程，t1 线程已经锁定 box2
        });

        // 主线程锁定 box2
        auto locked2 = box2.lock();
        // 等待 t1 线程锁定 box1
        wait_promise1.get_future().wait();
        // 主线程尝试锁定 box1，应该失败
        auto locked1 = box1.try_lock();
        wait_promise2.set_value();
        EXPECT_FALSE(locked1);

        t1.join();
    }

    // 验证使用 mc::lock_pair 函数避免死锁，仍然按上面引发死锁的顺序锁定
    {
        std::promise<void> wait_promise;
        std::thread        t1([&]() {
            auto [locked1, locked2] = mc::lock_pair(box1, box2);
            wait_promise.set_value();
            EXPECT_EQ(*locked1, 10);
            EXPECT_EQ(*locked2, 20);

            *locked1 = 30;
            *locked2 = 40;
        });

        wait_promise.get_future().wait();
        auto [locked2, locked1] = mc::lock_pair(box2, box1);
        EXPECT_EQ(*locked1, 30);
        EXPECT_EQ(*locked2, 40);

        *locked1 = 100;
        *locked2 = 200;
        t1.join();
    }

    // 验证最终值
    {
        auto locked1 = box1.lock();
        auto locked2 = box2.lock();

        EXPECT_EQ(*locked1, 100);
        EXPECT_EQ(*locked2, 200);
    }
}

/**
 * @brief 测试多对象锁定
 */
TEST_F(mutex_box_test, lock_multiple) {
    mutex_box<int> box1(10);
    mutex_box<int> box2(20);
    mutex_box<int> box3(30);
    mutex_box<int> box4(40);

    // 测试三对象锁定
    {
        auto [locked1, locked2, locked3] = mc::lock(box1, box2, box3);
        EXPECT_EQ(*locked1, 10);
        EXPECT_EQ(*locked2, 20);
        EXPECT_EQ(*locked3, 30);

        *locked1 = 100;
        *locked2 = 200;
        *locked3 = 300;
    }

    // 测试四对象锁定
    {
        auto [locked1, locked2, locked3, locked4] = mc::lock(box1, box2, box3, box4);
        EXPECT_EQ(*locked1, 100);
        EXPECT_EQ(*locked2, 200);
        EXPECT_EQ(*locked3, 300);
        EXPECT_EQ(*locked4, 40);

        *locked1 = 1000;
        *locked2 = 2000;
        *locked3 = 3000;
        *locked4 = 4000;
    }

    // 验证最终值
    {
        auto [locked1, locked2, locked3, locked4] = mc::lock(box1, box2, box3, box4);
        EXPECT_EQ(*locked1, 1000);
        EXPECT_EQ(*locked2, 2000);
        EXPECT_EQ(*locked3, 3000);
        EXPECT_EQ(*locked4, 4000);
    }
}

/**
 * @brief 测试多对象锁定的复杂场景
 */
TEST_F(mutex_box_test, lock_complex_scenarios) {
    mutex_box<std::string>      name_box("Alice");
    mutex_box<int>              age_box(25);
    mutex_box<double>           salary_box(50000.0);
    mutex_box<bool>             active_box(true);
    mutex_box<std::vector<int>> scores_box({90, 85, 88});

    // 测试两个对象锁定 - 模拟银行转账场景
    {
        auto [name_locked, age_locked] = mc::lock(name_box, age_box);

        // 模拟同时更新用户信息
        *name_locked = "Bob";
        *age_locked  = 30;

        EXPECT_EQ(*name_locked, "Bob");
        EXPECT_EQ(*age_locked, 30);
    }

    // 测试三个对象锁定 - 模拟工资调整场景
    {
        auto [age_locked, salary_locked, active_locked] = mc::lock(age_box, salary_box, active_box);

        // 模拟年龄增长和工资调整
        *age_locked += 1;
        *salary_locked *= 1.1; // 10% 加薪
        *active_locked = true;

        EXPECT_EQ(*age_locked, 31);
        EXPECT_DOUBLE_EQ(*salary_locked, 55000.0);
        EXPECT_TRUE(*active_locked);
    }

    // 测试五个对象锁定 - 模拟完整记录更新
    {
        auto [name_locked, age_locked, salary_locked, active_locked, scores_locked] =
            mc::lock(name_box, age_box, salary_box, active_box, scores_box);

        // 模拟完整的员工记录更新
        *name_locked   = "Charlie";
        *age_locked    = 35;
        *salary_locked = 60000.0;
        *active_locked = false;
        scores_locked->push_back(92);

        EXPECT_EQ(*name_locked, "Charlie");
        EXPECT_EQ(*age_locked, 35);
        EXPECT_DOUBLE_EQ(*salary_locked, 60000.0);
        EXPECT_FALSE(*active_locked);
        EXPECT_EQ(scores_locked->size(), 4);
        EXPECT_EQ(scores_locked->back(), 92);
    }

    // 验证最终状态
    {
        auto name_locked   = name_box.rlock();
        auto age_locked    = age_box.rlock();
        auto salary_locked = salary_box.rlock();
        auto active_locked = active_box.rlock();
        auto scores_locked = scores_box.rlock();

        EXPECT_EQ(*name_locked, "Charlie");
        EXPECT_EQ(*age_locked, 35);
        EXPECT_DOUBLE_EQ(*salary_locked, 60000.0);
        EXPECT_FALSE(*active_locked);
        EXPECT_EQ(scores_locked->size(), 4);
    }
}

/**
 * @brief 测试超时锁功能
 */
TEST_F(mutex_box_test, timeout_lock_functions) {
    using timeout_mutex = mc::sync::shared_mutex;
    mutex_box<int, timeout_mutex> sync_data(42);

    // 测试成功获取超时读锁
    {
        auto locked = sync_data.try_rlock_for(mc::milliseconds(100));
        EXPECT_TRUE(locked);
        EXPECT_EQ(*locked, 42);
    }

    // 测试成功获取超时写锁
    {
        auto locked = sync_data.try_wlock_for(mc::milliseconds(100));
        EXPECT_TRUE(locked);
        *locked = 100;
        EXPECT_EQ(*locked, 100);
    }

    // 测试超时失败场景 - 多线程环境
    {
        std::atomic<bool> reader_acquired{false};
        std::atomic<bool> writer_timeout{false};

        std::thread reader_thread([&]() {
            auto locked     = sync_data.rlock();
            reader_acquired = true;
            std::this_thread::sleep_for(mc::milliseconds(200));
            // 读锁持有200ms
        });

        std::thread writer_thread([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 确保读锁先获取
            auto locked    = sync_data.try_wlock_for(mc::milliseconds(50));
            writer_timeout = !static_cast<bool>(locked);
        });

        reader_thread.join();
        writer_thread.join();

        EXPECT_TRUE(reader_acquired);
        EXPECT_TRUE(writer_timeout); // 写锁应该超时
    }

    // 测试通用超时锁
    {
        // 使用标准库的 std::chrono::milliseconds 作为超时时间
        auto locked = sync_data.try_lock_for(
            std::chrono::milliseconds(100));
        EXPECT_TRUE(locked);

        *locked = 200;
        EXPECT_EQ(*locked, 200);
    }
}

/**
 * @brief 测试升级锁功能
 */
TEST_F(mutex_box_test, upgrade_lock_basic) {
    using upgrade_mutex = mc::sync::shared_mutex;
    mutex_box<int, upgrade_mutex> sync_data(42);

    // 测试基本的升级锁获取
    {
        auto locked = sync_data.ulock();
        EXPECT_TRUE(locked);
        EXPECT_EQ(*locked, 42);

        // 修改数据
        *locked = 100;
        EXPECT_EQ(*locked, 100);
    }

    // 验证数据确实被修改
    EXPECT_EQ(*sync_data.rlock(), 100);

    // 测试尝试获取升级锁
    {
        // 先获取一个升级锁
        auto locked1 = sync_data.try_ulock();
        EXPECT_TRUE(locked1);

        // 尝试获取第二个升级锁应该失败
        auto locked2 = sync_data.try_ulock();
        EXPECT_FALSE(locked2);
    }
}

/**
 * @brief 测试升级锁升级为写锁
 */
TEST_F(mutex_box_test, upgrade_lock_to_write) {
    using upgrade_mutex = mc::shared_mutex;
    mutex_box<int, upgrade_mutex> sync_data(42);

    // 测试升级锁升级为写锁
    {
        auto upgrade_locked = sync_data.ulock();
        EXPECT_TRUE(upgrade_locked);
        EXPECT_EQ(*upgrade_locked, 42);

        // 升级为写锁
        auto write_locked = std::move(upgrade_locked).upgrade_to_wlock();
        EXPECT_TRUE(write_locked);
        EXPECT_EQ(*write_locked, 42);

        // 修改数据
        *write_locked = 200;
        EXPECT_EQ(*write_locked, 200);
    }

    // 验证数据确实被修改
    EXPECT_EQ(*sync_data.rlock(), 200);

    // 测试尝试升级为写锁
    {
        auto upgrade_locked = sync_data.ulock();
        EXPECT_TRUE(upgrade_locked);

        // 尝试升级为写锁
        auto write_locked_opt = std::move(upgrade_locked).try_upgrade_to_wlock();
        EXPECT_TRUE(write_locked_opt.has_value());

        auto write_locked = std::move(*write_locked_opt);
        EXPECT_TRUE(write_locked);
        EXPECT_EQ(*write_locked, 200);
    }
}

/**
 * @brief 测试锁降级功能
 */
TEST_F(mutex_box_test, lock_downgrade) {
    using upgrade_mutex = mc::sync::shared_mutex;
    mutex_box<int, upgrade_mutex> sync_data(42);

    // 测试升级锁降级为读锁
    {
        auto upgrade_locked = sync_data.ulock();
        EXPECT_TRUE(upgrade_locked);
        EXPECT_EQ(*upgrade_locked, 42);

        // 降级为读锁
        auto read_locked = std::move(upgrade_locked).downgrade_to_rlock();
        EXPECT_TRUE(read_locked);
        EXPECT_EQ(*read_locked, 42);
    }

    // 测试写锁降级为读锁
    {
        auto write_locked = sync_data.wlock();
        EXPECT_TRUE(write_locked);
        *write_locked = 300;

        // 降级为读锁
        auto read_locked = std::move(write_locked).downgrade_to_rlock();
        EXPECT_TRUE(read_locked);
        EXPECT_EQ(*read_locked, 300);
    }

    // 测试写锁降级为升级锁
    {
        auto write_locked = sync_data.wlock();
        EXPECT_TRUE(write_locked);
        *write_locked = 400;

        // 降级为升级锁
        auto upgrade_locked = std::move(write_locked).downgrade_to_ulock();
        EXPECT_TRUE(upgrade_locked);
        EXPECT_EQ(*upgrade_locked, 400);

        // 升级锁可以读取和修改数据
        *upgrade_locked = 500;
        EXPECT_EQ(*upgrade_locked, 500);
    }
}

/**
 * @brief 测试升级锁与读锁的并发
 */
TEST_F(mutex_box_test, upgrade_lock_concurrency) {
    using upgrade_mutex = mc::sync::shared_mutex;
    mutex_box<int, upgrade_mutex> sync_data(42);

    std::atomic<bool> reader_started{false};
    std::atomic<bool> reader_finished{false};
    std::atomic<bool> upgrade_acquired{false};

    std::thread reader([&]() {
        reader_started   = true;
        auto read_locked = sync_data.rlock();
        EXPECT_TRUE(read_locked);
        EXPECT_EQ(*read_locked, 42);

        // 持有读锁一段时间
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        reader_finished = true;
    });

    // 等待读线程启动
    while (!reader_started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 获取升级锁（应该与读锁共存）
    auto upgrade_locked = sync_data.ulock();
    EXPECT_TRUE(upgrade_locked);
    EXPECT_EQ(*upgrade_locked, 42);
    upgrade_acquired = true;

    reader.join();
    EXPECT_TRUE(reader_finished);
    EXPECT_TRUE(upgrade_acquired);
}

/**
 * @brief 测试升级锁超时功能
 */
TEST_F(mutex_box_test, upgrade_lock_timeout) {
    using upgrade_mutex = mc::sync::shared_mutex;
    mutex_box<int, upgrade_mutex> sync_data(42);

    // 先获取一个升级锁
    auto locked1 = sync_data.ulock();
    EXPECT_TRUE(locked1);

    // 尝试超时获取另一个升级锁
    auto locked2 = sync_data.try_ulock_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(locked2);
}

/**
 * @brief 测试升级锁的 with_ulock 功能
 */
TEST_F(mutex_box_test, upgrade_lock_with_functions) {
    using upgrade_mutex = mc::sync::shared_mutex;
    mutex_box<int, upgrade_mutex> sync_data(42);

    // 测试 with_ulock
    int result = sync_data.with_ulock([](auto& value) {
        EXPECT_EQ(value, 42);
        value = 600;
        return value * 2;
    });

    EXPECT_EQ(result, 1200);

    // 验证数据确实被修改了（使用作用域确保读锁被释放）
    {
        auto locked = sync_data.rlock();
        EXPECT_EQ(*locked, 600);
    } // 读锁在这里被释放

    // 测试 with_ulock_ptr
    bool success = sync_data.with_ulock_ptr([](auto locked_ptr) {
        EXPECT_TRUE(locked_ptr);
        EXPECT_EQ(*locked_ptr, 600);

        // 检查锁是否有效
        if (!locked_ptr) {
            return false;
        }

        // 尝试升级为写锁
        auto write_ptr = std::move(locked_ptr).try_upgrade_to_wlock();
        if (write_ptr && write_ptr.has_value()) {
            auto& write_lock = *write_ptr.value();
            write_lock       = 700;
            return true;
        }
        return false;
    });

    EXPECT_TRUE(success);

    // 验证数据确实被修改了
    {
        auto final_locked = sync_data.rlock();
        EXPECT_EQ(*final_locked, 700);
    }
}
