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

// launch::deferred 真延迟模式专项测试。覆盖：lazy start、各拉取触发点、CAS 唯一启动、
// 完成时通过 executor 异步唤醒、worker 线程上 self-pull 通过嵌套驱动取值（不死锁）、
// cancel 与 task 启动的交互、高阶工厂 make_deferred_future 的值/void/异常路径，
// 以及 install 但未拉取场景下 ctx_dtor 的释放。

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include <mc/future.h>
#include <mc/futures/exceptions.h>
#include <mc/runtime/executor.h>
#include <test_utilities/base.h>

using namespace std::chrono_literals;

namespace {

class DeferredFuturesTest : public mc::test::TestWithRuntime {
public:
    mc::runtime::thread_pool& get_pool()
    {
        return get_runtime().io();
    }

    mc::runtime::any_executor get_pool_executor()
    {
        return get_pool().get_executor();
    }
};

// install 后任务体不应被立即派发，第一次拉取才启动。
TEST_F(DeferredFuturesTest, LazyStartDoesNotRunUntilPulled)
{
    auto promise = mc::make_promise<int>(get_pool_executor());
    promise.set_policy(mc::futures::launch::deferred);
    auto future = promise.get_future();

    std::atomic<int> ran_count{0};

    struct holder {
        mc::futures::Promise<int> p;
        std::atomic<int>*         counter;
    };
    auto* h = new holder{promise, &ran_count};

    future.get_state()->install_deferred_task(
        +[](void* ctx) noexcept {
            auto* hh = static_cast<holder*>(ctx);
            hh->counter->fetch_add(1, std::memory_order_acq_rel);
            hh->p.set_value(42);
        },
        h,
        +[](void* ctx) noexcept { delete static_cast<holder*>(ctx); });

    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(ran_count.load(), 0);
    EXPECT_FALSE(future.is_ready());
    EXPECT_FALSE(future.get_state()->is_deferred_task_started());

    EXPECT_EQ(future.get<int>(), 42);
    EXPECT_EQ(ran_count.load(), 1);
    EXPECT_TRUE(future.get_state()->is_deferred_task_started());
}

// wait_for 与 std::future 一致——任务未启动时返回 future_status::deferred 不触发。
TEST_F(DeferredFuturesTest, WaitForReturnsDeferredStatusWithoutStarting)
{
    std::atomic<int> ran{0};
    auto             future = mc::make_deferred_future<int>(get_pool_executor(), [&ran]() {
        ran.fetch_add(1, std::memory_order_acq_rel);
        return 7;
    });

    EXPECT_EQ(future.wait_for(50ms), mc::future_status::deferred);
    EXPECT_EQ(ran.load(), 0);
    EXPECT_FALSE(future.is_ready());

    future.wait();
    EXPECT_EQ(ran.load(), 1);
    EXPECT_EQ(future.get<int>(), 7);
}

// 任务启动后 wait_for 退化为正常等待路径。
TEST_F(DeferredFuturesTest, WaitForAfterStartedBlocksUntilReady)
{
    auto future = mc::make_deferred_future<int>(get_pool_executor(), []() {
        std::this_thread::sleep_for(20ms);
        return 7;
    });

    auto done_promise = mc::make_promise<void>(get_pool_executor());
    auto done_future  = done_promise.get_future();
    future.add_continuation([done_promise]() mutable {
        done_promise.set_value();
    }, mc::futures::launch::async, get_pool_executor());

    auto status = future.wait_for(2s);
    EXPECT_EQ(status, mc::future_status::ready);
    EXPECT_EQ(future.get<int>(), 7);
    (void)done_future.wait_for(2s);
}

// add_continuation 是拉取触发点之一。
TEST_F(DeferredFuturesTest, AddContinuationTriggersStart)
{
    std::atomic<int> ran{0};
    auto             future = mc::make_deferred_future<int>(get_pool_executor(), [&ran]() {
        ran.fetch_add(1, std::memory_order_acq_rel);
        return 99;
    });

    auto done_promise = mc::make_promise<int>(get_pool_executor());
    auto done_future  = done_promise.get_future();

    future.add_continuation([future, done_promise]() mutable {
        done_promise.set_value(future.get<int>());
    }, mc::futures::launch::async, get_pool_executor());

    auto status = done_future.wait_for(2s);
    EXPECT_EQ(status, mc::future_status::ready);
    EXPECT_EQ(done_future.get<int>(), 99);
    EXPECT_EQ(ran.load(), 1);
}

// 并发拉取下任务体仅启动一次（CAS 抢占）。
TEST_F(DeferredFuturesTest, ConcurrentPullStartsTaskOnce)
{
    constexpr int    pullers = 32;
    std::atomic<int> ran{0};

    auto future = mc::make_deferred_future<int>(get_pool_executor(), [&ran]() {
        ran.fetch_add(1, std::memory_order_acq_rel);
        std::this_thread::sleep_for(20ms);
        return 1;
    });

    std::vector<std::thread> threads;
    threads.reserve(pullers);
    for (int i = 0; i < pullers; ++i) {
        threads.emplace_back([f = future]() mutable { (void)f.get<int>(); });
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(ran.load(), 1);
    EXPECT_EQ(future.get<int>(), 1);
}

// 完成时通过 executor 异步唤醒：consumer continuation 必然在 producer 栈帧返回后才被唤醒；
// 反例（同步发布）下 consumer 可能在 producer_finished 置位前就观察到 ready。
TEST_F(DeferredFuturesTest, PublishIsLoopDriven)
{
    auto strand = mc::runtime::runtime_context::create_strand();

    std::atomic<bool>  reached_set_value{false};
    std::atomic<bool>  consumer_observed_ready{false};
    std::atomic<int>   consumer_observe_order{0};
    std::atomic<bool>  producer_finished{false};

    auto promise = mc::make_promise<int>(strand);
    promise.set_policy(mc::futures::launch::deferred);
    auto future = promise.get_future();

    struct holder {
        mc::futures::Promise<int> p;
        std::atomic<bool>*        reached;
        std::atomic<bool>*        finished;
    };
    auto* h = new holder{promise, &reached_set_value, &producer_finished};
    future.get_state()->install_deferred_task(
        +[](void* ctx) noexcept {
            auto* hh = static_cast<holder*>(ctx);
            hh->reached->store(true, std::memory_order_release);
            hh->p.set_value(123);
            // 模拟 set_value 后生产者栈帧上还有清理工作未结束。
            std::this_thread::sleep_for(50ms);
            hh->finished->store(true, std::memory_order_release);
        },
        h,
        +[](void* ctx) noexcept { delete static_cast<holder*>(ctx); });

    auto consumer_strand = mc::runtime::runtime_context::create_strand();
    auto done_promise    = mc::make_promise<void>(get_pool_executor());
    auto done_future     = done_promise.get_future();

    future.add_continuation([&consumer_observed_ready, &producer_finished, &consumer_observe_order,
                             done_promise]() mutable {
        consumer_observed_ready.store(true, std::memory_order_release);
        consumer_observe_order.store(producer_finished.load(std::memory_order_acquire) ? 1 : 0,
                                     std::memory_order_release);
        done_promise.set_value();
    }, mc::futures::launch::async, consumer_strand);

    auto status = done_future.wait_for(5s);
    ASSERT_EQ(status, mc::future_status::ready);

    EXPECT_TRUE(reached_set_value.load());
    EXPECT_TRUE(consumer_observed_ready.load());
    EXPECT_EQ(consumer_observe_order.load(), 1)
        << "consumer continuation observed ready before producer finished its stack frame; "
           "this indicates mark_ready was synchronously dispatched on producer's stack.";
}

// 在自身 thread_pool 的 worker 线程上同步 get 自身的 deferred future：
// thread_pool 多 worker 时，task body post 后由其他 worker 拉起执行，
// 当前 worker 上 m_cv.wait 也走嵌套驱动，整体不死锁。
TEST_F(DeferredFuturesTest, SelfPullOnPoolWorkerDrainsTask)
{
    auto pool_exec = get_pool_executor();

    auto promise = mc::make_promise<int>();
    auto trigger = promise.get_future();

    pool_exec.post([pool_exec, promise]() mutable {
        std::atomic<int> ran{0};
        auto             deferred = mc::make_deferred_future<int>(pool_exec, [&ran]() {
            ran.fetch_add(1, std::memory_order_acq_rel);
            return 7;
        });
        const int value     = deferred.get<int>();
        const int run_count = ran.load(std::memory_order_acquire);
        promise.set_value(value == 7 && run_count == 1 ? 1 : 0);
    });

    auto status = trigger.wait_for(5s);
    ASSERT_EQ(status, mc::future_status::ready);
    EXPECT_EQ(trigger.get<int>(), 1);
}

// pool worker 上 wait_for 短路返回 deferred 不死锁；主动 wait() 后嵌套驱动取值。
TEST_F(DeferredFuturesTest, SelfWaitForReturnsDeferredOnPoolWorker)
{
    auto pool_exec = get_pool_executor();

    auto promise = mc::make_promise<int>();
    auto trigger = promise.get_future();

    pool_exec.post([pool_exec, promise]() mutable {
        auto deferred  = mc::make_deferred_future<int>(pool_exec, []() { return 42; });
        auto status    = deferred.wait_for(50ms);
        const bool ok1 = (status == mc::future_status::deferred);
        const bool ok2 = !deferred.get_state()->is_deferred_task_started();
        deferred.wait();
        const bool ok3 = (deferred.get<int>() == 42);
        promise.set_value(ok1 && ok2 && ok3 ? 1 : 0);
    });

    auto status = trigger.wait_for(5s);
    ASSERT_EQ(status, mc::future_status::ready);
    EXPECT_EQ(trigger.get<int>(), 1);
}

// 行为边界锚定：串行化执行器（runtime_context::create_strand）的 worker 上同步
// wait/get 自身派发的 deferred future——串行化执行器同时只能跑一个任务，
// 当前任务还在栈上时下一个任务拿不到调度槽，wait/get 会无限挂起。
//
// 这是用户侧反模式（应改用 add_continuation 异步链路）。本用例用
// add_continuation 抢先启动 task，再 wait_for(short) 显式记录 timeout 行为
// 而不真正阻塞测试。shared 状态托管确保 lambda 在测试栈销毁后仍能安全访问。
TEST_F(DeferredFuturesTest, SelfPullOnSerializingExecutorIsAUserError)
{
    struct shared {
        std::atomic<int> probe_status{static_cast<int>(mc::future_status::invalid)};
        std::atomic<int> task_started_during_wait{0};
        std::atomic<int> task_started_total{0};
    };
    auto state  = std::make_shared<shared>();
    auto strand = mc::runtime::runtime_context::create_strand();

    auto trigger_promise = mc::make_promise<void>();
    auto trigger         = trigger_promise.get_future();

    strand.post([strand, state, trigger_promise]() mutable {
        auto deferred = mc::make_deferred_future<int>(strand, [state]() {
            state->task_started_total.fetch_add(1, std::memory_order_acq_rel);
            return 99;
        });
        deferred.add_continuation([]() noexcept {},
                                  mc::futures::launch::async, strand);

        const auto status = deferred.wait_for(50ms);
        // 在 strand handler 内、wait_for 紧后采样：此刻当前 handler 仍持有
        // strand 的调度权，串行化语义保证下一个 task（task body）一定还没跑。
        state->task_started_during_wait.store(state->task_started_total.load(std::memory_order_acquire),
                                              std::memory_order_release);
        state->probe_status.store(static_cast<int>(status), std::memory_order_release);
        trigger_promise.set_value();
    });

    ASSERT_EQ(trigger.wait_for(5s), mc::future_status::ready);
    EXPECT_EQ(state->probe_status.load(),
              static_cast<int>(mc::future_status::timeout));
    EXPECT_EQ(state->task_started_during_wait.load(), 0)
        << "task body 不应被拉起：串行化执行器上当前 handler 未返回，post 进去的"
           "deferred task 拿不到调度槽，self-pull 是用户侧反模式";

    // strand 上 pending 的 deferred task body / mark_ready / continuation 在
    // 上面 lambda 退出后才会被依次拉起来，必须等它们全部跑完再退出测试，
    // 否则 fixture 析构时仍有任务引用 state，造成 join 永挂或 UAF。
    auto barrier_promise = mc::make_promise<void>();
    auto barrier         = barrier_promise.get_future();
    strand.post([barrier_promise]() mutable { barrier_promise.set_value(); });
    ASSERT_EQ(barrier.wait_for(5s), mc::future_status::ready);
}

// 高阶工厂：值返回路径。
TEST_F(DeferredFuturesTest, MakeDeferredFutureValue)
{
    auto future = mc::make_deferred_future<std::string>(get_pool_executor(), []() {
        return std::string("hello");
    });
    EXPECT_FALSE(future.is_ready());
    EXPECT_EQ(future.get<std::string>(), "hello");
}

// 高阶工厂：void 返回路径。
TEST_F(DeferredFuturesTest, MakeDeferredFutureVoid)
{
    std::atomic<int> ran{0};
    auto             future = mc::make_deferred_future<void>(get_pool_executor(), [&ran]() {
        ran.fetch_add(1, std::memory_order_acq_rel);
    });
    EXPECT_EQ(ran.load(), 0);
    future.wait();
    EXPECT_EQ(ran.load(), 1);
    EXPECT_TRUE(future.is_ready());
}

// 高阶工厂：task 抛异常会被捕获并转为 future 异常状态。
TEST_F(DeferredFuturesTest, MakeDeferredFutureCapturesException)
{
    auto future = mc::make_deferred_future<int>(get_pool_executor(), []() -> int {
        throw std::runtime_error("boom");
    });

    EXPECT_THROW({ (void)future.get<int>(); }, std::runtime_error);
    EXPECT_TRUE(future.is_rejected());
}

// 多次 add_continuation 全部触发，但任务体仅启动一次。
TEST_F(DeferredFuturesTest, MultipleContinuationsAllFireOnce)
{
    std::atomic<int> ran{0};
    auto             future = mc::make_deferred_future<int>(get_pool_executor(), [&ran]() {
        ran.fetch_add(1, std::memory_order_acq_rel);
        return 5;
    });

    constexpr int n = 8;
    std::atomic<int>                              fired{0};
    std::vector<mc::futures::Promise<void>>       done_promises;
    std::vector<mc::futures::Future<void>>        done_futures;
    done_promises.reserve(n);
    done_futures.reserve(n);
    for (int i = 0; i < n; ++i) {
        auto p = mc::make_promise<void>(get_pool_executor());
        done_futures.push_back(p.get_future());
        done_promises.push_back(std::move(p));
    }

    for (int i = 0; i < n; ++i) {
        auto p = done_promises[i];
        future.add_continuation([&fired, p]() mutable {
            fired.fetch_add(1, std::memory_order_acq_rel);
            p.set_value();
        }, mc::futures::launch::async, get_pool_executor());
    }

    for (auto& f : done_futures) {
        ASSERT_EQ(f.wait_for(2s), mc::future_status::ready);
    }
    EXPECT_EQ(ran.load(), 1);
    EXPECT_EQ(fired.load(), n);
}

// 未启动的 deferred future 被 cancel 后，task body 不会被任何拉取动作启动。
TEST_F(DeferredFuturesTest, CancelBeforeStartPreventsTaskBody)
{
    std::atomic<int> ran{0};
    auto             future = mc::make_deferred_future<int>(get_pool_executor(), [&ran]() {
        ran.fetch_add(1, std::memory_order_acq_rel);
        return 1;
    });

    EXPECT_FALSE(future.is_ready());
    EXPECT_FALSE(future.get_state()->is_deferred_task_started());

    future.cancel();

    EXPECT_TRUE(future.is_ready());
    EXPECT_TRUE(future.get_state()->is_cancelled());

    EXPECT_THROW({ (void)future.get<int>(); }, mc::canceled_exception);

    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(ran.load(), 0);
    EXPECT_FALSE(future.get_state()->is_deferred_task_started());
}

// 已启动但 task 内 set_value 前被 cancel：set_value 静默无效，future 以 cancelled 终态发布。
TEST_F(DeferredFuturesTest, CancelAfterStartTaskSetValueIsIgnored)
{
    struct shared {
        std::atomic<int>  task_invocations{0};
        std::atomic<bool> ready_to_cancel{false};
        std::atomic<bool> task_can_finish{false};
    };
    auto state = std::make_shared<shared>();

    auto future = mc::make_deferred_future<int>(get_pool_executor(), [state]() {
        state->task_invocations.fetch_add(1, std::memory_order_acq_rel);
        state->ready_to_cancel.store(true, std::memory_order_release);
        while (!state->task_can_finish.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(1ms);
        }
        return 42;
    });

    auto done_promise = mc::make_promise<void>(get_pool_executor());
    auto done_future  = done_promise.get_future();
    future.add_continuation([done_promise]() mutable { done_promise.set_value(); },
                            mc::futures::launch::async, get_pool_executor());

    auto wait_until = std::chrono::steady_clock::now() + 2s;
    while (!state->ready_to_cancel.load(std::memory_order_acquire)
           && std::chrono::steady_clock::now() < wait_until) {
        std::this_thread::sleep_for(1ms);
    }
    // 即便此处 ASSERT 失败也要让 worker 退出，避免 task body 永挂在 fixture 退出后。
    auto release_worker = [&]() noexcept {
        state->task_can_finish.store(true, std::memory_order_release);
    };

    if (!state->ready_to_cancel.load()) {
        release_worker();
        FAIL() << "task body did not enter ready_to_cancel within 2s";
    }

    future.cancel();
    release_worker();

    EXPECT_EQ(done_future.wait_for(2s), mc::future_status::ready);
    EXPECT_EQ(state->task_invocations.load(), 1);
    EXPECT_TRUE(future.get_state()->is_deferred_task_started());
    EXPECT_TRUE(future.get_state()->is_cancelled());
    EXPECT_THROW({ (void)future.get<int>(); }, mc::canceled_exception);
}

// cancel 后 add_continuation 仍能在 executor 上触发，continuation 看到 cancelled state。
TEST_F(DeferredFuturesTest, ContinuationFiresAfterCancel)
{
    std::atomic<int> ran{0};
    auto             future = mc::make_deferred_future<int>(get_pool_executor(), [&ran]() {
        ran.fetch_add(1, std::memory_order_acq_rel);
        return 1;
    });

    future.cancel();
    EXPECT_TRUE(future.is_ready());

    auto done_promise = mc::make_promise<bool>(get_pool_executor());
    auto done_future  = done_promise.get_future();

    future.add_continuation([f = future, done_promise]() mutable {
        done_promise.set_value(f.get_state()->is_cancelled());
    }, mc::futures::launch::async, get_pool_executor());

    ASSERT_EQ(done_future.wait_for(2s), mc::future_status::ready);
    EXPECT_TRUE(done_future.get<bool>());

    EXPECT_EQ(ran.load(), 0);
    EXPECT_FALSE(future.get_state()->is_deferred_task_started());
}

// 任务启动后 wait_for 在未 ready 时应返回 timeout（而非 deferred），与 std::future 一致。
TEST_F(DeferredFuturesTest, WaitForAfterStartedReturnsTimeoutWhenNotReady)
{
    std::atomic<bool> task_can_finish{false};
    auto              future = mc::make_deferred_future<int>(get_pool_executor(),
                                                             [&task_can_finish]() {
        while (!task_can_finish.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(1ms);
        }
        return 7;
    });

    auto sink_promise = mc::make_promise<void>(get_pool_executor());
    auto sink_future  = sink_promise.get_future();
    future.add_continuation([sink_promise]() mutable { sink_promise.set_value(); },
                            mc::futures::launch::async, get_pool_executor());

    auto wait_until = std::chrono::steady_clock::now() + 2s;
    while (!future.get_state()->is_deferred_task_started()
           && std::chrono::steady_clock::now() < wait_until) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(future.get_state()->is_deferred_task_started());

    auto status = future.wait_for(50ms);
    EXPECT_EQ(status, mc::future_status::timeout);

    task_can_finish.store(true, std::memory_order_release);
    auto status2 = future.wait_for(2s);
    EXPECT_EQ(status2, mc::future_status::ready);
    EXPECT_EQ(future.get<int>(), 7);
    (void)sink_future.wait_for(2s);
}

// pool worker 上 add_continuation 抢占启动 task 后，同 pool 内 wait_for 走真等待路径，
// 嵌套驱动 task 队列，能在超时前把 task body 跑完并返回 ready。
TEST_F(DeferredFuturesTest, SelfWaitForDrainsTaskOnPoolWorker)
{
    auto pool_exec = get_pool_executor();

    auto trigger_promise = mc::make_promise<int>();
    auto trigger         = trigger_promise.get_future();

    pool_exec.post([pool_exec, trigger_promise]() mutable {
        auto deferred = mc::make_deferred_future<int>(pool_exec, []() { return 11; });
        deferred.add_continuation([]() noexcept {},
                                  mc::futures::launch::async, pool_exec);

        const auto status = deferred.wait_for(2s);
        const bool ok     = (status == mc::future_status::ready) && (deferred.get<int>() == 11);
        trigger_promise.set_value(ok ? 1 : 0);
    });

    auto status = trigger.wait_for(5s);
    ASSERT_EQ(status, mc::future_status::ready);
    EXPECT_EQ(trigger.get<int>(), 1);
}

// install 但从未拉取：state 析构 / pool 复用前 ctx_dtor 必被调用一次，无泄漏。
TEST_F(DeferredFuturesTest, NoPullDestroysContextWithoutLeak)
{
    std::atomic<int> dtor_count{0};
    {
        auto promise = mc::make_promise<int>(get_pool_executor());
        promise.set_policy(mc::futures::launch::deferred);
        auto future = promise.get_future();

        struct holder {
            std::atomic<int>* dc;
        };
        auto* h = new holder{&dtor_count};
        future.get_state()->install_deferred_task(
            +[](void* /*ctx*/) noexcept {},
            h,
            +[](void* ctx) noexcept {
                auto* hh = static_cast<holder*>(ctx);
                hh->dc->fetch_add(1, std::memory_order_acq_rel);
                delete hh;
            });
    }
    EXPECT_GE(dtor_count.load(), 1);
}

} // namespace
