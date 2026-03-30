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

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

#include <mc/future.h>
#include <mc/futures/callback_list.h>
#include <mc/futures/exceptions.h>
#include <mc/runtime/executor.h>
#include <runtime/include/thread_pool_impl.h>
#include <test_utilities/base.h>

using namespace std::chrono_literals;

namespace {
using native_timer = boost::asio::steady_timer;

template <typename T, typename = void>
struct has_public_get_executor : std::false_type {};

template <typename T>
struct has_public_get_executor<T, std::void_t<decltype(std::declval<T&>().get_executor())>> : std::true_type {};

template <typename T, typename = void>
struct has_public_get_shard : std::false_type {};

template <typename T>
struct has_public_get_shard<T, std::void_t<decltype(std::declval<T&>().get_shard(std::size_t{}))>> : std::true_type {};

template <typename T, typename = void>
struct has_context_method : std::false_type {};

template <typename T>
struct has_context_method<T, std::void_t<decltype(std::declval<T&>().context())>> : std::true_type {};

template <typename T, typename = void>
struct has_on_work_started_method : std::false_type {};

template <typename T>
struct has_on_work_started_method<T, std::void_t<decltype(std::declval<T&>().on_work_started())>> : std::true_type {};

template <typename T, typename = void>
struct has_on_work_finished_method : std::false_type {};

template <typename T>
struct has_on_work_finished_method<T, std::void_t<decltype(std::declval<T&>().on_work_finished())>> : std::true_type {};

template <typename T, typename = void>
struct has_bound_pool_method : std::false_type {};

template <typename T>
struct has_bound_pool_method<T, std::void_t<decltype(std::declval<T&>().bound_pool(nullptr))>> : std::true_type {};

template <typename T, typename = void>
struct has_get_bound_pool_method : std::false_type {};

template <typename T>
struct has_get_bound_pool_method<T, std::void_t<decltype(std::declval<T&>().get_bound_pool())>> : std::true_type {};

class FuturesTest : public mc::test::TestWithRuntime {
public:
    mc::runtime::thread_pool& get_io_context()
    {
        return get_runtime().io();
    }
};

static_assert(std::is_same_v<decltype(std::declval<mc::runtime::runtime_context&>().get_io_executor()),
                             mc::runtime::any_executor>,
              "runtime_context::get_io_executor() 应返回稳定的公共 any_executor");
static_assert(std::is_same_v<decltype(std::declval<mc::runtime::runtime_context&>().get_work_executor()),
                             mc::runtime::any_executor>,
              "runtime_context::get_work_executor() 应返回稳定的公共 any_executor");
static_assert(
    std::is_same_v<decltype(std::declval<mc::runtime::runtime_context&>().get_executor()), mc::runtime::any_executor>,
    "runtime_context::get_executor() 应返回稳定的公共 any_executor");
static_assert(std::is_same_v<decltype(mc::runtime::get_default_executor()), mc::runtime::any_executor>,
              "get_default_executor() 应返回稳定的公共 any_executor");
static_assert(std::is_same_v<decltype(std::declval<mc::runtime::runtime_context&>().create_io_strand()),
                             mc::runtime::any_executor>,
              "runtime_context::create_io_strand() 应返回稳定的公共 any_executor");
static_assert(std::is_same_v<decltype(std::declval<mc::runtime::runtime_context&>().create_work_strand()),
                             mc::runtime::any_executor>,
              "runtime_context::create_work_strand() 应返回稳定的公共 any_executor");
static_assert(!has_public_get_executor<mc::runtime::any_executor>::value, "any_executor 不应暴露具体后端访问器");
static_assert(!std::is_constructible_v<mc::runtime::any_executor, mc::runtime::executor>,
              "any_executor 不应再依赖 mc::runtime::executor 这条额外包装路径");
static_assert(!has_context_method<mc::runtime::any_executor>::value,
              "mc::runtime::any_executor 不应再暴露 Asio execution_context");
static_assert(!boost::asio::is_executor<mc::runtime::any_executor>::value,
              "mc::runtime::any_executor 不应再声明为 Asio executor");
static_assert(!has_context_method<mc::runtime::executor>::value,
              "mc::runtime::executor 不应再暴露 Asio execution_context");
static_assert(!std::is_convertible_v<mc::runtime::executor, boost::asio::any_io_executor>,
              "mc::runtime::executor 不应再暴露 any_io_executor 转换");
static_assert(!std::is_convertible_v<mc::runtime::executor, boost::asio::io_context::executor_type>,
              "mc::runtime::executor 不应再暴露 io_context::executor_type 转换");
static_assert(!has_on_work_started_method<mc::runtime::executor>::value,
              "mc::runtime::executor 不应再暴露 on_work_started");
static_assert(!has_on_work_finished_method<mc::runtime::executor>::value,
              "mc::runtime::executor 不应再暴露 on_work_finished");
static_assert(!has_bound_pool_method<mc::runtime::executor>::value, "mc::runtime::executor 不应再暴露 bound_pool");
static_assert(!has_get_bound_pool_method<mc::runtime::executor>::value,
              "mc::runtime::executor 不应再暴露 get_bound_pool");
static_assert(!std::is_convertible_v<mc::runtime::any_executor, boost::asio::any_io_executor>,
              "mc::runtime::any_executor 不应再暴露 any_io_executor 转换");
static_assert(!std::is_convertible_v<mc::runtime::any_executor, boost::asio::io_context::executor_type>,
              "mc::runtime::any_executor 不应再暴露 io_context::executor_type 转换");
static_assert(!has_context_method<mc::runtime::runtime_executor>::value,
              "mc::runtime::runtime_executor 不应再暴露 Asio execution_context");
static_assert(!std::is_convertible_v<mc::runtime::runtime_executor, boost::asio::any_io_executor>,
              "mc::runtime::runtime_executor 不应再暴露 any_io_executor 转换");
static_assert(!std::is_convertible_v<mc::runtime::runtime_executor, boost::asio::io_context::executor_type>,
              "mc::runtime::runtime_executor 不应再暴露 io_context::executor_type 转换");
static_assert(!boost::asio::is_executor<mc::runtime::runtime_executor>::value,
              "mc::runtime::runtime_executor 不应再声明为 Asio executor");
static_assert(!has_context_method<mc::runtime::runtime_strand>::value,
              "mc::runtime::runtime_strand 不应再暴露 Asio execution_context");
static_assert(!boost::asio::is_executor<mc::runtime::runtime_strand>::value,
              "mc::runtime::runtime_strand 不应再声明为 Asio executor");
static_assert(!std::is_convertible_v<mc::runtime::thread_pool::executor_type, boost::asio::any_io_executor>,
              "mc::runtime::thread_pool::executor_type 不应再暴露 any_io_executor 转换");
static_assert(!std::is_convertible_v<mc::runtime::thread_pool::executor_type, boost::asio::io_context::executor_type>,
              "mc::runtime::thread_pool::executor_type 不应再暴露 io_context::executor_type 转换");
static_assert(!has_context_method<mc::runtime::thread_pool::executor_type>::value,
              "mc::runtime::thread_pool::executor_type 不应再暴露 context()");
static_assert(!has_on_work_started_method<mc::runtime::thread_pool::executor_type>::value,
              "mc::runtime::thread_pool::executor_type 不应再暴露 on_work_started()");
static_assert(!has_on_work_finished_method<mc::runtime::thread_pool::executor_type>::value,
              "mc::runtime::thread_pool::executor_type 不应再暴露 on_work_finished()");
static_assert(!boost::asio::is_executor<mc::runtime::thread_pool::executor_type>::value,
              "mc::runtime::thread_pool::executor_type 不应再声明为 Asio executor");
static_assert(!has_public_get_shard<mc::runtime::thread_pool>::value, "mc::runtime::thread_pool 不应再公开 get_shard");
static_assert(!has_context_method<mc::runtime::immediate_executor>::value,
              "mc::runtime::immediate_executor 不应再暴露 Asio execution_context");
static_assert(!std::is_base_of_v<boost::asio::execution_context, mc::runtime::immediate_context>,
              "mc::runtime::immediate_context 不应再继承 Asio execution_context");
} // namespace

// 测试 promise 和 future 的基本功能
TEST_F(FuturesTest, BasicPromiseFuture)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    promise.set_value(42);
    EXPECT_EQ(future.get(), 42);
}

// 验证阻塞调度线程能继续处理 IO 事件
TEST_F(FuturesTest, WaitOnWorker)
{
    auto& pool = get_io_context();

    auto promise = mc::make_promise<int>(pool.get_executor());
    auto future  = promise.get_future();

    auto done_promise = mc::make_promise<void>();
    auto done_future  = done_promise.get_future();

    pool.get_executor().post([promise, future = std::move(future), done_promise]() mutable {
        // 1. 获取当前 shard
        auto* shard = mc::runtime::detail::thread_pool_impl::current_shard();
        ASSERT_NE(shard, nullptr) << "Must be running on a thread pool worker";

        // 2. 在同一个 shard 上启动一个定时器
        auto timer =
            std::make_shared<native_timer>(mc::runtime::detail::thread_pool_impl::get_native_io_executor(shard));
        timer->expires_after(std::chrono::milliseconds(50));
        timer->async_wait([promise, timer](const boost::system::error_code& ec) mutable {
            if (!ec) {
                promise.set_value(42); // 定时器触发，设置 promise 值
            }
        });

        // 3. 调用 wait()，wait() 会处理了 IO 循环，所以能够继续驱动定时器执行
        future.wait();

        done_promise.set_value();
    });

    auto status = done_future.wait_for(std::chrono::seconds(2000));
    ASSERT_EQ(status, mc::future_status::ready);
}

TEST_F(FuturesTest, ThreadPoolExecutorAcceptsMoveOnlyTask)
{
    auto done = std::make_unique<std::promise<void>>();
    auto wait = done->get_future();

    std::atomic<bool> executed{false};

    get_io_context().get_executor().post([done = std::move(done), &executed]() mutable {
        executed.store(true, std::memory_order_release);
        done->set_value();
    });

    EXPECT_EQ(wait.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(executed.load(std::memory_order_acquire));
}

TEST_F(FuturesTest, AnyExecutorAcceptsMoveOnlyTask)
{
    auto done = std::make_unique<std::promise<void>>();
    auto wait = done->get_future();

    std::atomic<bool>      executed{false};
    mc::runtime::any_executor executor(get_io_context().get_executor());

    executor.post([done = std::move(done), &executed]() mutable {
        executed.store(true, std::memory_order_release);
        done->set_value();
    });

    EXPECT_EQ(wait.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(executed.load(std::memory_order_acquire));
}

TEST_F(FuturesTest, RuntimeStrandAcceptsMoveOnlyTask)
{
    auto done = std::make_unique<std::promise<void>>();
    auto wait = done->get_future();

    std::atomic<bool>       executed{false};
    mc::runtime::runtime_strand strand;
    strand.bound_pool(&get_io_context());

    strand.post([done = std::move(done), &executed]() mutable {
        executed.store(true, std::memory_order_release);
        done->set_value();
    });

    EXPECT_EQ(wait.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(executed.load(std::memory_order_acquire));
}

// 测试 catch_error 捕获异常
TEST_F(FuturesTest, BasicErrorHandling)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future()
                      .then([](int) {
        throw std::runtime_error("测试异常");
    }).catch_error([](const mc::exception& e) {
        EXPECT_EQ(e.code(), mc::std_exception_code);
        EXPECT_STREQ(e.what(), "测试异常");
    });

    promise.set_value(0);
    EXPECT_NO_THROW(future.get());
}

// 测试 async 执行策略
TEST_F(FuturesTest, AsyncExecutionPolicy)
{
    auto promise    = mc::make_promise<int>(get_io_context());
    auto start_time = std::chrono::steady_clock::now();

    auto future = promise.get_future().then([](int value) {
        std::this_thread::sleep_for(100ms);
        return value * 2;
    }, mc::launch::async);

    promise.set_value(20);

    auto result  = future.get();
    auto elapsed = std::chrono::steady_clock::now() - start_time;

    EXPECT_EQ(result, 40);
    EXPECT_GE(elapsed, 100ms);
}

// 测试延迟执行
TEST_F(FuturesTest, DeferredExecutionPolicy)
{
    auto promise = mc::make_promise<int>(get_io_context());

    auto future = promise.get_future().then([](int value) {
        return value * 2;
    }, mc::launch::deferred);

    promise.set_value(20);
    EXPECT_EQ(future.get(), 40);
}

// 测试超时（同步）
TEST_F(FuturesTest, BasicTimeout)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    EXPECT_THROW(future.get_for(50ms), mc::timeout_exception);
}

// 测试链式调用，返回最后一个 future 的值
TEST_F(FuturesTest, SimpleChain)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future()
                      .then([](int value) {
        return value * 2;
    }).then([](int value) {
        return value + 1;
    });

    promise.set_value(20);
    EXPECT_EQ(future.get(), 41);
}

// 测试链式调用，允许在 then 中返回 future
TEST_F(FuturesTest, ChainWithFutureReturn)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future()
                      .then([&](int value) {
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
    EXPECT_EQ(future.get(), 41);
}

// 测试 catch_error 捕获异常
TEST_F(FuturesTest, CatchErrorWithException)
{
    int64_t code;
    auto    promise = mc::make_promise<int>(get_io_context());
    auto    future  = promise.get_future()
                      .then([](int) {
        throw std::runtime_error("测试异常");
        return 42;
    }).catch_error([&code](const mc::exception& e) {
        code = e.code();
        return -1;
    });

    promise.set_value(10);
    EXPECT_EQ(future.get(), -1);
    EXPECT_EQ(code, mc::std_exception_code);
}

// 测试 catch_error 无返回值
TEST_F(FuturesTest, CatchErrorWithoutException)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future()
                      .then([](int value) {
        return value * 2;
    })
                      .catch_error([](const mc::exception& e) {
        return -1;
    }).then([](int value) {
        EXPECT_EQ(value, 42);
    });

    promise.set_value(21);
    future.get();
}

// 测试 catch_error 无返回值
TEST_F(FuturesTest, CatchErrorVoidReturnWithException)
{
    auto promise = mc::make_promise<int>(get_io_context());
    bool caught  = false;

    auto future = promise.get_future()
                      .then([](int) -> void {
        throw std::runtime_error("测试异常");
    }).catch_error([&caught](const mc::exception&) {
        caught = true;
    });

    promise.set_value(42);
    future.get();
    EXPECT_TRUE(caught);
}

// 测试 catch_error 无返回值
TEST_F(FuturesTest, CatchErrorVoidReturnWithoutException)
{
    auto promise = mc::make_promise<int>(get_io_context());
    bool caught  = false;

    auto future = promise.get_future()
                      .then([](int) -> void {
        // 正常执行，无返回值
    }).catch_error([&caught](const mc::exception&) {
        caught = true;
    });

    promise.set_value(42);
    future.get();
    EXPECT_FALSE(caught);
}

// 测试 catch_error 恢复值
TEST_F(FuturesTest, ErrorRecoveryWithIntValue)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future()
                      .then([](int value) {
        if (value == 0) {
            throw std::runtime_error("值不能为0");
        }
        return value * 2;
    })
                      .catch_error([](const mc::exception&) {
        return 10; // 恢复默认值
    }).then([](int value) {
        return value + 5;
    });

    promise.set_value(0);        // 触发异常
    EXPECT_EQ(future.get(), 15); // 10 + 5
}

// 测试 catch_error 链式调用
TEST_F(FuturesTest, ErrorRecoveryChain)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future()
                      .then([](int value) {
        return value * 2; // 正常情况
    })
                      .catch_error([](const mc::exception&) {
        throw std::runtime_error("恢复失败");
        return -1;
    }).catch_error([](const mc::exception&) {
        return 99; // 最终恢复值
    });

    promise.set_value(5);
    EXPECT_EQ(future.get(), 10); // 5 * 2，没有触发异常
}

// 测试 promise::set_value 处理 Future 参数（成功和失败）
TEST_F(FuturesTest, PromiseSetValueFromFuture)
{
    auto outer_promise = mc::make_promise<int>(get_io_context());
    auto outer_future  = outer_promise.get_future();

    auto inner_promise = mc::make_promise<int>(get_io_context());
    auto inner_future  = inner_promise.get_future();

    // 传递 Future 给 set_value，验证成功场景
    outer_promise.template set_value<decltype(inner_future)>(std::move(inner_future));
    EXPECT_FALSE(outer_future.is_ready());

    inner_promise.set_value(123);
    EXPECT_EQ(outer_future.get(), 123);

    auto failing_promise = mc::make_promise<int>(get_io_context());
    auto failing_future  = failing_promise.get_future();

    auto inner_fail_promise = mc::make_promise<int>(get_io_context());
    auto inner_fail_future  = inner_fail_promise.get_future();

    // 传递 Future 给 set_value，验证异常传播
    failing_promise.template set_value<decltype(inner_fail_future)>(std::move(inner_fail_future));
    inner_fail_promise.set_exception(std::runtime_error("inner failure"));
    EXPECT_THROW(failing_future.get(), std::runtime_error);
}

TEST_F(FuturesTest, PromiseSetValueFromFutureCannotBeRebound)
{
    auto outer_promise = mc::make_promise<int>(get_io_context());
    auto outer_future  = outer_promise.get_future();

    auto inner_promise0 = mc::make_promise<int>(get_io_context());
    auto inner_future0  = inner_promise0.get_future();
    outer_promise.template set_value<decltype(inner_future0)>(std::move(inner_future0));

    auto inner_promise1 = mc::make_promise<int>(get_io_context());
    auto inner_future1  = inner_promise1.get_future();
    EXPECT_THROW(outer_promise.template set_value<decltype(inner_future1)>(std::move(inner_future1)),
                 mc::futures::promise_already_satisfied);
    EXPECT_THROW(outer_promise.set_value(1), mc::futures::promise_already_satisfied);
    EXPECT_THROW(outer_promise.set_exception(std::runtime_error("x")), mc::futures::promise_already_satisfied);

    inner_promise0.set_value(7);
    EXPECT_EQ(outer_future.get(), 7);
}

// 测试 promise::set_value 重复设置触发异常
TEST_F(FuturesTest, PromiseSetValueAlreadySatisfiedThrows)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    promise.set_value(42);
    EXPECT_THROW(promise.set_value(43), mc::futures::promise_already_satisfied);
    EXPECT_EQ(future.get(), 42);
}

// 测试 promise 只允许被设置一次值
TEST_F(FuturesTest, PromiseSetValueConcurrentOnlyOnce)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    std::atomic<bool> start{false};
    std::atomic<int>  success_count{0};
    std::atomic<int>  satisfied_count{0};
    std::atomic<int>  other_error_count{0};

    auto worker = [&](int value) {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        try {
            promise.set_value(value);
            success_count.fetch_add(1);
        } catch (const mc::futures::promise_already_satisfied&) {
            satisfied_count.fetch_add(1);
        } catch (...) {
            other_error_count.fetch_add(1);
        }
    };

    std::thread t1(worker, 1);
    std::thread t2(worker, 2);
    start.store(true, std::memory_order_release);
    t1.join();
    t2.join();

    EXPECT_EQ(other_error_count.load(), 0);
    EXPECT_EQ(success_count.load(), 1);
    EXPECT_EQ(satisfied_count.load(), 1);

    auto value = future.get();
    EXPECT_TRUE(value == 1 || value == 2);
}

// 测试 promise::set_exception 重复设置触发异常
TEST_F(FuturesTest, PromiseSetExceptionAlreadySatisfiedThrows)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    promise.set_exception(std::runtime_error("first"));
    EXPECT_THROW(promise.set_exception(std::runtime_error("second")), mc::futures::promise_already_satisfied);
    EXPECT_THROW(future.get(), std::runtime_error);
}

// 测试 promise 在取消后忽略 set_value
TEST_F(FuturesTest, PromiseSetValueIgnoredAfterCancel)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // 先取消 future
    future.cancel();
    // 取消后调用 set_value 应直接返回，不影响取消结果
    promise.set_value(42);
    EXPECT_THROW(future.get(), mc::canceled_exception);
}

// 测试 promise 在取消后忽略 set_exception
TEST_F(FuturesTest, PromiseSetExceptionIgnoredAfterCancel)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // 先取消 future
    future.cancel();
    // 取消后调用 set_exception 应直接返回，不影响取消结果
    promise.set_exception(std::runtime_error("ignored"));
    EXPECT_THROW(future.get(), mc::canceled_exception);
}

// 测试 promise::get_future 重复获取触发异常
TEST_F(FuturesTest, PromiseGetFutureTwiceThrows)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();
    EXPECT_THROW(promise.get_future(), mc::futures::future_already_retrieved);
    promise.set_value(7);
    EXPECT_EQ(future.get(), 7);
}

// 测试 Future::get_for 在结果已就绪时返回正常值
TEST_F(FuturesTest, FutureGetForReady)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    promise.set_value(11);
    EXPECT_EQ(future.get_for(50ms), 11);
}

// 测试 Future::wait_for 返回 ready 分支
TEST_F(FuturesTest, FutureWaitForReadyStatus)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    promise.set_value(22);
    EXPECT_EQ(future.wait_for(50ms), mc::futures::future_status::ready);
    EXPECT_EQ(future.get(), 22);
}

// 测试 catch_error 在 Future 已完成时直接返回原值
TEST_F(FuturesTest, FutureCatchErrorOnReadyValue)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();
    promise.set_value(33);

    std::atomic<int> handler_called{0};
    auto             recovered = future.catch_error([&handler_called](const mc::exception&) {
        handler_called.fetch_add(1);
        return -1;
    });

    EXPECT_EQ(recovered.get(), 33);
    EXPECT_EQ(handler_called.load(), 0);
}

namespace {
struct non_std_error {};
} // namespace

// 测试 catch_error 处理未知异常分支
TEST_F(FuturesTest, FutureCatchErrorHandlesUnknownException)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    promise.set_exception(non_std_error{});

    std::atomic<bool> handler_called{false};
    auto              recovered = future.catch_error([&handler_called](const mc::exception& ex) {
        // 确认异常被包装为 mc::exception，并且 handler 被调用
        handler_called.store(true);
        // 未知异常被包装后，至少应该有一个非零代码或有效的异常消息
        // 如果代码为 0，至少验证异常消息不为空
        if (ex.code() == 0) {
            // 验证异常消息不为空（未知异常应该被包装）
            EXPECT_FALSE(ex.what() == nullptr || mc::string(ex.what()).empty());
            return 1; // 返回非零值表示异常被正确处理
        }
        return static_cast<int>(ex.code());
    });
    // 等待 future 完成，确保 handler 被执行
    // catch_error 是异步的，需要调用 get() 来等待 handler 执行
    auto result = recovered.get();
    // 验证 handler 被调用（异常被正确捕获）
    EXPECT_TRUE(handler_called.load());
    // 验证返回值非零（表示异常被正确处理）
    EXPECT_NE(result, 0);
}

// 测试 then 在已就绪 Future 上使用 dispatch 策略
TEST_F(FuturesTest, FutureThenDispatchOnReadyState)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();
    promise.set_value(5);

    std::atomic<bool> executed{false};
    auto              chained = future.then([&executed](int value) {
        executed.store(true);
        return value + 1;
    }, mc::launch::dispatch);

    EXPECT_EQ(chained.get(), 6);
    EXPECT_TRUE(executed.load());
}

// 测试 then 在已就绪 Future 上使用 deferred 策略
TEST_F(FuturesTest, FutureThenDeferredOnReadyState)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();
    promise.set_value(8);

    auto deferred_future = future.then([](int value) {
        return value + 2;
    }, mc::launch::deferred);

    // deferred 策略下 wait_for 返回 deferred
    EXPECT_EQ(deferred_future.wait_for(1ms), mc::futures::future_status::deferred);

    // 取消后验证抛出取消异常，避免悬挂回调
    deferred_future.cancel();
    EXPECT_THROW(deferred_future.get(), mc::canceled_exception);
}

// 测试 async_get 在已就绪 Future 上使用 dispatch 策略
TEST_F(FuturesTest, FutureAsyncGetDispatchOnReadyState)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();
    promise.set_value(12);

    std::promise<int> result_promise;
    auto              result_future = result_promise.get_future();

    future.async_get([&result_promise](int value) {
        result_promise.set_value(value + 3);
    }, mc::launch::dispatch);

    EXPECT_EQ(result_future.get(), 15);
}

// 测试 finally 在已就绪 Future 上执行清理回调
TEST_F(FuturesTest, FutureFinallyOnReadyState)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();
    promise.set_value(20);

    std::atomic<bool> cleanup_called{false};
    auto              chained = future.finally([&cleanup_called]() {
        cleanup_called.store(true);
    });

    EXPECT_EQ(chained.get(), 20);
    EXPECT_TRUE(cleanup_called.load());
}

TEST_F(FuturesTest, FutureThenUsesExplicitExecutorOverride)
{
    mc::runtime::thread_pool override_pool(1, "future_override_then");
    override_pool.start();

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    std::atomic<bool> ran_on_override{false};
    auto chained = future.then([&ran_on_override, override_executor = override_pool.get_executor()](int value) {
        ran_on_override.store(override_executor.running_in_this_thread());
        return value + 1;
    }, mc::launch::dispatch, override_pool.get_executor());

    promise.set_value(20);

    EXPECT_EQ(chained.get(), 21);
    EXPECT_TRUE(ran_on_override.load());

    override_pool.stop();
    override_pool.join();
}

TEST_F(FuturesTest, FutureFinallyUsesExplicitExecutorOverride)
{
    mc::runtime::thread_pool override_pool(1, "future_override_finally");
    override_pool.start();

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    std::atomic<bool> cleanup_on_override{false};
    auto chained = future.finally([&cleanup_on_override, override_executor = override_pool.get_executor()]() {
        cleanup_on_override.store(override_executor.running_in_this_thread());
    }, mc::launch::dispatch, override_pool.get_executor());

    promise.set_value(20);

    EXPECT_EQ(chained.get(), 20);
    EXPECT_TRUE(cleanup_on_override.load());

    override_pool.stop();
    override_pool.join();
}

// 测试 tap 在已就绪 Future 上检查结果
TEST_F(FuturesTest, FutureTapOnReadyState)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();
    promise.set_value(30);

    std::atomic<int> observed{0};
    future
        .tap([&observed](int value) {
        observed.store(value);
    }).then([](auto&& result) {
        return result;
    });

    EXPECT_EQ(future.get(), 30);
    EXPECT_EQ(observed.load(), 30);
}

// 测试 tap_error
TEST_F(FuturesTest, FutureTapError)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();
    promise.set_exception(std::runtime_error("test exception"));

    std::atomic<int> observed{0};
    auto             tapped = future
                      .tap_error([&observed](const mc::exception& ex) {
        observed.store(ex.code());
    }).then([](auto&& result) {
        // 由于 tap_error 本身不会链接到 future 调用链，在 tap_error 后面
        // 增加一级调用链确保 tap_error 回调先于 result 的回调执行，保证
        // observed 被正确赋值
        return result;
    });

    EXPECT_THROW(tapped.get(), std::runtime_error);
    EXPECT_EQ(observed.load(), mc::std_exception_code);
}

// 测试 finally，在 future 完成时，调用 finally 函数
TEST_F(FuturesTest, FinallyWithSuccess)
{
    bool cleanup_called = false;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future()
                      .then([](int value) {
        return value * 2;
    }).finally([&cleanup_called]() {
        cleanup_called = true;
    });

    promise.set_value(10);
    EXPECT_EQ(future.get(), 20);
    EXPECT_TRUE(cleanup_called);
}

// 测试 finally，在 future 完成时，调用 finally 函数
TEST_F(FuturesTest, FinallyWithException)
{
    bool cleanup_called = false;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future()
                      .then([](int) {
        throw std::runtime_error("测试异常");
        return 42;
    }).finally([&cleanup_called]() {
        cleanup_called = true;
    });

    promise.set_value(10);

    EXPECT_THROW(future.get(), std::exception);
    EXPECT_TRUE(cleanup_called);
}

// 测试 tap，在 future 完成时，调用 tap 函数，但不会影响 future 的值
TEST_F(FuturesTest, TapWithSuccess)
{
    int observed_value = 0;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future()
                      .then([](int value) {
        return value * 2;
    })
                      .tap([&observed_value](int value) {
        observed_value = value;
    }).then([](auto&& result) {
        // 由于 tap 本身不会链接到 future 调用链，在 tap 后面
        // 增加一级调用链确保 tap 回调先于 result 的回调执行，保证
        // observed_value 被正确赋值
        return result;
    });

    promise.set_value(10);

    EXPECT_EQ(future.get(), 20);
    EXPECT_EQ(observed_value, 20);
}

// 测试 tap，在 future 完成时，调用 tap 函数，但不会影响 future 的值
TEST_F(FuturesTest, TapWithException)
{
    int observed_value = -1;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future()
                      .then([](int) {
        throw std::runtime_error("测试异常");
        return 42;
    })
                      .tap([&observed_value](int value) {
        observed_value = value; // 不应该被调用
    }).then([](auto&& result) {
        // 由于 tap 本身不会链接到 future 调用链，在 tap 后面
        // 增加一级调用链确保 tap 回调先于 result 的回调执行
        return result;
    });

    promise.set_value(10);

    EXPECT_THROW(future.get(), std::exception);
    EXPECT_EQ(observed_value, -1); // 没有被调用
}

// 测试 all，所有子 future 完成时，all 完成
TEST_F(FuturesTest, AllWithSuccess)
{
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

    auto all_future = mc::all(p1.get_future(), p2.get_future(), p3.get_future(),
                              mc::delay() // 添加一个 mc::delay() 在末尾验证 void 类型的 future 也可以被组合
    );

    p1.set_value(42);
    p2.set_value(3.14);
    p3.set_value("hello");

    // 顺带测试一下 any 的组合使用
    auto result = mc::any(mc::delay(1ms), mc::delay(1s))
                      .then([&all_future](auto&&) {
        return all_future.get();
    }).get();

    EXPECT_EQ(std::get<0>(result), 42);
    EXPECT_DOUBLE_EQ(std::get<1>(result), 3.14);
    EXPECT_EQ(std::get<2>(result), "hello");
    EXPECT_EQ(std::get<3>(result), std::monostate{});
}

// 任意一个子 future 抛出异常时，all 也会异常
TEST_F(FuturesTest, AllWithException)
{
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

    auto all_future = mc::all(p1.get_future(), p2.get_future(), p3.get_future());

    p1.set_value(42);
    p2.set_exception(std::runtime_error("测试异常"));
    p3.set_value("hello");

    EXPECT_THROW(all_future.get(), std::exception);
}

// 撤销 all 时，子 future 也会被撤销
TEST_F(FuturesTest, AllCancelPropagation)
{
    // 测试当all返回的future被取消时，子future也被取消
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

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
    EXPECT_THROW(f1.get(), mc::canceled_exception);
    EXPECT_THROW(f2.get(), mc::canceled_exception);
    EXPECT_THROW(f3.get(), mc::canceled_exception);

    // 验证所有子future都被取消
    EXPECT_TRUE(f1_canceled);
    EXPECT_TRUE(f2_canceled);
    EXPECT_TRUE(f3_canceled);
}

TEST_F(FuturesTest, ContainerAllWithSuccess)
{
    std::vector promises = {mc::make_promise<int>(get_io_context()), mc::make_promise<int>(get_io_context()),
                            mc::make_promise<int>(get_io_context())};

    std::vector<typename decltype(promises)::value_type::future_type> futures;
    futures.reserve(3);
    futures.emplace_back(promises[0].get_future());
    futures.emplace_back(promises[1].get_future());
    futures.emplace_back(promises[2].get_future());

    auto all_future = mc::all(futures.begin(), futures.end());

    std::vector<int> expected = {1, 2, 3};
    for (std::size_t i = 0; i < promises.size(); ++i) {
        promises[i].set_value(expected[i]);
    }

    EXPECT_EQ(all_future.get(), expected);
}

// 任意一个子 future 抛出异常时，all 也会异常
TEST_F(FuturesTest, ContainerAllWithException)
{
    std::vector promises = {mc::make_promise<int>(get_io_context()), mc::make_promise<int>(get_io_context()),
                            mc::make_promise<int>(get_io_context())};

    std::vector<typename decltype(promises)::value_type::future_type> futures;
    futures.reserve(3);
    futures.emplace_back(promises[0].get_future());
    futures.emplace_back(promises[1].get_future());
    futures.emplace_back(promises[2].get_future());

    auto all_future = mc::all(futures.begin(), futures.end());

    promises[0].set_value(1);
    promises[1].set_exception(std::runtime_error("测试异常"));

    EXPECT_THROW(all_future.get(), std::exception);
    EXPECT_EQ(futures[0].is_ready(), true);                 // 第一个完成
    EXPECT_EQ(futures[1].is_rejected(), true);              // 第二个异常
    EXPECT_THROW(futures[2].get(), mc::canceled_exception); // 第三个结果因为第二个异常而取消
}

// 任意一个子 future 取消，all 也会取消
TEST_F(FuturesTest, ContainerAllWithAnyFutureCancel)
{
    std::vector promises = {mc::make_promise<int>(get_io_context()), mc::make_promise<int>(get_io_context()),
                            mc::make_promise<int>(get_io_context())};

    std::vector<typename decltype(promises)::value_type::future_type> futures;
    futures.reserve(3);
    futures.emplace_back(promises[0].get_future());
    futures.emplace_back(promises[1].get_future());
    futures.emplace_back(promises[2].get_future());

    auto all_future = mc::all(futures.begin(), futures.end());

    promises[0].set_value(1);
    promises[1].cancel();
    promises[2].set_value(3);

    EXPECT_THROW(all_future.get(), mc::canceled_exception);
    EXPECT_EQ(futures[0].is_ready(), true);     // 第一个完成
    EXPECT_EQ(futures[1].is_cancelled(), true); // 第二个取消
    EXPECT_EQ(futures[2].is_cancelled(), true); // 第三个因为第二个取消而取消
}

TEST_F(FuturesTest, ContainerAllWithResultFutureCancel)
{
    std::vector promises = {mc::make_promise<int>(get_io_context()), mc::make_promise<int>(get_io_context()),
                            mc::make_promise<int>(get_io_context())};

    std::vector<typename decltype(promises)::value_type::future_type> futures;
    futures.reserve(3);
    futures.emplace_back(promises[0].get_future());
    futures.emplace_back(promises[1].get_future());
    futures.emplace_back(promises[2].get_future());

    auto all_future = mc::all(futures.begin(), futures.end());

    promises[0].set_value(1);
    all_future.cancel();
    promises[1].set_value(2);
    promises[2].set_value(3);

    EXPECT_THROW(all_future.get(), mc::canceled_exception);
    EXPECT_EQ(futures[0].is_ready(), true);     // 第一个完成
    EXPECT_EQ(futures[1].is_cancelled(), true); // 第二个因为结果future被取消而取消
    EXPECT_EQ(futures[2].is_cancelled(), true); // 第三个因为结果future被取消而取消
}

// 测试 any，任何一个 future 完成时，any 完成
TEST_F(FuturesTest, AnyWithSuccess)
{
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

    auto any_future = mc::any(p1.get_future(), p2.get_future(), p3.get_future());

    p2.set_value(3.14); // 第二个完成

    auto result = any_future.get();

    EXPECT_EQ(result.first, 1); // 索引 1（第二个）
    EXPECT_DOUBLE_EQ(std::get<double>(result.second), 3.14);
}

// 测试当any返回的future被取消时，子future也被取消
TEST_F(FuturesTest, AnyCancelPropagation)
{
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

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
    EXPECT_THROW(f1.get(), mc::canceled_exception);
    EXPECT_THROW(f2.get(), mc::canceled_exception);
    EXPECT_THROW(f3.get(), mc::canceled_exception);

    // 验证所有子future都被取消
    EXPECT_TRUE(f1_canceled);
    EXPECT_TRUE(f2_canceled);
    EXPECT_TRUE(f3_canceled);
}

// 测试当any的所有子future都被取消时，any也被取消
TEST_F(FuturesTest, AnyAllChildrenCanceled)
{
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

    auto any_future = mc::any(p1.get_future(), p2.get_future(), p3.get_future());

    // 取消所有子future
    p1.cancel();
    p2.cancel();
    p3.cancel();

    // 验证any_future被取消
    EXPECT_TRUE(any_future.is_rejected());
    try {
        any_future.get();
    } catch (const mc::exception& e) {
        EXPECT_EQ(e.code(), mc::canceled_exception_code);

        // 验证 any 会收集所有子 future 的异常消息
        EXPECT_EQ(e.messages().size(), 3);
    }
}

// 测试 resolve
TEST_F(FuturesTest, MakeReadyFuture)
{
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
TEST_F(FuturesTest, MakeExceptionalFuture)
{
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

// 测试为 future 添加超时
TEST_F(FuturesTest, TimeoutFunctionSuccess)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = mc::timeout(promise.get_future(), 100ms, get_io_context());

    promise.set_value(42);

    EXPECT_EQ(future.get(), 42);
}

// 测试为 future 添加超时
TEST_F(FuturesTest, TimeoutFunctionTimeout)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = mc::timeout(promise.get_future(), 50ms, get_io_context());

    // 不设置值，让其超时
    EXPECT_THROW(future.get(), mc::timeout_exception);
}

// 测试当 result_future 被取消时，内部定时器被正确取消
TEST_F(FuturesTest, TimeoutFunctionCancelOnResultFutureCancel)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = mc::timeout(promise.get_future(), 1000ms, get_io_context()); // 长超时

    // 取消 result_future，应该取消内部定时器
    future.cancel();

    EXPECT_THROW(future.get(), mc::canceled_exception);
    // 内部定时器应该被取消，不会产生额外的回调
}

// 测试当原始 future 在创建timeout前被取消时的行为
TEST_F(FuturesTest, TimeoutFunctionCancelOnOriginalPromiseCancel)
{
    auto promise         = mc::make_promise<int>(get_io_context());
    auto original_future = promise.get_future();

    // 先取消原始 future，然后创建 timeout future
    original_future.cancel();
    auto timeout_future = mc::timeout(std::move(original_future), 1000ms, get_io_context());

    // 原始 future 被取消，timeout_future 应该也抛出 canceled_exception
    EXPECT_THROW(timeout_future.get(), mc::canceled_exception);
}

// 测试当原始 promise 在创建timeout后被取消时的行为
TEST_F(FuturesTest, TimeoutFunctionCancelOnPromiseAfterTimeout)
{
    auto promise         = mc::make_promise<int>(get_io_context());
    auto original_future = promise.get_future();

    // 先创建 timeout future，然后取消原始 promise
    auto timeout_future = mc::timeout(std::move(original_future), 1000ms, get_io_context());
    promise.cancel(); // 通过promise取消，因为original_future已经被move到内部了

    // 原始 promise 被取消，timeout_future 应该也抛出 canceled_exception
    EXPECT_THROW(timeout_future.get(), mc::canceled_exception);
}

// 测试传入已经cancelled的future时的优化行为
TEST_F(FuturesTest, TimeoutFunctionWithAlreadyCancelledFuture)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // 先取消future
    future.cancel();

    // 此时future已经cancelled，timeout函数应该返回已取消的future
    auto timeout_future = mc::timeout(std::move(future), 1000ms, get_io_context());

    // 应该抛出取消异常
    EXPECT_THROW(timeout_future.get(), mc::canceled_exception);
}

// 测试使用私有 Boost timer 延时执行
TEST_F(FuturesTest, DelayedExecution)
{
    auto start_time = std::chrono::steady_clock::now();

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // 在 100ms 后设置值
    native_timer timer(mc::runtime::detail::thread_pool_impl::get_native_io_executor(get_io_context(), 0), 100ms);
    timer.async_wait([&promise](const boost::system::error_code&) {
        // 延时设置 promise 值
        promise.set_value(42);
    });

    EXPECT_EQ(future.get(), 42);

    auto elapsed = std::chrono::steady_clock::now() - start_time;
    EXPECT_GE(elapsed, 100ms);
}

// 测试使用 mc::delay 延时执行
TEST_F(FuturesTest, DelayedFuture)
{
    auto start_time = std::chrono::steady_clock::now();

    auto future = mc::delay(1ms, get_io_context()).then([]() {
        return 42;
    });

    EXPECT_EQ(future.get(), 42);

    auto elapsed = std::chrono::steady_clock::now() - start_time;
    EXPECT_GE(elapsed, 1ms);
}

// === 取消测试 ===

// 测试取消延时执行的 future
TEST_F(FuturesTest, CancelDelayedFuture)
{
    auto delayed_future = mc::delay(1000ms, get_io_context());
    delayed_future.cancel();
    EXPECT_THROW(delayed_future.get(), mc::canceled_exception);
}

// 测试取消延时执行的链式调用
TEST_F(FuturesTest, CancelChainedFuture)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().then([](int value) {
        return value * 2;
    });

    future.cancel();
    promise.set_value(10);
    EXPECT_THROW(future.get(), mc::canceled_exception);
}

// 测试取消延时执行的回调，并验证 cancel 回调是否被调用
TEST_F(FuturesTest, CancelCallbackExecution)
{
    bool callback_called = false;
    auto delayed_future  = mc::delay(100ms, get_io_context());

    delayed_future.on_cancel([&callback_called]() {
        callback_called = true;
    });

    delayed_future.cancel();
    EXPECT_THROW(delayed_future.get(), mc::canceled_exception);
    EXPECT_TRUE(callback_called);
}

// 测试嵌套调用 cancel 回调
TEST_F(FuturesTest, CancelCallbackNested)
{
    std::vector<int> call_order;
    auto             delayed_future = mc::delay(100ms, get_io_context());

    delayed_future.on_cancel([&]() {
        call_order.push_back(1);

        delayed_future.on_cancel([&call_order]() {
            call_order.push_back(2);
        });
    });

    delayed_future.cancel();
    EXPECT_THROW(delayed_future.get(), mc::canceled_exception);
    EXPECT_EQ(call_order.size(), 2);
    EXPECT_EQ(call_order[0], 1);
    EXPECT_EQ(call_order[1], 2);
}

TEST_F(FuturesTest, CanceledException)
{
    auto delayed_future = mc::delay(1000ms, get_io_context());
    delayed_future.cancel();
    EXPECT_THROW(delayed_future.get(), mc::canceled_exception);
}

TEST_F(FuturesTest, TimeoutException)
{
    auto promise        = mc::make_promise<int>(get_io_context());
    auto future         = promise.get_future();
    auto timeout_future = mc::timeout(std::move(future), 50ms, get_io_context());

    EXPECT_THROW(timeout_future.get(), mc::timeout_exception);
}

TEST_F(FuturesTest, ExceptionInfoPreservationInCatchError)
{
    mc::exception_ptr ex;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future()
                      .then([](int value) -> int {
        MC_THROW(mc::timeout_exception, "测试超时: 期望${expected}ms, 实际${actual}ms",
                 ("expected", 1000)("actual", 2000));
        return value;
    }).catch_error([&](const mc::exception& e) -> int {
        ex = e.dynamic_copy_exception();
        return static_cast<int>(e.code());
    });

    promise.set_value(42);

    auto result = future.get();
    EXPECT_EQ(ex->code(), mc::timeout_exception_code);
    EXPECT_STREQ(ex->what(), "测试超时: 期望1000ms, 实际2000ms");
    EXPECT_EQ(result, mc::timeout_exception_code);
}

TEST_F(FuturesTest, CancelExceptionHandling)
{
    auto promise      = mc::make_promise<int>(get_io_context());
    bool error_called = false;
    auto future       = promise.get_future()
                      .then([](int value) -> int {
        // mc::canceled_exception 是特殊异常，等效于直接调用 promise.cancel()，
        // 不会触发 catch_error 回调
        MC_THROW(mc::canceled_exception, "用户取消: ${reason}", ("reason", "手动停止"));
        return value;
    }).catch_error([&](const mc::exception& e) {
        error_called = true;
        return -1;
    });

    promise.set_value(42);

    EXPECT_THROW(future.get(), mc::canceled_exception);
    EXPECT_FALSE(error_called);
}

TEST_F(FuturesTest, StdExceptionHandling)
{
    mc::exception_ptr ex;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future()
                      .then([](int) -> int {
        throw std::invalid_argument("参数无效");
    }).catch_error([&](const mc::exception& e) -> int {
        ex = e.dynamic_copy_exception();
        return -2;
    });

    promise.set_value(42);

    auto result = future.get();
    EXPECT_TRUE(ex != nullptr);
    EXPECT_EQ(ex->code(), mc::std_exception_code);
    EXPECT_STREQ(ex->what(), "参数无效");
    EXPECT_EQ(result, -2);
}

TEST_F(FuturesTest, AnyFirstSuccessCancel)
{
    // 测试any中第一个成功时，其他future被取消
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

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

    auto result = any_future.get();
    EXPECT_EQ(result.first, 0); // 第一个future的索引
    EXPECT_EQ(std::get<int>(result.second), 42);

    EXPECT_THROW(f2.get(), mc::canceled_exception);
    EXPECT_THROW(f3.get(), mc::canceled_exception);

    // 验证其他future被取消
    EXPECT_TRUE(f2_canceled);
    EXPECT_TRUE(f3_canceled);
}

TEST_F(FuturesTest, AllOneChildCancelPropagation)
{
    // 测试all中一个子future被取消时，其他也被取消
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

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

    // all_future应该抛出取消异常
    EXPECT_THROW(all_future.get(), mc::canceled_exception);
    EXPECT_THROW(f3.get(), mc::canceled_exception);
    f1.get();
    f2.get();

    // f1、f2 在 p3 cancel 前已经 ready，所以不会被取消
    EXPECT_FALSE(f1_canceled);
    EXPECT_FALSE(f2_canceled);
}

TEST_F(FuturesTest, AnySuccessAfterSomeCanceled)
{
    // 测试any中部分future被取消，但仍有成功的情况
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

    auto f1 = p1.get_future();
    auto f2 = p2.get_future();
    auto f3 = p3.get_future();

    auto any_future = mc::any(std::move(f1), std::move(f2), std::move(f3));

    // 第一个和第三个被取消
    p1.cancel();
    p3.cancel();

    // 第二个成功
    p2.set_value(3.14);

    auto result = any_future.get();
    EXPECT_EQ(result.first, 1); // 第二个future的索引
    EXPECT_DOUBLE_EQ(std::get<double>(result.second), 3.14);
}

// 测试部分成功后异常的情况
TEST_F(FuturesTest, AllPartialSuccessThenException)
{
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

    auto all_future = mc::all(p1.get_future(), p2.get_future(), p3.get_future());

    // 前两个成功完成
    p1.set_value(42);
    p2.set_value(3.14);

    // 第三个抛出异常
    p3.set_exception(std::runtime_error("第三个失败"));

    EXPECT_THROW(all_future.get(), std::runtime_error);
}

// 测试多个异常同时发生时保留第一个异常
TEST_F(FuturesTest, AllMultipleSimultaneousExceptions)
{
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

    auto f1         = p1.get_future();
    auto f2         = p2.get_future();
    auto f3         = p3.get_future();
    auto all_future = mc::all(f1, f2, f3);

    // 同时设置多个异常
    p1.set_exception(std::runtime_error("第一个异常"));
    EXPECT_THROW(f2.get(), mc::canceled_exception);
    EXPECT_THROW(f3.get(), mc::canceled_exception);

    // 对于 all 来说，第一个异常意味着整体失败，其他异常会被忽略
    EXPECT_THROW(all_future.get(), std::runtime_error);
}

// 测试异常和取消混合的情况
TEST_F(FuturesTest, AllMixedExceptionAndCancel)
{
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

    auto f1 = p1.get_future();
    auto f2 = p2.get_future();
    auto f3 = p3.get_future();

    auto all_future = mc::all(std::move(f1), std::move(f2), std::move(f3));

    // 第一个取消
    p1.cancel();
    // 第二个异常
    p2.set_exception(std::runtime_error("第二个异常"));
    // 第三个尚未完成

    // 对于 all 来说，第一个取消意味着整体取消，其他异常会被忽略
    EXPECT_THROW(all_future.get(), mc::canceled_exception);
}

// 测试 deferred 执行策略
TEST_F(FuturesTest, AllWithDeferredExecution)
{
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());

    auto f1 = p1.get_future().then([](int v) {
        return v * 2;
    }, mc::launch::deferred);
    auto f2 = p2.get_future().then([](double v) {
        return v * 2;
    }, mc::launch::deferred);

    auto all_future = mc::all(std::move(f1), std::move(f2));

    p1.set_value(21);
    p2.set_value(1.57);

    auto result = all_future.get();
    EXPECT_EQ(std::get<0>(result), 42);
    EXPECT_DOUBLE_EQ(std::get<1>(result), 3.14);
}

// 测试部分异常后成功的情况
TEST_F(FuturesTest, AnySuccessAfterSomeExceptions)
{
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

    auto any_future = mc::any(p1.get_future(), p2.get_future(), p3.get_future());

    // 前两个失败
    p1.set_exception(std::runtime_error("第一个失败"));
    p2.set_exception(std::runtime_error("第二个失败"));

    // 第三个成功
    p3.set_value("success");

    // 任何一个成功都算整体成功
    auto result = any_future.get();
    EXPECT_EQ(result.first, 2); // 第三个的索引
    EXPECT_EQ(std::get<mc::string>(result.second), "success");
}

// 测试多种异常混合的情况
TEST_F(FuturesTest, AnyWithMixedExceptions)
{
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());
    auto p3 = mc::make_promise<mc::string>(get_io_context());

    auto f1         = p1.get_future();
    auto f2         = p2.get_future();
    auto f3         = p3.get_future();
    auto any_future = mc::any(f1, f2, f3);

    // 设置不同类型的异常
    p1.set_exception(std::runtime_error("运行时错误"));
    EXPECT_THROW(f1.get(), std::runtime_error);

    p2.set_exception(std::logic_error("逻辑错误"));
    EXPECT_THROW(f2.get(), std::logic_error);

    p3.set_exception(std::invalid_argument("参数错误"));
    EXPECT_THROW(f3.get(), std::invalid_argument);

    // any 整体失败后，返回最后一个异常
    // 由于有多个调度线程，最后一个不一定是 p3 的异常，所以这里验证有异常抛出即可
    EXPECT_THROW(any_future.get(), std::exception);
}

// 测试容器版本的 any
TEST_F(FuturesTest, ContainerAnyWithSuccess)
{
    std::vector promises = {mc::make_promise<int>(get_io_context()), mc::make_promise<int>(get_io_context()),
                            mc::make_promise<int>(get_io_context())};

    std::vector<typename decltype(promises)::value_type::future_type> futures;
    futures.reserve(3);
    for (auto& p : promises) {
        futures.emplace_back(p.get_future());
    }

    auto any_future = mc::any(futures.begin(), futures.end());

    // 第二个成功完成
    promises[1].set_value(42);

    auto result = any_future.get();
    EXPECT_EQ(result.first, 1); // 第二个的索引
    EXPECT_EQ(result.second, 42);
    EXPECT_THROW(futures[0].get(), mc::canceled_exception);
    EXPECT_THROW(futures[2].get(), mc::canceled_exception);
    EXPECT_TRUE(futures[0].is_cancelled()); // 任何一个成功都会取消其他未完成的 future
    EXPECT_TRUE(futures[2].is_cancelled()); // 任何一个成功都会取消其他未完成的 future
}

// 测试 deferred 执行策略
TEST_F(FuturesTest, AnyWithDeferredExecution)
{
    auto p1 = mc::make_promise<int>(get_io_context());
    auto p2 = mc::make_promise<double>(get_io_context());

    auto f1 = p1.get_future().then([](int v) {
        return v * 2;
    }, mc::launch::deferred);
    auto f2 = p2.get_future().then([](double v) {
        return v * 2;
    }, mc::launch::deferred);

    bool f1_canceled = false;
    f1.on_cancel([&]() {
        f1_canceled = true;
    });

    auto any_future = mc::any(std::move(f1), std::move(f2));

    p2.set_value(1.57); // 只设置第二个值

    auto result = any_future.get();
    EXPECT_EQ(result.first, 1); // 第二个的索引
    EXPECT_DOUBLE_EQ(std::get<double>(result.second), 3.14);

    EXPECT_THROW(f1.get(), mc::canceled_exception);

    // 验证其他future被取消
    EXPECT_TRUE(f1_canceled); // 任何一个成功都会取消其他未完成的 future
}

TEST_F(FuturesTest, CancelWithCacheError)
{
    bool catch_error_called = false;

    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future().catch_error([&](auto&&) {
        // 取消操作直接传播异常到整个调用链，不会触发 catch_error 回调
        catch_error_called = true;
        return 0;
    });

    // 取消操作
    promise.cancel();

    EXPECT_THROW(future.get(), mc::canceled_exception);
    EXPECT_FALSE(catch_error_called);
}

TEST_F(FuturesTest, OnCancelAfterReadyDoesNotRetainOtherState)
{
    auto& pool = mc::futures::state_pool::instance();
    pool.clear_all_pools();

    {
        auto promise_ready = mc::make_promise<int>(get_io_context());
        auto future_ready  = promise_ready.get_future();

        promise_ready.set_value(1);
        EXPECT_EQ(future_ready.get(), 1);

        {
            auto promise_other = mc::make_promise<int>(get_io_context());
            auto future_other  = promise_other.get_future();

            promise_other.set_value(2);
            EXPECT_EQ(future_other.get(), 2);

            future_ready.on_cancel(future_other);
        }

        // 因为其他测试用例可能残留未完成的 future，这会造成当前用例执行时 state_pool 回收
        // 残留的 future State，所以这里用大于等于而不是等于
        auto stats_mid = pool.get_stats();
        EXPECT_GE(stats_mid.total_global_states, 1);
    }

    auto stats_final = pool.get_stats();
    EXPECT_GE(stats_final.total_global_states, 2);
}

// 测试取消链式 future，会传递取消动作到所有 future
TEST_F(FuturesTest, CancelChainedNested)
{
    bool nested_canceled0 = false;
    bool nested_canceled1 = false;
    bool nested_canceled2 = false;
    bool nested_canceled3 = false;
    bool nested_canceled4 = false;
    bool nested_canceled5 = false;

    auto promise = mc::make_promise<int>(mc::runtime::immediate_executor());
    auto future  = promise.get_future()
                      .on_cancel([&]() {
        nested_canceled0 = true; // 外层: promise 操作返回的 future
    }).then([&](auto&& value) {
        return mc::delay(100ms)
            .on_cancel([&]() {
            nested_canceled1 = true; // 第一级: delay 操作返回的 future
        })
            .then([]() {
            return 42;
        })
            .on_cancel([&]() {
            nested_canceled2 = true; // 第二级: then 操作返回的 future
        })
            .catch_error([](auto&&) {
            return 42;
        })
            .on_cancel([&]() {
            nested_canceled3 = true; // 第三级: catch_error 操作返回的 future
        })
            .finally([]() {
            return 42;
        })
            .on_cancel([&]() {
            nested_canceled4 = true; // 第四级: finally 操作返回的 future
        })
            .tap([](auto&&) {
            return 42;
        }).on_cancel([&]() {
            nested_canceled5 = true; // 第五级: tap 操作返回的 future
        });
    });

    // 先求解第一个 future，触发创建嵌套的 future
    promise.set_value(1);

    // 取消外层 future，触发嵌套的 future 取消
    future.cancel();

    EXPECT_THROW(future.get(), mc::canceled_exception);
    EXPECT_FALSE(nested_canceled0); // 外层已经求解，不会触发 cancel 回调
    EXPECT_TRUE(nested_canceled1);
    EXPECT_TRUE(nested_canceled2);
    EXPECT_TRUE(nested_canceled3);
    EXPECT_TRUE(nested_canceled4);
    EXPECT_TRUE(nested_canceled5);
}

// 测试取消内部 future，会传递取消动作到外部 future
TEST_F(FuturesTest, CancelInnerFuture)
{
    bool inner_canceled1 = false;
    bool inner_canceled2 = false;

    auto promise = mc::make_promise<int>(mc::runtime::immediate_executor());
    auto future  = promise.get_future().then([&](auto&&) {
        auto outer_future = mc::delay(1000ms);

        // 创建一个内部定时器，在 10ms 后取消外部定时器
        // 故意用 runtime 的 work 上下文而不是 io 上下文，测试 future 可以跨上下文运行
        mc::delay(10ms, get_runtime().work()).then([outer_future]() mutable {
            outer_future.cancel();
        });

        return outer_future
            .on_cancel([&]() {
            inner_canceled1 = true;
        })
            .then([](auto&&) {
            return 42;
        }).on_cancel([&]() {
            inner_canceled2 = true;
        });
    });

    promise.set_value(1);

    EXPECT_THROW(future.get(), mc::canceled_exception);
    EXPECT_TRUE(inner_canceled1);
    EXPECT_TRUE(inner_canceled2);
}

// 测试 callback_list 和 callback_pool 的功能
TEST_F(FuturesTest, CallbackListBasicOperations)
{
    mc::futures::callback_list list;

    // 测试 empty() 方法
    EXPECT_TRUE(list.empty());

    // 测试 push_back
    int counter = 0;
    list.push_back([&counter]() {
        counter++;
    });
    EXPECT_FALSE(list.empty());

    list.push_back([&counter]() {
        counter += 2;
    });

    // 测试 execute_and_clear
    list.execute_and_clear();
    EXPECT_EQ(counter, 3);
    EXPECT_TRUE(list.empty());
}

// 测试 callback_list 的 clear 方法
TEST_F(FuturesTest, CallbackListClear)
{
    mc::futures::callback_list list;

    int counter = 0;
    list.push_back([&counter]() {
        counter++;
    });
    list.push_back([&counter]() {
        counter++;
    });

    EXPECT_FALSE(list.empty());

    // 测试 clear 方法（不执行回调）
    list.clear();
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(counter, 0); // 回调不应该被执行
}

// 测试 callback_list 的 swap 方法
TEST_F(FuturesTest, CallbackListSwap)
{
    mc::futures::callback_list list1;
    mc::futures::callback_list list2;

    int counter1 = 0;
    int counter2 = 0;

    list1.push_back([&counter1]() {
        counter1 = 1;
    });
    list2.push_back([&counter2]() {
        counter2 = 2;
    });

    // 交换两个列表
    list1.swap(list2);

    // 执行并验证
    list1.execute_and_clear();
    EXPECT_EQ(counter1, 0);
    EXPECT_EQ(counter2, 2);

    list2.execute_and_clear();
    EXPECT_EQ(counter1, 1);
    EXPECT_EQ(counter2, 2);
}

// 使用独立的 callback_pool 单例进行测试，避免其他测试用例影响
struct callback_pool_tag {};
auto& get_test_callback_pool()
{
    return mc::futures::callback_pool::instance(callback_pool_tag{});
}

class test_callback_list {
public:
    void push_back(std::function<void()> callback)
    {
        auto node = get_test_callback_pool().acquire_node(std::move(callback));
        m_callbacks.emplace_back(std::move(node));
    }

    void execute_and_clear()
    {
        while (!m_callbacks.empty()) {
            auto node = std::move(m_callbacks.front());
            m_callbacks.pop_front();

            mc::futures::safe_invoke(std::move(node->m_callback));
            get_test_callback_pool().release_node(std::move(node));
        }
    }

private:
    std::list<mc::futures::callback_node_ptr> m_callbacks;
};

// 测试 callback_pool 的 get_stats 方法
TEST_F(FuturesTest, CallbackPoolGetStats)
{
    auto& pool = get_test_callback_pool();
    pool.clear();

    // 获取初始统计信息
    auto stats = pool.get_stats();
    EXPECT_GE(stats.pool_size, 0U);
    EXPECT_GT(stats.max_size, 0U);

    // 使用一些节点后再次获取统计信息
    test_callback_list list;
    list.push_back([]() {
    });
    list.push_back([]() {
    });
    list.execute_and_clear();

    auto stats_after = pool.get_stats();
    EXPECT_GE(stats_after.pool_size, stats.pool_size);
}

// 测试 callback_pool 的 set_max_pool_size 方法
TEST_F(FuturesTest, CallbackPoolSetMaxPoolSize)
{
    auto& pool = get_test_callback_pool();

    // 设置较小的最大池大小
    pool.set_max_pool_size(5);

    // 创建多个节点并释放，使池达到最大大小
    test_callback_list list;
    for (int i = 0; i < 10; ++i) {
        list.push_back([]() {
        });
    }
    list.execute_and_clear();

    // 验证池大小不超过最大值
    auto stats = pool.get_stats();
    EXPECT_LE(stats.pool_size, 5U);
    EXPECT_EQ(stats.max_size, 5U);

    // 测试设置更大的最大池大小，并验证池会保留现有节点
    pool.set_max_pool_size(20);
    stats = pool.get_stats();
    EXPECT_LE(stats.pool_size, 20U);
    EXPECT_EQ(stats.max_size, 20U);

    // 测试设置更小的最大池大小，验证多余的节点会被释放
    pool.set_max_pool_size(3);
    stats = pool.get_stats();
    EXPECT_LE(stats.pool_size, 3U);
    EXPECT_EQ(stats.max_size, 3U);
}

// 测试 callback_pool 的 clear 方法
TEST_F(FuturesTest, CallbackPoolClear)
{
    auto& pool = get_test_callback_pool();

    // 创建一些节点并释放到池中
    test_callback_list list;
    for (int i = 0; i < 5; ++i) {
        list.push_back([]() {
        });
    }
    list.execute_and_clear();

    // 验证池中有节点
    auto stats_before = pool.get_stats();
    EXPECT_GT(stats_before.pool_size, 0U);

    // 清空池
    pool.clear();

    // 验证池已清空
    auto stats_after = pool.get_stats();
    EXPECT_EQ(stats_after.pool_size, 0U);
}

// 测试 callback_pool::release_node 中 node 为 nullptr 的分支
TEST_F(FuturesTest, CallbackPoolReleaseNullptr)
{
    auto& pool = get_test_callback_pool();

    // 释放 nullptr 节点应该安全返回
    std::unique_ptr<mc::futures::callback_node> null_node(nullptr);
    pool.release_node(std::move(null_node));

    // 验证没有崩溃
    auto stats = pool.get_stats();
    EXPECT_GE(stats.pool_size, 0U);
}

// 测试 callback_pool::release_node 中池大小达到最大值的分支
TEST_F(FuturesTest, CallbackPoolReleaseNodeMaxSize)
{
    auto& pool = get_test_callback_pool();

    // 设置较小的最大池大小
    pool.set_max_pool_size(2);
    pool.clear(); // 先清空池

    // 创建并释放节点，使池达到最大值
    test_callback_list list;
    for (int i = 0; i < 3; ++i) {
        list.push_back([]() {
        });
    }
    list.execute_and_clear();

    // 验证池大小不超过最大值
    auto stats = pool.get_stats();
    EXPECT_LE(stats.pool_size, 2U);

    // 再次释放节点，应该因为池已满而不被接受
    test_callback_list list2;
    list2.push_back([]() {
    });
    list2.execute_and_clear();

    // 池大小应该仍然不超过最大值
    stats = pool.get_stats();
    EXPECT_LE(stats.pool_size, 2U);
}

// 测试 callback_pool::acquire_node 复用池中节点的分支
TEST_F(FuturesTest, CallbackPoolAcquireReuseNode)
{
    auto& pool = get_test_callback_pool();

    // 重置池状态并设置较小的最大容量，便于观察变化
    pool.clear();
    pool.set_max_pool_size(10);

    // 首次 push_back 会创建新的节点
    test_callback_list list;
    list.push_back([]() {
    });
    list.execute_and_clear();

    // 节点释放后应被缓存到池中
    auto stats_after_release = pool.get_stats();
    EXPECT_EQ(stats_after_release.pool_size, 1U);
    EXPECT_EQ(stats_after_release.max_size, 10U);

    // 再次 push_back 应从池中复用节点，池大小减为 0
    list.push_back([]() {
    });
    auto stats_after_acquire = pool.get_stats();
    EXPECT_EQ(stats_after_acquire.pool_size, 0U);
    EXPECT_EQ(stats_after_acquire.max_size, 10U);

    // 再次执行并释放，节点应回到池中
    list.execute_and_clear();
    auto stats_after_reuse = pool.get_stats();
    EXPECT_EQ(stats_after_reuse.pool_size, 1U);
    EXPECT_EQ(stats_after_reuse.max_size, 10U);

    // 恢复默认设置，避免影响其他测试
    pool.set_max_pool_size(1000);
    pool.clear();
}

// 测试 safe_invoke 的异常处理分支
TEST_F(FuturesTest, SafeInvokeExceptionHandling)
{
    bool exception_thrown = false;

    // 测试抛出异常的 callback
    mc::futures::safe_invoke([&exception_thrown]() {
        exception_thrown = true;
        throw std::runtime_error("测试异常");
    });

    // 异常应该被捕获，不会传播
    EXPECT_TRUE(exception_thrown);

    // 测试正常执行的 callback
    int counter = 0;
    mc::futures::safe_invoke([&counter]() {
        counter = 42;
    });
    EXPECT_EQ(counter, 42);
}

// 测试 callback_list 的移动语义
TEST_F(FuturesTest, CallbackListMoveSemantics)
{
    mc::futures::callback_list list1;

    int counter = 0;
    list1.push_back([&counter]() {
        counter = 1;
    });

    // 测试移动构造
    mc::futures::callback_list list2(std::move(list1));
    EXPECT_TRUE(list1.empty());
    EXPECT_FALSE(list2.empty());

    list2.execute_and_clear();
    EXPECT_EQ(counter, 1);

    // 测试移动赋值
    mc::futures::callback_list list3;
    list3.push_back([&counter]() {
        counter = 2;
    });

    list1 = std::move(list3);
    EXPECT_TRUE(list3.empty());
    EXPECT_FALSE(list1.empty());

    list1.execute_and_clear();
    EXPECT_EQ(counter, 2);
}

// 测试 catch_error 从嵌套 future 中捕获异常
TEST_F(FuturesTest, CatchErrorFromNestedFuture)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // then 返回一个会立即抛异常的 future
    auto nested_future = future.then([this](int) {
        // 返回一个会立即抛异常的 future
        auto inner_promise = mc::make_promise<int>(get_io_context());
        auto inner_future  = inner_promise.get_future();
        inner_promise.set_exception(std::runtime_error("nested error"));
        return inner_future;
    });

    // 后续 catch_error 确认捕获异常并恢复默认值
    auto recovered = nested_future.catch_error([](const mc::exception&) {
        return 100; // 恢复默认值
    });

    promise.set_value(42);

    // 验证 catch_error 捕获了嵌套 future 的异常并返回了默认值
    EXPECT_EQ(recovered.get(), 100);
}

// 测试异常类的拷贝/移动/动态方法
TEST_F(FuturesTest, FuturesExceptionCopyAndRethrow)
{
    // 手动构造 future_already_retrieved 异常
    {
        // 使用 MC_MAKE_EXCEPTION 构造异常
        auto ex1 = MC_MAKE_EXCEPTION(mc::futures::future_already_retrieved, "test message");

        // 测试拷贝构造
        mc::futures::future_already_retrieved ex2(ex1);
        EXPECT_STREQ(ex1.what(), ex2.what());

        // 测试移动构造
        mc::futures::future_already_retrieved ex3(std::move(ex2));
        EXPECT_STREQ(ex1.what(), ex3.what());

        // 注意：由于异常类声明了移动构造函数，拷贝赋值运算符被隐式删除
        // 我们通过拷贝构造来测试拷贝语义
        mc::futures::future_already_retrieved ex4(ex1);
        EXPECT_STREQ(ex1.what(), ex4.what());

        // 注意：移动赋值运算符也被隐式删除
        // 我们通过移动构造来测试移动语义
        mc::futures::future_already_retrieved ex5(std::move(ex1));
        EXPECT_STREQ(ex3.what(), ex5.what());

        // 测试 dynamic_copy_exception
        auto copied = ex5.dynamic_copy_exception();
        ASSERT_NE(copied, nullptr);
        EXPECT_STREQ(ex5.what(), copied->what());

        // 测试 dynamic_rethrow_exception
        try {
            ex5.dynamic_rethrow_exception();
            FAIL() << "应该抛出异常";
        } catch (const mc::futures::future_already_retrieved& e) {
            EXPECT_STREQ(ex5.what(), e.what());
        }
    }

    // 手动构造 promise_already_satisfied 异常
    {
        // 使用 MC_MAKE_EXCEPTION 构造异常
        auto ex1 = MC_MAKE_EXCEPTION(mc::futures::promise_already_satisfied, "test message");

        // 测试拷贝构造
        mc::futures::promise_already_satisfied ex2(ex1);
        EXPECT_STREQ(ex1.what(), ex2.what());

        // 测试移动构造
        mc::futures::promise_already_satisfied ex3(std::move(ex2));
        EXPECT_STREQ(ex1.what(), ex3.what());

        // 注意：由于异常类声明了移动构造函数，拷贝赋值运算符被隐式删除
        // 我们通过拷贝构造来测试拷贝语义
        mc::futures::promise_already_satisfied ex4(ex1);
        EXPECT_STREQ(ex1.what(), ex4.what());

        // 注意：移动赋值运算符也被隐式删除
        // 我们通过移动构造来测试移动语义
        mc::futures::promise_already_satisfied ex5(std::move(ex1));
        EXPECT_STREQ(ex3.what(), ex5.what());

        // 测试 dynamic_copy_exception
        auto copied = ex5.dynamic_copy_exception();
        ASSERT_NE(copied, nullptr);
        EXPECT_STREQ(ex5.what(), copied->what());

        // 测试 dynamic_rethrow_exception
        try {
            ex5.dynamic_rethrow_exception();
            FAIL() << "应该抛出异常";
        } catch (const mc::futures::promise_already_satisfied& e) {
            EXPECT_STREQ(ex5.what(), e.what());
        }
    }
}

// 压力测试：临时 future 链在高频创建/销毁下 future 链生命周期是否正确
TEST_F(FuturesTest, stress_temporary_then_chain)
{
    for (int i = 0; i < 500; ++i) {
        auto p = mc::make_promise<int>(get_io_context());

        // 创建一条包含临时 future 的链：外层 then 返回内层 future，随后在 catch_error 中收敛为值
        auto result_future = p.get_future()
                                 .then([this](int v) {
            // 返回一个临时 future，立即就绪
            auto inner_p = mc::make_promise<int>(get_io_context());
            auto inner_f = inner_p.get_future();
            inner_p.set_value(v + 1);
            return inner_f; // 中间层 future 为临时对象
        })
                                 .catch_error([](const mc::exception&) {
            return -1; // 异常时返回默认值
        }).then([](int v) {
            return v * 2;
        }, mc::launch::dispatch);

        // 触发链条执行，并在 result_future 上同步获取结果
        p.set_value(1);
        EXPECT_EQ(result_future.get(), 4);
    }
}

// 压力测试：all 结合内联临时 then，不持有外层 future 的情况下 future 链生命周期是否正确
TEST_F(FuturesTest, stress_all_with_inline_temporaries)
{
    for (int i = 0; i < 300; ++i) {
        auto p1 = mc::make_promise<int>(get_io_context());
        auto p2 = mc::make_promise<double>(get_io_context());
        auto p3 = mc::make_promise<mc::string>(get_io_context());

        auto f = mc::all(p1.get_future().then([](int v) {
            return v + 1;
        }),
                         p2.get_future().then([](double v) {
            return v + 1.0;
        }),
                         p3.get_future().then([](mc::string v) {
            return v + "!";
        }));

        p1.set_value(1);
        p2.set_value(2.0);
        p3.set_value("ok");

        auto r = f.get();
        EXPECT_EQ(std::get<0>(r), 2);
        EXPECT_DOUBLE_EQ(std::get<1>(r), 3.0);
        EXPECT_EQ(std::get<2>(r), "ok!");
    }
}

// 压力测试：any 结合内联临时 then，不持有外层 future 的情况下 future 链生命周期是否正确
TEST_F(FuturesTest, stress_any_with_inline_temporaries)
{
    for (int i = 0; i < 300; ++i) {
        auto p1 = mc::make_promise<int>(get_io_context());
        auto p2 = mc::make_promise<double>(get_io_context());

        // 第二个先完成，验证其他临时 future 取消路径
        auto f = mc::any(p1.get_future().then([](int v) {
            return v + 1;
        }),
                         p2.get_future().then([](double v) {
            return v + 1.0;
        }));

        p2.set_value(1.0);

        auto r = f.get();
        EXPECT_EQ(r.first, 1);
        EXPECT_DOUBLE_EQ(std::get<double>(r.second), 2.0);
    }
}

// 测试 as_future 相同类型转换（应该直接返回，不进行转换）
TEST_F(FuturesTest, AsFutureSameType)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // 转换为相同类型，应该直接返回
    auto converted = future.as_future<int>();
    promise.set_value(42);

    EXPECT_EQ(converted.get(), 42);
    // 原 future 应该已经被移动，不再有效
    EXPECT_FALSE(future.valid());
}

// 测试 as_future 转换为 void
TEST_F(FuturesTest, AsFutureToVoid)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // 从 int 转换为 void
    auto void_future = future.as_future<void>();
    promise.set_value(42);

    // void future 的 get() 应该正常返回
    EXPECT_NO_THROW(void_future.get());
    EXPECT_TRUE(void_future.is_ready());
}

// 测试 as_future 从 void 转换为其他类型
TEST_F(FuturesTest, AsFutureFromVoid)
{
    auto promise = mc::make_promise<void>(get_io_context());
    auto future  = promise.get_future();

    // 从 void 转换为 int，应该使用默认构造
    auto int_future = future.as_future<int>();
    promise.set_value();

    // 应该得到默认构造的 int 值（0）
    EXPECT_EQ(int_future.get(), 0);
}

// 测试 as_future 转换为 std::monostate
TEST_F(FuturesTest, AsFutureToMonostate)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // 从 int 转换为 std::monostate，这会被转换成 Future<void> 类型
    auto void_future = future.as_future<std::monostate>();
    promise.set_value(42);

    EXPECT_NO_THROW(void_future.get());
}

// 测试 as_future 不同类型之间的转换（int 转 double）
TEST_F(FuturesTest, AsFutureIntToDouble)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // 从 int 转换为 double
    auto double_future = future.as_future<double>();
    promise.set_value(42);

    // 应该得到转换后的 double 值
    EXPECT_DOUBLE_EQ(double_future.get(), 42.0);
}

// 测试 as_future 不同类型之间的转换（double 转 int）
TEST_F(FuturesTest, AsFutureDoubleToInt)
{
    auto promise = mc::make_promise<double>(get_io_context());
    auto future  = promise.get_future();

    // 从 double 转换为 int
    auto int_future = future.as_future<int>();
    promise.set_value(3.14);

    // 应该得到转换后的 int 值（截断）
    EXPECT_EQ(int_future.get(), 3);
}

// 测试 as_future 不同类型之间的转换（long 转 int）
TEST_F(FuturesTest, AsFutureLongToInt)
{
    auto promise = mc::make_promise<long>(get_io_context());
    auto future  = promise.get_future();

    // 从 long 转换为 int
    auto int_future = future.as_future<int>();
    promise.set_value(123456L);

    // 应该得到转换后的 int 值
    EXPECT_EQ(int_future.get(), 123456);
}

// 测试 as_future 链式转换
TEST_F(FuturesTest, AsFutureChainedConversion)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // 链式转换：int -> double -> void
    auto void_future = future.as_future<double>().as_future<void>();
    promise.set_value(42);

    EXPECT_NO_THROW(void_future.get());
    EXPECT_TRUE(void_future.is_ready());
}

// 测试 as_future 在异常情况下的行为
TEST_F(FuturesTest, AsFutureWithException)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // 转换为 double
    auto double_future = future.as_future<double>();
    promise.set_exception(std::runtime_error("测试异常"));

    // 异常应该传播到转换后的 future
    EXPECT_THROW(double_future.get(), std::runtime_error);
}

// 测试 as_future 使用自定义执行器
TEST_F(FuturesTest, AsFutureWithCustomExecutor)
{
    auto promise = mc::make_promise<int>(get_io_context());
    auto future  = promise.get_future();

    // 使用自定义执行器进行转换
    auto void_future = future.as_future<void>(get_io_context().get_executor());
    promise.set_value(42);

    EXPECT_NO_THROW(void_future.get());
    EXPECT_TRUE(void_future.is_ready());
}

// 测试空状态（默认构造）的 future 行为
TEST_F(FuturesTest, test_default_constructed_empty_state)
{
    // 测试默认构造的 future（空状态）
    mc::future<int> empty_future;
    EXPECT_FALSE(empty_future.valid());
    EXPECT_FALSE(empty_future.is_ready());
    EXPECT_FALSE(empty_future.is_cancelled());
    EXPECT_FALSE(empty_future.is_rejected());

    // 空状态下调用 get() 应该抛出异常
    EXPECT_THROW(empty_future.get(), mc::futures::invalid_future_exception);

    // 空状态下调用 get_exception() 应该返回 nullptr
    auto ex = empty_future.get_exception();
    EXPECT_EQ(ex, nullptr);

    // 空状态下调用 wait() 应该安全返回
    EXPECT_NO_THROW(empty_future.wait());

    // 空状态下调用 wait_for() 应该返回 invalid
    auto status = empty_future.wait_for(100ms);
    EXPECT_EQ(status, mc::future_status::invalid);

    // 空状态下调用 cancel() 应该安全返回
    EXPECT_NO_THROW(empty_future.cancel());
}

// 测试空状态的链式操作
TEST_F(FuturesTest, test_empty_state_chaining)
{
    mc::future<int> empty_future;

    // 空状态下的 then 操作会返回一个 rejected 的 future
    auto chained = empty_future.then([](int v) {
        return v * 2;
    });

    // chained 的 future 是一个 rejected 的 future
    EXPECT_TRUE(chained.is_rejected());

    // 空状态下的 catch_error 操作会创建一个包含 invalid_future_exception 的 rejected future，
    // 然后调用 catch_error 回调，并返回一个有效的 future
    std::atomic<bool>    catch_error_called{false};
    std::atomic<int64_t> exception_code{0};
    auto recovered = empty_future.catch_error([&catch_error_called, &exception_code](const mc::exception& ex) {
        catch_error_called.store(true);
        // 验证异常代码是 invalid_future_code
        exception_code.store(ex.code());
        EXPECT_EQ(ex.code(), mc::futures::invalid_future_code);
        return 42;
    });

    // recovered 的 future 应该是有效的，并且包含值 42
    EXPECT_TRUE(recovered.valid());
    EXPECT_EQ(recovered.get(), 42);
    EXPECT_TRUE(catch_error_called.load());
    EXPECT_EQ(exception_code.load(), mc::futures::invalid_future_code);

    // 空状态下的 finally 操作会创建一个包含 invalid_future_exception 的 rejected future，
    // 然后调用 finally 回调
    std::atomic<bool> finally_called{false};
    auto              final_result = empty_future.finally([&finally_called]() {
        finally_called.store(true);
    });

    // final_result 的 future 应该是 rejected 状态（包含 invalid_future_exception）
    EXPECT_TRUE(final_result.is_rejected());
    EXPECT_THROW(final_result.get(), mc::futures::invalid_future_exception);
    EXPECT_TRUE(finally_called.load());
}

// 测试空状态的移动操作
TEST_F(FuturesTest, test_empty_state_move_operations)
{
    mc::future<int> empty_future1;
    mc::future<int> empty_future2(std::move(empty_future1));

    // 移动后，empty_future1 应该还是空状态
    EXPECT_FALSE(empty_future1.valid());

    // empty_future2 也应该是空状态
    EXPECT_FALSE(empty_future2.valid());

    // 移动赋值
    mc::future<int> empty_future3;
    empty_future3 = std::move(empty_future2);

    // 都应该是空状态
    EXPECT_FALSE(empty_future2.valid());
    EXPECT_FALSE(empty_future3.valid());
}

// 测试空状态转换为其他类型
TEST_F(FuturesTest, test_empty_state_type_conversion)
{
    mc::future<int> empty_future;

    // 空状态下转换为其他类型的 future 会调用 then，而 then 在空状态下会返回一个 rejected 的 future
    auto converted_void = empty_future.as_future<void>();
    EXPECT_TRUE(converted_void.valid());
    EXPECT_TRUE(converted_void.is_rejected());
    EXPECT_THROW(converted_void.get(), mc::futures::invalid_future_exception);

    auto converted_double = empty_future.as_future<double>();
    EXPECT_TRUE(converted_double.valid());
    EXPECT_TRUE(converted_double.is_rejected());
    EXPECT_THROW(converted_double.get(), mc::futures::invalid_future_exception);
}
