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
#include <mc/engine.h>
#include <mc/engine/event.h>
#include <mc/exception.h>

using namespace std::chrono_literals;

namespace {

// 测试用的异步接口
class AsyncInterface : public mc::engine::interface<AsyncInterface> {
public:
    MC_INTERFACE("org.test.AsyncInterface")

    // 立即返回的同步方法
    int32_t sync_method(int32_t value)
    {
        return value * 2;
    }

    // 延迟返回的异步方法
    mc::result<int32_t> async_method(int32_t value)
    {
        return mc::delay(mc::milliseconds(100)).then([value]() {
            return value * 3;
        });
    }

    // 可能同步或异步返回的混合方法
    mc::result<std::string> hybrid_method(const std::string& value, bool immediate)
    {
        if (immediate) {
            std::string result = "immediate:" + value;
            operation_completed(result);
            return result;
        }
        return mc::delay(mc::milliseconds(100)).then([this, value]() {
            std::string result = "delayed:" + value;
            operation_completed(result);
            return result;
        });
    }

    // 抛出异常的方法
    mc::result<void> error_method(bool immediate, bool use_throw)
    {
        if (immediate) {
            if (use_throw) {
                MC_THROW(mc::invalid_arg_exception, "immediate error");
            } else {
                return mc::make_shared<mc::error>("TestError", "immediate error");
            }
        }

        return mc::delay(mc::milliseconds(100)).then([use_throw]() {
            if (use_throw) {
                MC_THROW(mc::invalid_arg_exception, "delayed error");
            } else {
                return mc::make_shared<mc::error>("TestError", "delayed error");
            }
        });
    }

    mc::result<void> void_method(bool immediate)
    {
        if (immediate) {
            return {};
        }

        return mc::delay(mc::milliseconds(100)).then([]() {
        });
    }

    // 可取消的长时间运行方法
    mc::result<std::string> long_running_method()
    {
        return mc::delay(mc::milliseconds(1000)).then([]() {
            return "completed";
        });
    }

    // 链式异步操作方法
    mc::result<int32_t> chain_method(int32_t value)
    {
        return mc::delay(mc::milliseconds(50))
            .then([value]() {
            return value + 1;
        })
            .then([](int32_t v) {
            return mc::delay(mc::milliseconds(50)).then([v]() {
                return v * 2;
            });
        }).then([](int32_t v) {
            return v + 10;
        });
    }

    // 并发异步操作方法
    mc::result<std::vector<int32_t>> parallel_method(const std::vector<int32_t>& values)
    {
        std::vector<mc::future<int32_t>> futures;
        for (auto value : values) {
            futures.push_back(mc::delay(mc::milliseconds(50)).then([value]() {
                return value * 2;
            }));
        }
        return mc::all(futures.begin(), futures.end());
    }

    mc::signal<void(std::string_view)> operation_completed;
};

class AsyncObject : public mc::engine::object<AsyncObject> {
public:
    MC_OBJECT(AsyncObject, "AsyncObject", "/org/test/AsyncObject", (AsyncInterface))

    AsyncObject(mc::engine::core_object* parent = nullptr) : mc::engine::object<AsyncObject>(parent)
    {}

    AsyncInterface m_iface;
};

} // namespace

MC_REFLECT(AsyncInterface,
           ((sync_method, "SyncMethod"))((async_method, "AsyncMethod"))((hybrid_method, "HybridMethod"))(
               (error_method, "ErrorMethod"))((void_method, "VoidMethod"))((long_running_method, "LongRunningMethod"))(
               (chain_method, "ChainMethod"))((parallel_method, "ParallelMethod"))((operation_completed,
                                                                                    "OperationCompleted")))
MC_REFLECT(AsyncObject, ((m_iface, "async")))

class async_invoke_test : public ::testing::Test {
protected:
    void SetUp() override
    {}

    void TearDown() override
    {}

    AsyncObject                  obj;
    mc::engine::abstract_object& obj_base = obj;
};

// 测试同步方法调用
TEST_F(async_invoke_test, test_sync_method)
{
    auto result = obj_base.async_invoke("SyncMethod", {42});
    EXPECT_TRUE(result.is_value() && result.is_ready());
    EXPECT_EQ(result.get(), 84);
}

// 测试异步方法调用
TEST_F(async_invoke_test, test_async_method)
{
    auto result = obj_base.async_invoke("AsyncMethod", {42});
    EXPECT_TRUE(result.is_future() && !result.is_ready());
    EXPECT_EQ(result.get(), 126);
}

// 测试混合方法调用
TEST_F(async_invoke_test, test_hybrid_method)
{
    // 测试立即返回
    auto immediate_result = obj_base.async_invoke("HybridMethod", {"test", true});
    EXPECT_TRUE(immediate_result.is_value() && immediate_result.is_ready());
    EXPECT_EQ(immediate_result.get(), "immediate:test");

    // 测试延迟返回
    auto delayed_result = obj_base.async_invoke("HybridMethod", {"test", false});
    EXPECT_TRUE(delayed_result.is_future() && !delayed_result.is_ready());
    EXPECT_EQ(delayed_result.get(), "delayed:test");
}

// 测试错误处理
TEST_F(async_invoke_test, test_error_handling)
{
    // 测试立即错误（如果发生错误 get() 会抛出异常）
    auto immediate_error = obj_base.async_invoke("ErrorMethod", {true, true});
    EXPECT_THROW(immediate_error.get(), mc::invalid_arg_exception);

    // 验证通过 mc::engine::error 返回错误（验证通过 catch_error 捕获错误）
    immediate_error = obj_base.async_invoke("ErrorMethod", {true, false});
    immediate_error
        .catch_error([](const mc::exception& ex) -> mc::variant {
        EXPECT_EQ(ex.name(), "TestError");
        EXPECT_EQ(ex.top_message(), "immediate error");
        return {};
    }).wait();

    // 测试延迟错误（验证通过 get_error() 获取错误信息）
    auto delayed_error = obj_base.async_invoke("ErrorMethod", {false, true});
    EXPECT_TRUE(delayed_error.is_error()); // is_error() 函数会阻塞等待直到异步调用完成
    auto err = delayed_error.get_error();  // get_error() 函数会阻塞等待直到异步调用完成
    EXPECT_EQ(err && err->is_set(), true);
    EXPECT_EQ(err->get_message(), "delayed error");

    // 验证延迟错误，通过 mc::future<mc::error_ptr> 返回错误（验证通过 get_error() 获取错误信息）
    delayed_error = obj_base.async_invoke("ErrorMethod", {false, false});
    EXPECT_TRUE(delayed_error.is_error());
    auto err1 = delayed_error.get_error();
    EXPECT_EQ(err1 && err1->is_set(), true);
    EXPECT_EQ(err1->get_message(), "delayed error");
    EXPECT_EQ(err1->get_name(), "TestError");
}

TEST_F(async_invoke_test, test_void_result)
{
    auto result = obj_base.async_invoke("VoidMethod", {true});
    EXPECT_TRUE(result.is_value() && result.is_ready());
    EXPECT_EQ(result.get(), mc::variant());

    result = obj_base.async_invoke("VoidMethod", {false});
    EXPECT_TRUE(result.is_future() && !result.is_ready());
    bool async_completed = false;
    result
        .then([&]() {
        async_completed = true;
    }).wait();
    EXPECT_TRUE(async_completed);
}

// 测试取消操作
TEST_F(async_invoke_test, test_cancellation)
{
    auto result = obj_base.async_invoke("LongRunningMethod");
    EXPECT_TRUE(result.is_future() && !result.is_ready());

    // 取消操作
    result.cancel();
    EXPECT_THROW(result.get(), mc::canceled_exception);
}

// 测试链式异步操作
TEST_F(async_invoke_test, test_chain_operations)
{
    auto result = obj_base.async_invoke("ChainMethod", {5});
    EXPECT_TRUE(result.is_future() && !result.is_ready());
    EXPECT_EQ(result.get(), 22); // (5 + 1) * 2 + 10
}

// 测试并发异步操作
TEST_F(async_invoke_test, test_parallel_operations)
{
    std::vector<int32_t> values = {1, 2, 3, 4, 5};
    auto                 result = obj_base.async_invoke("ParallelMethod", {values});
    EXPECT_TRUE(result.is_future() && !result.is_ready());

    std::vector<int32_t> expected = {2, 4, 6, 8, 10};
    EXPECT_EQ(result.get().as_array(), expected);
}

// 测试超时处理
TEST_F(async_invoke_test, test_timeout)
{
    auto result         = obj_base.async_invoke("LongRunningMethod");
    auto timeout_result = mc::timeout(result.as_future(), mc::milliseconds(50));
    EXPECT_THROW(timeout_result.get(), mc::timeout_exception);
}

// 测试多个异步操作的组合
TEST_F(async_invoke_test, test_combined_operations)
{
    auto sync_result   = obj_base.async_invoke("SyncMethod", {10});
    auto async_result  = obj_base.async_invoke("AsyncMethod", {20});
    auto hybrid_result = obj_base.async_invoke("HybridMethod", {"test", false});

    // 等待所有操作完成
    auto combined = mc::all(sync_result.as_future(), async_result.as_future(), hybrid_result.as_future());

    auto [sync_value, async_value, hybrid_value] = combined.get();

    EXPECT_EQ(sync_value, 20);
    EXPECT_EQ(async_value, 60);
    EXPECT_EQ(hybrid_value, "delayed:test");
}

// 测试信号与异步操作的结合
TEST_F(async_invoke_test, test_signal_with_async)
{
    std::string signal_value;
    auto        conn = obj_base.connect("OperationCompleted", [&](const mc::variants& args) -> mc::variant {
        signal_value = args[0].as<std::string>();
        return {};
    });

    // 触发一个异步操作，完成时会发出信号
    auto result              = obj_base.async_invoke("HybridMethod", {"test", true});
    bool immediate_completed = false;
    result
        .then([&](const std::string& val) {
        EXPECT_EQ(signal_value, "immediate:test");
        immediate_completed = true;
    }).wait();
    EXPECT_TRUE(immediate_completed);

    // 触发一个异步操作，完成时会发出信号
    result                 = obj_base.async_invoke("HybridMethod", {"test", false});
    bool delayed_completed = false;
    result
        .then([&](const std::string& val) {
        EXPECT_EQ(signal_value, "delayed:test");
        delayed_completed = true;
    }).wait();
    EXPECT_TRUE(delayed_completed);
}

TEST_F(async_invoke_test, test_async_invoke_event_filter_wraps_sync_variant_as_ready_result)
{
    obj.install_event_filter([&](mc::core::object*, mc::event& e) -> bool {
        auto* invoke_event = dynamic_cast<mc::engine::method_invoke_event*>(&e);
        if (invoke_event == nullptr || invoke_event->method_name() != "AsyncMethod") {
            return false;
        }
        EXPECT_TRUE(invoke_event->is_async());
        invoke_event->set_result(404);
        return true;
    });

    auto result = obj_base.async_invoke("AsyncMethod", {99});
    EXPECT_TRUE(result.is_value() && result.is_ready());
    EXPECT_EQ(result.get(), 404);
}

TEST_F(async_invoke_test, test_async_invoke_event_filter_can_return_delayed_async_result)
{
    obj.install_event_filter([&](mc::core::object*, mc::event& e) -> bool {
        auto* invoke_event = dynamic_cast<mc::engine::method_invoke_event*>(&e);
        if (invoke_event == nullptr || invoke_event->method_name() != "AsyncMethod") {
            return false;
        }
        EXPECT_TRUE(invoke_event->is_async());
        invoke_event->set_async_result(mc::delay(mc::milliseconds(20)).then([]() {
            return mc::variant(900);
        }));
        return true;
    });

    auto result = obj_base.async_invoke("AsyncMethod", {1});
    EXPECT_TRUE(result.is_future() && !result.is_ready());
    EXPECT_EQ(result.get(), 900);
}

TEST_F(async_invoke_test, test_async_invoke_event_filter_can_return_error_async_result)
{
    obj.install_event_filter([&](mc::core::object*, mc::event& e) -> bool {
        auto* invoke_event = dynamic_cast<mc::engine::method_invoke_event*>(&e);
        if (invoke_event == nullptr || invoke_event->method_name() != "AsyncMethod") {
            return false;
        }
        invoke_event->set_async_result(mc::result<mc::variant>(mc::make_shared<mc::error>("FilterErr", "from filter")));
        return true;
    });

    auto result = obj_base.async_invoke("AsyncMethod", {1});
    EXPECT_TRUE(result.is_error());
    auto err = result.get_error();
    ASSERT_TRUE(err && err->is_set());
    EXPECT_EQ(err->get_name(), "FilterErr");
    EXPECT_EQ(err->get_message(), "from filter");
}

TEST_F(async_invoke_test, test_async_invoke_event_filter_observe_falls_through_to_routing)
{
    int filter_hits = 0;
    obj.install_event_filter([&](mc::core::object*, mc::event& e) -> bool {
        auto* invoke_event = dynamic_cast<mc::engine::method_invoke_event*>(&e);
        if (invoke_event != nullptr && invoke_event->method_name() == "AsyncMethod" && invoke_event->is_async()) {
            ++filter_hits;
        }
        return false;
    });

    auto result = obj_base.async_invoke("AsyncMethod", {10});
    EXPECT_EQ(filter_hits, 1);
    EXPECT_EQ(result.get(), 30);
}