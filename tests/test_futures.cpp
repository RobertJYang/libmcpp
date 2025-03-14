#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include <mc/future.h>

using namespace std::chrono_literals;

TEST(FuturesTest, BasicPromiseFuture) {
    boost::asio::io_context io_context;
    auto                    promise = mc::future::make_promise<int>(io_context);
    auto                    future  = promise.get_future();

    promise.set_value(42);
    EXPECT_EQ(future.get(), 42);
}

TEST(FuturesTest, ErrorHandling) {
    boost::asio::io_context io_context;
    auto                    promise = mc::future::make_promise<int>(io_context);
    auto                    future  = promise.get_future()
                      .then([](int) {
                          throw std::runtime_error("测试异常");
                      })
                      .catch_error([](const std::exception& e) {
                          return std::string(e.what());
                      });

    promise.set_value(0);
    io_context.run();
    EXPECT_EQ(future.get(), "测试异常");
}

TEST(FuturesTest, ExecutionPolicy) {
    boost::asio::io_context io_context;
    auto                    promise    = mc::future::make_promise<int>(io_context);
    auto                    start_time = std::chrono::steady_clock::now();

    auto future = promise.get_future()
                      .then(
                          [](int value) {
                              std::this_thread::sleep_for(100ms);
                              return value * 2;
                          },
                          mc::future::launch::async)
                      .then(
                          [](int value) {
                              return value + 1;
                          },
                          mc::future::launch::deferred);

    promise.set_value(20);
    io_context.run();

    auto result  = future.get();
    auto elapsed = std::chrono::steady_clock::now() - start_time;

    EXPECT_EQ(result, 41);
    EXPECT_GE(elapsed, 100ms);
}

TEST(FuturesTest, Timeout) {
    boost::asio::io_context io_context;
    auto                    promise = mc::future::make_promise<int>(io_context);
    auto                    future  = promise.get_future();

    std::thread delayed_set([&promise]() {
        std::this_thread::sleep_for(200ms);
        promise.set_value(42);
    });

    EXPECT_THROW(future.get_for(100ms), mc::future::timeout_error);
    delayed_set.join();
}