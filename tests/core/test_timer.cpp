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
    timer_service() : mc::core::service_base("org.openubmc.test_service") {
    }

    ~timer_service() override {
    }

    void test_single_shot() {
        m_is_called.set_value(true);
    }

    std::promise<bool> m_is_called;
};

class timer_test : public mc::test::TestWithApplication {
protected:
    void SetUp() override {
        service = mc::make_shared<timer_service>();
    }

    void TearDown() override {
        service.reset();
    }

    mc::shared_ptr<timer_service> service;
};

} // namespace

TEST_F(timer_test, test_single_shot) {
    mc::core::timer::single_shot(mc::milliseconds(10), service, &timer_service::test_single_shot);

    auto future = service->m_is_called.get_future();
    auto status = future.wait_for(std::chrono::milliseconds(100));
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(future.get());
}

// 测试周期性定时器
TEST_F(timer_test, test_periodic_timer) {
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
    timer->start(mc::milliseconds(100));

    auto status = wait.get_future().wait_for(std::chrono::milliseconds(500));
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_EQ(count, expected_count);

    timer->stop();
}

// 测试取消定时器
TEST_F(timer_test, test_cancel_timer) {
    auto timer = mc::core::timer::single_shot(mc::milliseconds(100), service,
                                              &timer_service::test_single_shot);
    timer->stop();

    auto future = service->m_is_called.get_future();
    auto status = future.wait_for(std::chrono::milliseconds(300));
    EXPECT_EQ(status, std::future_status::timeout);
}

// 测试多个定时器同时运行
TEST_F(timer_test, test_multiple_timers) {
    std::promise<bool> timer1_called;
    std::promise<bool> timer2_called;
    std::promise<bool> timer3_called;

    mc::core::timer::single_shot(mc::milliseconds(100), service, [&]() {
        timer1_called.set_value(true);
    });
    mc::core::timer::single_shot(mc::milliseconds(200), service, [&]() {
        timer2_called.set_value(true);
    });
    mc::core::timer::single_shot(mc::milliseconds(300), service, [&]() {
        timer3_called.set_value(true);
    });

    auto s1 = timer1_called.get_future().wait_for(std::chrono::milliseconds(500));
    auto s2 = timer2_called.get_future().wait_for(std::chrono::milliseconds(500));
    auto s3 = timer3_called.get_future().wait_for(std::chrono::milliseconds(500));

    EXPECT_EQ(s1, std::future_status::ready);
    EXPECT_EQ(s2, std::future_status::ready);
    EXPECT_EQ(s3, std::future_status::ready);
}

// 测试嵌套定时器
TEST_F(timer_test, test_nested_timers) {
    int                count = 0;
    std::promise<void> wait;

    mc::core::timer::single_shot(mc::milliseconds(100), [&]() {
        count++;

        mc::core::timer::single_shot(mc::milliseconds(100), [&]() {
            count++;
            wait.set_value();
        });
    });

    auto status = wait.get_future().wait_for(std::chrono::milliseconds(300));
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_EQ(count, 2);
}
