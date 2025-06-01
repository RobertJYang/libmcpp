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

#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include <mc/future.h>

using namespace std::chrono_literals;

namespace {
class FuturesTest : public ::testing::Test {
public:
    void SetUp() override {
        io_context = std::make_unique<boost::asio::io_context>();
    }

    void TearDown() override {
        io_context->stop();
        io_context->run();
    }

    boost::asio::io_context& get_io_context() {
        return *io_context;
    }

    void run_to_completion() {
        io_context->run();
    }

    template <typename Rep, typename Period>
    void run_for(const std::chrono::duration<Rep, Period>& timeout) {
        io_context->run_for(timeout);
    }

private:
    std::unique_ptr<boost::asio::io_context> io_context;
};
} // namespace

// === 基础功能测试 ===

// 测试 promise 和 future 的基本功能
TEST_F(FuturesTest, BasicPromiseFuture) {
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    promise.set_value(42);
    EXPECT_EQ(future.get(), 42);
}

// 测试 catch_error 捕获异常
TEST_F(FuturesTest, BasicErrorHandling) {
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int) {
        throw std::runtime_error("测试异常");
    }).catch_error([](const mc::exception& e) {
        return std::string(e.what());
    });

    promise.set_value(0);
    run_to_completion();
    EXPECT_EQ(future.get(), "测试异常");
}

// === 执行策略测试 ===

// 测试 async 执行策略
TEST_F(FuturesTest, AsyncExecutionPolicy) {
    auto promise    = mc::make_promise<int>(get_io_context());
    auto start_time = std::chrono::steady_clock::now();

    auto future = promise.get_future().then([](int value) {
        std::this_thread::sleep_for(100ms);
        return value * 2;
    }, mc::launch::async);

    promise.set_value(20);
    run_to_completion();

    auto result  = future.get();
    auto elapsed = std::chrono::steady_clock::now() - start_time;

    EXPECT_EQ(result, 40);
    EXPECT_GE(elapsed, 100ms);
}

// 测试延迟执行
TEST_F(FuturesTest, DeferredExecutionPolicy) {
    auto promise = mc::make_promise<int>(get_io_context());

    auto future = promise.get_future().then([](int value) {
        return value * 2;
    }, mc::launch::deferred);

    promise.set_value(20);
    run_to_completion();

    EXPECT_EQ(future.get(), 40);
}

// === 超时测试 ===

// 测试超时（同步）
TEST_F(FuturesTest, BasicTimeout) {
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    EXPECT_THROW(future.get_for(50ms), mc::timeout_exception);
}

// === 链式调用测试 ===

// 测试链式调用，返回最后一个 future 的值
TEST_F(FuturesTest, SimpleChain) {
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int value) {
        return value * 2;
    }).then([](int value) {
        return value + 1;
    });

    promise.set_value(20);
    run_to_completion();
    EXPECT_EQ(future.get(), 41);
}

// 测试链式调用，允许在 then 中返回 future
TEST_F(FuturesTest, ChainWithFutureReturn) {
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([&](int value) {
        if (value == 0) {
            return mc::resolve(2.0, get_io_context());
        }
        auto p2 = mc::make_promise<double>(get_io_context());
        p2.set_value(value * 2.0);
        return p2.get_future();
    }).then([](double value) {
        return value + 1;
    });

    promise.set_value(20);
    run_to_completion();
    EXPECT_EQ(future.get(), 41);
}

// === Catch Error 测试 ===

// 测试 catch_error 捕获异常
TEST_F(FuturesTest, CatchErrorWithException) {
    int64_t code;
    auto    promise = mc::make_promise<int>(get_io_context());
    auto    future  = promise.get_future().then([](int) {
        throw std::runtime_error("测试异常");
        return 42;
    }).catch_error([&code](const mc::exception& e) {
        code = e.code();
    });

    promise.set_value(10);
    run_to_completion();
    future.get();
    EXPECT_EQ(code, mc::std_exception_code);
}

// 测试 catch_error 无返回值
TEST_F(FuturesTest, CatchErrorWithoutException) {
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int value) {
        return value * 2;
    }).catch_error([](const mc::exception& e) {
        return -1;
    }).then([](int value) {
        EXPECT_EQ(value, 42);
    });

    promise.set_value(21);
    run_to_completion();
    future.get();
}

// 测试 catch_error 无返回值
TEST_F(FuturesTest, CatchErrorVoidReturnWithException) {
    auto promise = mc::make_promise<int>(get_io_context());
    bool caught  = false;

    auto future = promise.get_future().then([](int) -> void {
        throw std::runtime_error("测试异常");
    }).catch_error([&caught](const mc::exception&) {
        caught = true;
    });

    promise.set_value(42);
    run_to_completion();
    future.get();
    EXPECT_TRUE(caught);
}

// 测试 catch_error 无返回值
TEST_F(FuturesTest, CatchErrorVoidReturnWithoutException) {
    auto promise = mc::make_promise<int>(get_io_context());
    bool caught  = false;

    auto future = promise.get_future().then([](int) -> void {
        // 正常执行，无返回值
    }).catch_error([&caught](const mc::exception&) {
        caught = true;
    });

    promise.set_value(42);
    run_to_completion();
    future.get();
    EXPECT_FALSE(caught);
}

// === Error Recovery 测试 ===

// 测试 catch_error 恢复值
TEST_F(FuturesTest, ErrorRecoveryWithIntValue) {
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int value) {
        if (value == 0) {
            throw std::runtime_error("值不能为0");
        }
        return value * 2;
    }).catch_error([](const mc::exception&) {
        return 10; // 恢复默认值
    }).then([](int value) {
        return value + 5;
    });

    promise.set_value(0); // 触发异常
    run_to_completion();
    EXPECT_EQ(future.get(), 15); // 10 + 5
}

// 测试 catch_error 链式调用
TEST_F(FuturesTest, ErrorRecoveryChain) {
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int value) {
        return value * 2; // 正常情况
    }).catch_error([](const mc::exception&) {
        throw std::runtime_error("恢复失败");
        return -1;
    }).catch_error([](const mc::exception&) {
        return 99; // 最终恢复值
    });

    promise.set_value(5);
    run_to_completion();
    EXPECT_EQ(future.get(), 10); // 5 * 2，没有触发异常
}

// === Finally 测试 ===

// 测试 finally，在 future 完成时，调用 finally 函数
TEST_F(FuturesTest, FinallyWithSuccess) {
    bool cleanup_called = false;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int value) {
        return value * 2;
    }).finally([&cleanup_called]() {
        cleanup_called = true;
    });

    promise.set_value(10);
    run_to_completion();
    EXPECT_EQ(future.get(), 20);
    EXPECT_TRUE(cleanup_called);
}

// 测试 finally，在 future 完成时，调用 finally 函数
TEST_F(FuturesTest, FinallyWithException) {
    bool cleanup_called = false;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int) {
        throw std::runtime_error("测试异常");
        return 42;
    }).finally([&cleanup_called]() {
        cleanup_called = true;
    });

    promise.set_value(10);
    run_to_completion();
    EXPECT_THROW(future.get(), std::exception);
    EXPECT_TRUE(cleanup_called);
}

// === Tap 测试 ===

// 测试 tap，在 future 完成时，调用 tap 函数，但不会影响 future 的值
TEST_F(FuturesTest, TapWithSuccess) {
    int observed_value = 0;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int value) {
        return value * 2;
    }).tap([&observed_value](int value) {
        observed_value = value;
    });

    promise.set_value(10);
    run_to_completion();
    EXPECT_EQ(future.get(), 20);
    EXPECT_EQ(observed_value, 20);
}

// 测试 tap，在 future 完成时，调用 tap 函数，但不会影响 future 的值
TEST_F(FuturesTest, TapWithException) {
    int observed_value = -1;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int) {
        throw std::runtime_error("测试异常");
        return 42;
    }).tap([&observed_value](int value) {
        observed_value = value; // 不应该被调用
    });

    promise.set_value(10);
    run_to_completion();
    EXPECT_THROW(future.get(), std::exception);
    EXPECT_EQ(observed_value, -1); // 没有被调用
}

// === All/Any 测试 ===

// 测试 all，所有子 future 完成时，all 完成
TEST_F(FuturesTest, AllWithSuccess) {
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<std::string>(get_io_context());

    auto all_future = mc::all(p1.get_future(), p2.get_future(), p3.get_future());

    p1.set_value(42);
    p2.set_value(3.14);
    p3.set_value("hello");

    run_to_completion();
    auto result = all_future.get();

    EXPECT_EQ(std::get<0>(result), 42);
    EXPECT_DOUBLE_EQ(std::get<1>(result), 3.14);
    EXPECT_EQ(std::get<2>(result), "hello");
}

// 任意一个子 future 抛出异常时，all 也会异常
TEST_F(FuturesTest, AllWithException) {
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<std::string>(get_io_context());

    auto all_future = mc::all(p1.get_future(), p2.get_future(), p3.get_future());

    p1.set_value(42);
    p2.set_exception(std::make_exception_ptr(std::runtime_error("测试异常")));
    p3.set_value("hello");

    run_to_completion();
    EXPECT_THROW(all_future.get(), std::exception);
}

// 撤销 all 时，子 future 也会被撤销
TEST_F(FuturesTest, AllCancelPropagation) {
    // 测试当all返回的future被取消时，子future也被取消
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<std::string>(get_io_context());

    auto f1 = p1.get_future();
    auto f2 = p2.get_future();
    auto f3 = p3.get_future();

    // 注册取消回调来验证子future确实被取消
    bool f1_canceled = false;
    bool f2_canceled = false;
    bool f3_canceled = false;

    f1.on_cancel([&f1_canceled]() {
        f1_canceled = true;
    });
    f2.on_cancel([&f2_canceled]() {
        f2_canceled = true;
    });
    f3.on_cancel([&f3_canceled]() {
        f3_canceled = true;
    });

    auto all_future = mc::all(std::move(f1), std::move(f2), std::move(f3));

    // 取消all_future，应该传播到子future
    all_future.cancel();

    // 验证all_future被取消
    EXPECT_THROW(all_future.get(), mc::canceled_exception);

    // 验证所有子future都被取消
    EXPECT_TRUE(f1_canceled);
    EXPECT_TRUE(f2_canceled);
    EXPECT_TRUE(f3_canceled);
}

// 测试 any，任何一个 future 完成时，any 完成
TEST_F(FuturesTest, AnyWithSuccess) {
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<std::string>(get_io_context());

    auto any_future = mc::any(p1.get_future(), p2.get_future(), p3.get_future());

    p2.set_value(3.14); // 第二个完成

    run_to_completion();
    auto result = any_future.get();

    EXPECT_EQ(result.first, 1); // 索引 1（第二个）
    EXPECT_DOUBLE_EQ(std::get<double>(result.second), 3.14);
}

// 测试当any返回的future被取消时，子future也被取消
TEST_F(FuturesTest, AnyCancelPropagation) {
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<std::string>(get_io_context());

    auto f1 = p1.get_future();
    auto f2 = p2.get_future();
    auto f3 = p3.get_future();

    // 注册取消回调来验证子future确实被取消
    bool f1_canceled = false;
    bool f2_canceled = false;
    bool f3_canceled = false;

    f1.on_cancel([&f1_canceled]() {
        f1_canceled = true;
    });
    f2.on_cancel([&f2_canceled]() {
        f2_canceled = true;
    });
    f3.on_cancel([&f3_canceled]() {
        f3_canceled = true;
    });

    auto any_future = mc::any(std::move(f1), std::move(f2), std::move(f3));

    // 取消any_future，应该传播到子future
    any_future.cancel();

    // 验证any_future被取消
    EXPECT_THROW(any_future.get(), mc::canceled_exception);

    // 验证所有子future都被取消
    EXPECT_TRUE(f1_canceled);
    EXPECT_TRUE(f2_canceled);
    EXPECT_TRUE(f3_canceled);
}

// 测试当any的所有子future都被取消时，any也被取消
TEST_F(FuturesTest, AnyAllChildrenCanceled) {
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<std::string>(get_io_context());

    auto f1 = p1.get_future();
    auto f2 = p2.get_future();
    auto f3 = p3.get_future();

    auto any_future = mc::any(std::move(f1), std::move(f2), std::move(f3));

    // 取消所有子future
    p1.cancel();
    p2.cancel();
    p3.cancel();

    run_to_completion();

    // 验证any_future被取消
    EXPECT_THROW(any_future.get(), mc::canceled_exception);
}

// === 工厂函数测试 ===

// 测试 resolve
TEST_F(FuturesTest, MakeReadyFuture) {
    auto f1 = mc::resolve(42, get_io_context());
    auto f2 = mc::resolve(3.14, get_io_context());

    // 创建一个立即完成的 void future
    auto promise = mc::make_promise<void>(get_io_context());
    auto f3      = promise.get_future();
    promise.set_value();

    EXPECT_EQ(f1.get(), 42);
    EXPECT_DOUBLE_EQ(f2.get(), 3.14);
    f3.get(); // void future
}

// 测试 reject
TEST_F(FuturesTest, MakeExceptionalFuture) {
    // 1. 使用异常指针构造 reject future
    auto ex = std::make_exception_ptr(std::runtime_error("测试异常"));
    auto f1 = mc::reject<int>(ex, get_io_context());
    auto f2 = mc::reject<void>(ex, get_io_context());

    // 2. 使用异常对象构造 reject future
    auto f3 = mc::reject<void>(std::runtime_error("测试异常"), get_io_context());

    EXPECT_THROW(f1.get(), std::runtime_error);
    EXPECT_THROW(f2.get(), std::runtime_error);
    EXPECT_THROW(f3.get(), std::runtime_error);
}

// === 超时函数测试 ===

// 测试为 future 添加超时
TEST_F(FuturesTest, TimeoutFunctionSuccess) {
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = mc::timeout(promise.get_future(), 100ms, get_io_context());

    promise.set_value(42);
    run_to_completion();
    EXPECT_EQ(future.get(), 42);
}

// 测试为 future 添加超时
TEST_F(FuturesTest, TimeoutFunctionTimeout) {
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = mc::timeout(promise.get_future(), 50ms, get_io_context());

    // 不设置值，让其超时
    run_for(100ms);
    EXPECT_THROW(future.get(), mc::timeout_exception);
}

// 测试当 result_future 被取消时，内部定时器被正确取消
TEST_F(FuturesTest, TimeoutFunctionCancelOnResultFutureCancel) {
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = mc::timeout(promise.get_future(), 1000ms, get_io_context()); // 长超时

    // 取消 result_future，应该取消内部定时器
    future.cancel();

    EXPECT_THROW(future.get(), mc::canceled_exception);
    // 内部定时器应该被取消，不会产生额外的回调
}

// 测试当原始 future 在创建timeout前被取消时的行为
TEST_F(FuturesTest, TimeoutFunctionCancelOnOriginalPromiseCancel) {
    auto promise         = mc::make_promise<int>(get_io_context());
    auto original_future = promise.get_future();

    // 先取消原始 future，然后创建 timeout future
    original_future.cancel();
    auto timeout_future = mc::timeout(std::move(original_future), 1000ms, get_io_context());

    run_to_completion();

    // 原始 future 被取消，timeout_future 应该也抛出 canceled_exception
    EXPECT_THROW(timeout_future.get(), mc::canceled_exception);
}

// 测试当原始 promise 在创建timeout后被取消时的行为
TEST_F(FuturesTest, TimeoutFunctionCancelOnPromiseAfterTimeout) {
    auto promise         = mc::make_promise<int>(get_io_context());
    auto original_future = promise.get_future();

    // 先创建 timeout future，然后取消原始 promise
    auto timeout_future = mc::timeout(std::move(original_future), 1000ms, get_io_context());
    promise.cancel(); // 通过promise取消，因为original_future已经被move到内部了

    run_to_completion();

    // 原始 promise 被取消，timeout_future 应该也抛出 canceled_exception
    EXPECT_THROW(timeout_future.get(), mc::canceled_exception);
}

// 测试传入已经cancelled的future时的优化行为
TEST_F(FuturesTest, TimeoutFunctionWithAlreadyCancelledFuture) {
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // 先取消future
    future.cancel();

    // 此时future已经cancelled，timeout函数应该返回已取消的future
    auto timeout_future = mc::timeout(std::move(future), 1000ms, get_io_context());

    // 应该抛出取消异常
    EXPECT_THROW(timeout_future.get(), mc::canceled_exception);
}

// === 延迟执行测试 ===

// 测试使用 boost::asio::steady_timer 延时执行
TEST_F(FuturesTest, DelayedExecution) {
    auto start_time = std::chrono::steady_clock::now();

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // 在 100ms 后设置值
    boost::asio::steady_timer timer(get_io_context(), 100ms);
    timer.async_wait([&promise](const boost::system::error_code&) {
        // 延时设置 promise 值
        promise.set_value(42);
    });

    run_to_completion();
    auto elapsed = std::chrono::steady_clock::now() - start_time;

    EXPECT_EQ(future.get(), 42);
    EXPECT_GE(elapsed, 100ms);
}

// 测试使用 mc::delay 延时执行
TEST_F(FuturesTest, DelayedFuture) {
    auto start_time = std::chrono::steady_clock::now();

    auto future = mc::delay(100ms, get_io_context()).then([]() {
        return 42;
    });

    run_to_completion();
    auto elapsed = std::chrono::steady_clock::now() - start_time;

    EXPECT_EQ(future.get(), 42);
    EXPECT_GE(elapsed, 100ms);
}

// === 取消测试 ===

// 测试取消延时执行的 future
TEST_F(FuturesTest, CancelDelayedFuture) {
    auto delayed_future = mc::delay(1000ms, get_io_context());
    delayed_future.cancel();
    EXPECT_THROW(delayed_future.get(), mc::canceled_exception);
}

// 测试取消延时执行的链式调用
TEST_F(FuturesTest, CancelChainedFuture) {
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int value) {
        return value * 2;
    });

    future.cancel();
    promise.set_value(10);
    EXPECT_THROW(future.get(), mc::canceled_exception);
}

// 测试取消延时执行的回调，并验证 cancel 回调是否被调用
TEST_F(FuturesTest, CancelCallbackExecution) {
    bool callback_called = false;
    auto delayed_future  = mc::delay(100ms, get_io_context());

    delayed_future.on_cancel([&callback_called]() {
        callback_called = true;
    });

    delayed_future.cancel();
    EXPECT_TRUE(callback_called);
}

// 测试嵌套调用 cancel 回调
TEST_F(FuturesTest, CancelCallbackNested) {
    std::vector<int> call_order;
    auto             delayed_future = mc::delay(100ms, get_io_context());

    delayed_future.on_cancel([&]() {
        call_order.push_back(1);

        delayed_future.on_cancel([&call_order]() {
            call_order.push_back(2);
        });
    });

    delayed_future.cancel();
    EXPECT_EQ(call_order.size(), 2);
    EXPECT_EQ(call_order[0], 1);
    EXPECT_EQ(call_order[1], 2);
}

// === 异常类型测试 ===

TEST_F(FuturesTest, CanceledException) {
    auto delayed_future = mc::delay(1000ms, get_io_context());
    delayed_future.cancel();
    EXPECT_THROW(delayed_future.get(), mc::canceled_exception);
}

TEST_F(FuturesTest, TimeoutException) {
    auto promise        = mc::make_promise<int>(get_io_context());
    auto future         = promise.get_future();
    auto timeout_future = mc::timeout(std::move(future), 50ms, get_io_context());

    run_for(100ms);
    EXPECT_THROW(timeout_future.get(), mc::timeout_exception);
}

TEST_F(FuturesTest, ExceptionInfoPreservationInCatchError) {
    mc::exception_ptr ex;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int value) -> int {
        MC_THROW(mc::timeout_exception, "测试超时: 期望${expected}ms, 实际${actual}ms",
                  ("expected", 1000)("actual", 2000));
        return value;
    }).catch_error([&](const mc::exception& e) -> int {
        ex = e.dynamic_copy_exception();
        return static_cast<int>(e.code());
    });

    promise.set_value(42);
    run_to_completion();

    auto result = future.get();
    EXPECT_EQ(ex->code(), mc::timeout_exception_code);
    EXPECT_STREQ(ex->what(), "测试超时: 期望1000ms, 实际2000ms");
    EXPECT_EQ(result, mc::timeout_exception_code);
}

TEST_F(FuturesTest, CancelExceptionHandling) {
    mc::exception_ptr ex;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int value) -> int {
        MC_THROW(mc::canceled_exception, "用户取消: ${reason}", ("reason", "手动停止"));
        return value;
    }).catch_error([&](const mc::exception& e) -> int {
        ex = e.dynamic_copy_exception();
        return static_cast<int>(e.code());
    });

    promise.set_value(42);
    run_to_completion();

    auto result = future.get();

    EXPECT_TRUE(ex != nullptr);
    EXPECT_EQ(ex->code(), mc::canceled_exception_code);
    EXPECT_STREQ(ex->what(), "用户取消: 手动停止");
    EXPECT_EQ(result, mc::canceled_exception_code);
}

TEST_F(FuturesTest, StdExceptionHandling) {
    mc::exception_ptr ex;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int) -> int {
        throw std::invalid_argument("参数无效");
    }).catch_error([&](const mc::exception& e) -> int {
        ex = e.dynamic_copy_exception();
        return -2;
    });

    promise.set_value(42);
    run_to_completion();

    auto result = future.get();
    EXPECT_TRUE(ex != nullptr);
    EXPECT_EQ(ex->code(), mc::std_exception_code);
    EXPECT_STREQ(ex->what(), "参数无效");
    EXPECT_EQ(result, -2);
}

// === 详细的取消传递测试 ===

TEST_F(FuturesTest, AnyFirstSuccessCancel) {
    // 测试any中第一个成功时，其他future被取消
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<std::string>(get_io_context());

    auto f1 = p1.get_future();
    auto f2 = p2.get_future();
    auto f3 = p3.get_future();

    // 注册取消回调来验证其他future确实被取消
    bool f2_canceled = false;
    bool f3_canceled = false;

    f2.on_cancel([&f2_canceled]() {
        f2_canceled = true;
    });
    f3.on_cancel([&f3_canceled]() {
        f3_canceled = true;
    });

    auto any_future = mc::any(std::move(f1), std::move(f2), std::move(f3));

    // 第一个完成
    p1.set_value(42);
    run_to_completion();

    auto result = any_future.get();
    EXPECT_EQ(result.first, 0); // 第一个future的索引
    EXPECT_EQ(std::get<int>(result.second), 42);

    // 验证其他future被取消
    EXPECT_TRUE(f2_canceled);
    EXPECT_TRUE(f3_canceled);
}

TEST_F(FuturesTest, AllOneChildCancelPropagation) {
    // 测试all中一个子future被取消时，其他也被取消
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<std::string>(get_io_context());

    auto f1 = p1.get_future();
    auto f2 = p2.get_future();
    auto f3 = p3.get_future();

    // 注册取消回调来验证其他future确实被取消
    bool f1_canceled = false;
    bool f2_canceled = false;

    f1.on_cancel([&f1_canceled]() {
        f1_canceled = true;
    });
    f2.on_cancel([&f2_canceled]() {
        f2_canceled = true;
    });

    auto all_future = mc::all(std::move(f1), std::move(f2), std::move(f3));

    // 设置前两个值
    p1.set_value(42);
    p2.set_value(3.14);

    // 第三个被取消
    p3.cancel();

    run_to_completion();

    // all_future应该抛出取消异常
    EXPECT_THROW(all_future.get(), mc::canceled_exception);

    // f1、f2 在 p3 cancel 前已经 ready，所以不会被取消
    EXPECT_FALSE(f1_canceled);
    EXPECT_FALSE(f2_canceled);
}

TEST_F(FuturesTest, AnySuccessAfterSomeCanceled) {
    // 测试any中部分future被取消，但仍有成功的情况
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<std::string>(get_io_context());

    auto f1 = p1.get_future();
    auto f2 = p2.get_future();
    auto f3 = p3.get_future();

    auto any_future = mc::any(std::move(f1), std::move(f2), std::move(f3));

    // 第一个和第三个被取消
    p1.cancel();
    p3.cancel();

    // 第二个成功
    p2.set_value(3.14);

    run_to_completion();

    auto result = any_future.get();
    EXPECT_EQ(result.first, 1); // 第二个future的索引
    EXPECT_DOUBLE_EQ(std::get<double>(result.second), 3.14);
}
