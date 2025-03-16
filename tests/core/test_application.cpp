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

#include "mc/core/application.h"
#include <gtest/gtest.h>
#include <thread>
#include <future>
#include <chrono>

namespace mc {
namespace test {

// 应用程序单例测试
TEST(ApplicationTest, Singleton) {
    // 获取应用程序实例
    application& app1 = application::instance();
    application& app2 = application::instance();
    
    // 两次获取的应该是同一个实例
    EXPECT_EQ(&app1, &app2);
}

// 版本管理测试
TEST(ApplicationTest, VersionManagement) {
    application& app = application::instance();
    
    // 设置版本
    std::string version = "1.2.3";
    app.set_version(version);
    
    // 验证版本
    EXPECT_EQ(app.version(), version);
}

// 管理器访问测试
TEST(ApplicationTest, ManagerAccess) {
    application& app = application::instance();
    
    // 验证各个管理器都可以访问
    EXPECT_NO_THROW(app.get_plugin_manager());
    EXPECT_NO_THROW(app.get_service_manager());
    EXPECT_NO_THROW(app.get_config_manager());
    EXPECT_NO_THROW(app.get_supervisor_manager());
}

// IO上下文测试
TEST(ApplicationTest, IOContext) {
    application& app = application::instance();
    
    // 获取IO上下文和执行器
    auto& io_context = app.get_io_context();
    auto& strand = app.get_strand();
    
    // 验证IO上下文可用
    EXPECT_FALSE(io_context.stopped());
}

// 初始化测试
TEST(ApplicationTest, Initialize) {
    application& app = application::instance();
    
    // 简单初始化
    bool result = app.initialize();
    EXPECT_TRUE(result);
    
    // 使用命令行参数初始化
    const char* argv[] = {"program", "--threads=2"};
    int argc = sizeof(argv) / sizeof(argv[0]);
    
    result = app.initialize(argc, const_cast<char**>(argv));
    EXPECT_TRUE(result);
}

// 生命周期管理测试
TEST(ApplicationTest, Lifecycle) {
    application& app = application::instance();
    
    // 启动应用程序
    app.start();
    EXPECT_FALSE(app.is_stopped());
    
    // 停止应用程序
    app.stop();
    EXPECT_TRUE(app.is_stopped());
    
    // 清理资源
    EXPECT_NO_THROW(app.cleanup());
}

// 异步任务测试
TEST(ApplicationTest, AsyncTasks) {
    // 重置单例状态
    application::reset_for_test();
    
    application& app = application::instance();
    
    // 初始化应用程序
    app.initialize();
    
    // 启动但不要调用exec()，以免阻塞测试
    app.start();
    
    // 在应用程序的执行器上运行任务
    bool task_executed = false;
    boost::asio::post(app.get_strand(), [&task_executed]() {
        task_executed = true;
    });
    
    // 手动运行IO上下文一次，确保任务执行
    app.get_io_context().poll_one();  // 使用poll_one而不是run_one，防止阻塞
    
    // 可能需要多次poll来确保任务执行
    for (int i = 0; i < 10 && !task_executed; ++i) {
        app.get_io_context().poll_one();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 验证任务已执行
    EXPECT_TRUE(task_executed);
    
    // 停止应用程序
    app.stop();
}

// 多线程测试
TEST(ApplicationTest, MultiThreading) {
    // 重置单例状态
    application::reset_for_test();
    
    application& app = application::instance();
    
    // 初始化应用程序
    app.initialize();
    
    // 启动应用程序
    app.start();
    
    // 设置一个标志来指示应用程序是否应该停止
    std::atomic<bool> should_stop{false};
    
    // 在另一个线程中启动应用程序的执行并在短时间后停止它
    std::thread exec_thread([&]() {
        // 创建一个线程来在短时间后停止应用程序
        std::thread stop_thread([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            should_stop.store(true);
            app.stop();
        });
        
        // 运行IO上下文直到没有更多工作或应用程序被停止
        while (!should_stop.load() && !app.get_io_context().stopped()) {
            app.get_io_context().poll_one();  // 使用poll_one而不是run_one，防止阻塞
        }
        
        // 等待停止线程完成
        stop_thread.join();
    });
    
    // 等待执行线程完成
    exec_thread.join();
    
    // 验证应用程序已停止
    EXPECT_TRUE(app.is_stopped());
}

// 错误处理测试
TEST(ApplicationTest, ErrorHandling) {
    // 重置单例状态
    application::reset_for_test();
    
    application& app = application::instance();
    
    // 使用无效的命令行参数
    const char* argv[] = {"program", "--invalid-option"};
    int argc = sizeof(argv) / sizeof(argv[0]);
    
    // 初始化应该成功，因为无效选项会被忽略
    bool result = app.initialize(argc, const_cast<char**>(argv));
    EXPECT_TRUE(result);
}

} // namespace test
} // namespace mc 