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
#include <mc/core/application.h>
#include <mc/core/timer.h>
#include <mc/dict.h>
#include <test_utilities/test_base.h>

namespace {

struct timer_service : public mc::core::service_base {
    timer_service()
        : mc::core::service_base("org.openubmc.test_service")
    {
    }

    ~timer_service() override
    {
    }

    void test_single_shot()
    {
        m_is_called.set_value(true);
    }

    std::promise<bool> m_is_called;
};

class timer_test : public mc::test::TestWithApplication {
protected:
    void SetUp() override
    {
        service = mc::make_shared<timer_service>();
    }

    void TearDown() override
    {
        service.reset();
    }

    mc::shared_ptr<timer_service> service;
};

} // namespace

TEST_F(timer_test, test_single_shot)
{
    mc::core::timer::single_shot(mc::milliseconds(10), service, &timer_service::test_single_shot);

    auto future = service->m_is_called.get_future();
    auto status = future.wait_for(std::chrono::milliseconds(100));
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(future.get());
}

// 测试周期性定时器
TEST_F(timer_test, test_periodic_timer)
{
    auto timer = mc::make_shared<mc::core::timer>(service.get());

    int                count          = 0;
    int                expected_count = 3;
    std::promise<void> wait;

    timer->timeout.connect([&]() {
        count++;
        if (count >= expected_count) {
            timer->stop();
            wait.set_value();
        }
    });
    timer->set_single_shot(false);
    timer->start(mc::milliseconds(50)); // 从 100ms 减少到 50ms

    auto status = wait.get_future().wait_for(std::chrono::milliseconds(250)); // 从 500ms 减少到 250ms
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_EQ(count, expected_count);

    timer->stop();
}

// 测试取消定时器
TEST_F(timer_test, test_cancel_timer)
{
    auto timer = mc::core::timer::single_shot(mc::milliseconds(100), service,
                                              &timer_service::test_single_shot);
    timer->stop();

    auto future = service->m_is_called.get_future();
    auto status = future.wait_for(std::chrono::milliseconds(150)); // 从 300ms 减少到 150ms
    EXPECT_EQ(status, std::future_status::timeout);
}

// 测试多个定时器同时运行
TEST_F(timer_test, test_multiple_timers)
{
    std::promise<bool> timer1_called;
    std::promise<bool> timer2_called;
    std::promise<bool> timer3_called;

    // 保存 timer 指针，确保在测试结束前它们不会被销毁
    auto timer1 = mc::core::timer::single_shot(mc::milliseconds(30), service, [&]() { // 从 100ms 减少到 30ms
        timer1_called.set_value(true);
    });
    auto timer2 = mc::core::timer::single_shot(mc::milliseconds(60), service, [&]() { // 从 200ms 减少到 60ms
        timer2_called.set_value(true);
    });
    auto timer3 = mc::core::timer::single_shot(mc::milliseconds(90), service, [&]() { // 从 300ms 减少到 90ms
        timer3_called.set_value(true);
    });

    auto s1 = timer1_called.get_future().wait_for(std::chrono::milliseconds(200)); // 从 500ms 减少到 200ms
    auto s2 = timer2_called.get_future().wait_for(std::chrono::milliseconds(200));
    auto s3 = timer3_called.get_future().wait_for(std::chrono::milliseconds(200));

    EXPECT_EQ(s1, std::future_status::ready);
    EXPECT_EQ(s2, std::future_status::ready);
    EXPECT_EQ(s3, std::future_status::ready);

    // 确保所有定时器都已停止（single_shot 会自动停止，但显式停止更安全）
    timer1->stop();
    timer2->stop();
    timer3->stop();
}

// 测试嵌套定时器
TEST_F(timer_test, test_nested_timers)
{
    int                count = 0;
    std::promise<void> wait;

    // 保存第一个 timer 指针，确保在测试结束前它不会被销毁
    auto timer1 = mc::core::timer::single_shot(mc::milliseconds(50), service, [&]() { // 从 100ms 减少到 50ms
        count++;

        // 第二个 timer 在第一个 timer 的回调中创建，会在回调完成后自动停止
        auto timer2 = mc::core::timer::single_shot(mc::milliseconds(50), service, [&]() { // 从 100ms 减少到 50ms
            count++;
            wait.set_value();
        });
        // 保存 timer2 的引用，防止在回调执行前被销毁
        MC_UNUSED(timer2);
    });

    auto status = wait.get_future().wait_for(std::chrono::milliseconds(150)); // 从 300ms 减少到 150ms
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_EQ(count, 2);

    // 确保第一个定时器已停止
    timer1->stop();
}

// 测试定时器间隔设置
TEST_F(timer_test, test_timer_interval)
{
    auto timer = mc::make_shared<mc::core::timer>(service.get());

    // 测试设置间隔
    timer->set_interval(mc::milliseconds(50));
    EXPECT_EQ(timer->interval(), mc::milliseconds(50));

    // 测试设置单次触发
    timer->set_single_shot(true);
    EXPECT_TRUE(timer->is_single_shot());

    timer->set_single_shot(false);
    EXPECT_FALSE(timer->is_single_shot());
}

// 测试定时器状态检查
TEST_F(timer_test, test_timer_status)
{
    auto timer = mc::make_shared<mc::core::timer>(service.get());

    // 初始状态应该是不活跃的
    EXPECT_FALSE(timer->is_active());

    // 启动定时器
    timer->start(mc::milliseconds(100));
    EXPECT_TRUE(timer->is_active());

    // 停止定时器（异步停），轮询等待直至不活跃或超时
    timer->stop();
    for (int i = 0; i < 10 && timer->is_active(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_FALSE(timer->is_active());
}

// 测试定时器重复启动
TEST_F(timer_test, test_timer_restart)
{
    auto timer = mc::make_shared<mc::core::timer>(service.get());

    int                count = 0;
    std::promise<void> first_trigger;
    std::promise<void> second_trigger;

    timer->timeout.connect([&]() {
        count++;
        if (count == 1) {
            first_trigger.set_value();
        } else if (count == 2) {
            second_trigger.set_value();
        }
    });

    // 第一次启动
    timer->start(mc::milliseconds(50));
    // 等待第一次触发
    auto status1 = first_trigger.get_future().wait_for(std::chrono::milliseconds(100));
    EXPECT_EQ(status1, std::future_status::ready);

    // 第二次启动（应该重置定时器）
    timer->start(mc::milliseconds(50));
    // 等待第二次触发
    auto status2 = second_trigger.get_future().wait_for(std::chrono::milliseconds(100));
    EXPECT_EQ(status2, std::future_status::ready);

    // 应该至少触发两次
    EXPECT_GE(count, 2);

    // 停止定时器，确保测试结束时没有未完成的异步操作
    timer->stop();
}

// 测试定时器间隔变更
TEST_F(timer_test, test_timer_interval_change)
{
    auto timer = mc::make_shared<mc::core::timer>(service.get());

    int                count          = 0;
    int                expected_count = 2;
    std::promise<void> done;
    timer->timeout.connect([&]() {
        count++;
        if (count >= expected_count) {
            timer->stop();
            done.set_value();
        }
    });

    // 启动定时器
    timer->start(mc::milliseconds(50)); // 从 100ms 减少到 50ms

    // 在定时器运行期间改变间隔
    std::this_thread::sleep_for(std::chrono::milliseconds(25)); // 从 50ms 减少到 25ms
    timer->set_interval(mc::milliseconds(100));                 // 从 200ms 减少到 100ms

    // 等待回调完成，而不是使用固定的 sleep
    auto status = done.get_future().wait_for(std::chrono::milliseconds(500)); // 从 1000ms 减少到 500ms
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_EQ(count, expected_count);
}

// 测试空回调函数
TEST_F(timer_test, test_null_callback)
{
    // 测试空回调函数应该返回空定时器
    auto timer1 = mc::core::timer::single_shot(mc::milliseconds(100), nullptr);
    EXPECT_FALSE(timer1);

    // 测试空回调函数和上下文
    auto timer2 = mc::core::timer::single_shot(mc::milliseconds(100), nullptr, nullptr);
    EXPECT_FALSE(timer2);
}

// 测试零间隔定时器
TEST_F(timer_test, test_zero_interval_timer)
{
    auto timer = mc::make_shared<mc::core::timer>(service.get());

    int                count          = 0;
    int                expected_count = 5;
    std::promise<void> wait;

    timer->timeout.connect([&]() {
        count++;
        if (count >= expected_count) {
            timer->stop();
            wait.set_value();
        }
    });

    // 设置零间隔（应该立即触发）
    timer->set_interval(mc::milliseconds(0));
    timer->start(mc::milliseconds(0));

    // 等待回调完成，而不是使用 sleep
    auto status = wait.get_future().wait_for(std::chrono::milliseconds(100));
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_EQ(count, expected_count);
}

// 测试定时器析构
TEST_F(timer_test, test_timer_destruction)
{
    std::promise<bool> timer_called;

    {
        auto timer = mc::make_shared<mc::core::timer>(service.get());
        timer->timeout.connect([&]() {
            timer_called.set_value(true);
        });
        timer->start(mc::milliseconds(200));
        // 提前停止，确保析构前取消
        timer->stop();
        // timer 在此处析构
    }

    // 等待一段时间，确保定时器不会在析构后触发
    auto status = timer_called.get_future().wait_for(std::chrono::milliseconds(150));
    EXPECT_EQ(status, std::future_status::timeout);
}
