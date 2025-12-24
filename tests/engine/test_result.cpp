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
#include <mc/error_engine.h>
#include <mc/exception.h>
#include <mc/future.h>
#include <mc/log/log_message.h>
#include <mc/result.h>
#include <test_utilities/test_base.h>

#include <string>

using namespace std::chrono_literals;

namespace {

class result_test : public mc::test::TestBase {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

constexpr std::string_view TestErrorName = "Test.TestError";

// 测试基本构造
TEST_F(result_test, test_basic_construction) {
    // 测试基本类型构造
    mc::result<int> int_result(42);
    EXPECT_TRUE(int_result.is_value());
    EXPECT_TRUE(int_result.is_completed());
    EXPECT_EQ(int_result.get(), 42);

    // 测试字符串构造
    mc::result<std::string> str_result(std::string("test"));
    EXPECT_TRUE(str_result.is_value());
    EXPECT_TRUE(str_result.is_completed());
    EXPECT_EQ(str_result.get(), "test");

    // 测试 future 构造
    auto            promise = mc::make_promise<int>();
    auto            future  = promise.get_future();
    mc::result<int> future_result(std::move(future));
    EXPECT_TRUE(future_result.is_future());
    EXPECT_FALSE(future_result.is_completed());

    promise.set_value(42);
    EXPECT_TRUE(future_result.is_completed());
    EXPECT_EQ(future_result.get(), 42);
}

// 测试错误构造
TEST_F(result_test, test_error_construction) {
    // 测试从错误名和消息构造
    mc::result<int> error_result(mc::make_error(TestErrorName, "test error message"));
    EXPECT_TRUE(error_result.is_error());
    EXPECT_TRUE(error_result.is_completed());

    auto error = error_result.get_error();
    EXPECT_EQ(error && error->is_set(), true);
    EXPECT_EQ(error->get_name(), TestErrorName);
    EXPECT_EQ(error->get_message(), "test error message");

    // 测试从异常构造
    auto            ex = MC_MAKE_EXCEPTION(mc::invalid_arg_exception, "test exception");
    mc::result<int> exception_result(mc::error::from_exception(ex));
    EXPECT_TRUE(exception_result.is_error());
    EXPECT_TRUE(exception_result.is_completed());
    EXPECT_THROW(exception_result.get(), mc::method_call_exception);
}

// 测试移动构造和赋值
TEST_F(result_test, test_move_operations) {
    // 测试移动构造
    mc::result<int> original(42);
    mc::result<int> moved(std::move(original));
    EXPECT_TRUE(moved.is_value());
    EXPECT_EQ(moved.get(), 42);

    // 测试移动赋值
    mc::result<int> assigned;
    assigned = std::move(moved);
    EXPECT_TRUE(assigned.is_value());
    EXPECT_EQ(assigned.get(), 42);

    // 测试 future 移动
    auto            promise = mc::make_promise<int>();
    auto            future  = promise.get_future();
    mc::result<int> future_result(std::move(future));
    mc::result<int> moved_future(std::move(future_result));
    EXPECT_TRUE(moved_future.is_future());

    promise.set_value(42);
    EXPECT_EQ(moved_future.get(), 42);
}

// 测试链式操作
TEST_F(result_test, test_chaining) {
    // 测试值的链式操作
    mc::result<int> value_result(42);
    auto            chained_value = value_result.then([](int v) {
        return v * 2;
    });
    EXPECT_EQ(chained_value.get(), 84);

    // 测试future的链式操作
    auto            promise = mc::make_promise<int>();
    auto            future  = promise.get_future();
    mc::result<int> future_result(std::move(future));

    auto chained_future = future_result.then([](int v) {
        return v * 2;
    });

    promise.set_value(42);
    EXPECT_EQ(chained_future.get(), 84);

    // 测试错误的链式操作
    mc::result<int> error_result(mc::make_error(TestErrorName, "test error"));
    auto            chained_error = error_result.then([](int v) {
        return v * 2;
    });

    EXPECT_THROW(chained_error.get(), mc::method_call_exception);
}

// 测试错误处理
TEST_F(result_test, test_error_handling) {
    // 测试catch_error处理
    mc::result<int> error_result(mc::make_error(TestErrorName, "test error"));

    auto recovered = error_result.catch_error([](const mc::exception& ex) {
        return 42;
    });
    EXPECT_EQ(recovered.get(), 42);

    // 测试链式错误处理
    auto            promise = mc::make_promise<int>();
    auto            future  = promise.get_future();
    mc::result<int> future_result(std::move(future));

    auto chained = future_result.then([](int v) -> int {
        MC_THROW(mc::invalid_arg_exception, "test exception");
        return v;
    }).catch_error([](const mc::exception& ex) {
        return 42;
    });

    promise.set_value(10);
    EXPECT_EQ(chained.get(), 42);
}

// 测试超时处理
TEST_F(result_test, test_timeout) {
    auto            promise = mc::make_promise<int>();
    mc::result<int> result  = promise.get_future();

    // 测试超时
    auto timeout_result = mc::timeout(result.as_future(), 50ms);
    EXPECT_THROW(timeout_result.get(), mc::timeout_exception);

    // 测试正常完成
    promise        = mc::make_promise<int>();
    result         = promise.get_future();
    timeout_result = mc::timeout(result.as_future(), 1s);

    promise.set_value(42);
    EXPECT_EQ(timeout_result.get(), 42);
}

// 测试取消操作
TEST_F(result_test, test_cancellation_result_then) {
    auto            promise      = mc::make_promise<int>();
    mc::result<int> result       = promise.get_future();
    auto            chain_result = result.then([]() {
        return mc::delay(10ms);
    });

    result.cancel();
    ASSERT_TRUE(result.is_error());
    EXPECT_THROW(result.get(), mc::canceled_exception);

    ASSERT_TRUE(chain_result.is_rejected());
    EXPECT_THROW(chain_result.get(), mc::canceled_exception);
}

// 测试取消操作
TEST_F(result_test, test_cancellation_result_catch_error) {
    auto            promise      = mc::make_promise<int>();
    mc::result<int> result       = promise.get_future();
    auto            chain_result = result.catch_error([]() {
        return mc::delay(10ms);
    });

    result.cancel();
    ASSERT_TRUE(result.is_error());
    EXPECT_THROW(result.get(), mc::canceled_exception);

    ASSERT_TRUE(chain_result.is_rejected());
    EXPECT_THROW(chain_result.get(), mc::canceled_exception);
}

// 测试组合操作
TEST_F(result_test, test_combined_operations) {
    auto p1 = mc::make_promise<int>();
    auto p2 = mc::make_promise<std::string>();

    mc::result<int>         r1 = p1.get_future();
    mc::result<std::string> r2 = p2.get_future();

    auto combined = mc::all(r1.as_future(), r2.as_future());

    p1.set_value(42);
    p2.set_value("test");

    auto [v1, v2] = combined.get();
    EXPECT_EQ(v1, 42);
    EXPECT_EQ(v2, "test");
}

// 测试类型转换
TEST_F(result_test, test_type_conversion) {
    // 测试基础类型转换
    mc::result<int> int_result(42);

    auto double_result = int_result.then([](int v) {
        return static_cast<double>(v);
    });
    EXPECT_EQ(double_result.get(), 42.0);

    // 测试字符串转换
    mc::result<std::string> str_result(std::string("42"));

    auto converted_result = str_result.then([](const std::string& s) {
        return std::stoi(s);
    });
    EXPECT_EQ(converted_result.get(), 42);
}

// 测试void类型特化
TEST_F(result_test, test_void_specialization) {
    // 测试void返回值
    mc::result<void> void_result;
    EXPECT_NO_THROW(void_result.get());

    // 测试void到值的转换
    auto value_result = void_result.then([]() {
        return 42;
    });
    EXPECT_EQ(value_result.get(), 42);

    // 测试值到void的转换
    mc::result<int> int_result(42);
    auto            converted_void = int_result.then([](int v) {
        // do nothing
    });
    EXPECT_NO_THROW(converted_void.get());
}

TEST_F(result_test, test_make_result) {
    // mc::result<int>
    auto r1 = mc::make_result(42);
    EXPECT_EQ(r1.get(), 42);

    // mc::result<std::string_view>
    auto r2 = mc::make_result<std::string_view>("test");
    EXPECT_EQ(r2.get(), "test");

    // mc::result<int>
    auto r3 = mc::make_result<int>(mc::make_error(TestErrorName, "test error"));
    EXPECT_THROW(r3.get(), mc::method_call_exception);

    // mc::result<void>
    auto r4 = mc::make_result();
    EXPECT_NO_THROW(r4.get());

    // mc::result<std::string_view>
    auto r5 = mc::make_result(mc::resolve<std::string_view>("123"));
    EXPECT_EQ(r5.get(), "123");

    // mc::result<int>
    auto r6 = mc::make_result(mc::delay(50ms).then([]() {
        return 42;
    }));
    EXPECT_EQ(r6.get(), 42);

    // mc::result<void>
    auto r7 = mc::make_result(mc::delay(50ms).then([]() {
        MC_THROW(mc::invalid_arg_exception, "test exception");
    }));
    EXPECT_THROW(r7.get(), mc::invalid_arg_exception);
}

TEST_F(result_test, test_default_error_selection) {
    mc::error_engine::reset_for_test();

    auto default_error = mc::detail::get_default_error();
    ASSERT_TRUE(default_error);
    EXPECT_EQ(default_error->get_name(), "org.freedesktop.DBus.Error.Failed");
    EXPECT_EQ(default_error->get_message(), "Failed to execute method");

    auto& engine      = mc::error_engine::get_instance();
    auto  custom_err  = mc::make_error("test.custom.error", "custom message");
    auto  previous    = engine.set_last_error(custom_err);
    EXPECT_FALSE(previous);

    auto reused = mc::detail::get_default_error();
    EXPECT_EQ(reused, custom_err);
}

TEST_F(result_test, test_throw_and_make_method_call_exception) {
    mc::error_engine::reset_for_test();

    auto err = mc::make_error("test.throw.error", "throw message");
    ASSERT_THROW(
        {
            try {
                mc::detail::throw_method_call_exception(err);
            } catch (const mc::method_call_exception& ex) {
                EXPECT_EQ(ex.name(), "test.throw.error");
                EXPECT_TRUE(std::string(ex.to_string()).find("throw message") != std::string::npos);
                throw;
            }
        },
        mc::method_call_exception);

    mc::error_engine::reset_for_test();
    try {
        mc::detail::throw_method_call_exception(nullptr);
        FAIL() << "expected default error";
    } catch (const mc::method_call_exception& ex) {
        EXPECT_EQ(ex.name(), "org.freedesktop.DBus.Error.Failed");
    }

    auto made = mc::detail::make_method_call_exception(err);
    EXPECT_EQ(made.name(), "test.throw.error");
    EXPECT_TRUE(std::string(made.to_string()).find("throw message") != std::string::npos);
}

TEST_F(result_test, test_make_method_call_exception_uses_last_error) {
    mc::error_engine::reset_for_test();
    auto& engine    = mc::error_engine::get_instance();
    auto  last_err  = mc::make_error("test.last.error", "last error");
    engine.set_last_error(last_err);

    auto ex = mc::detail::make_method_call_exception(nullptr);
    EXPECT_EQ(ex.name(), "test.last.error");
    EXPECT_TRUE(std::string(ex.to_string()).find("last error") != std::string::npos);
}

} // namespace