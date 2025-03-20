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

/**
 * @file main.cpp
 * @brief 数据库组件测试程序主入口
 */
#include "common.h"

// 主函数
int main(int argc, char** argv) {
    // 设置信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 设置日志级别
    mc::get_default_logger().set_level(mc::log::level::info);
    
    ilog("数据库共享内存组件测试程序启动");
    
    // 确定要运行的测试
    TestType test_type = TestType::ALL;
    
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "memory") {
            test_type = TestType::SHARED_MEMORY;
        } else if (arg == "mutex") {
            test_type = TestType::SHARED_MUTEX;
        } else if (arg == "multi") {
            test_type = TestType::MULTI_PROCESS;
        } else if (arg == "multi_mutex") {
            test_type = TestType::MULTI_PROCESS_MUTEX;
        } else if (arg == "rwmutex") {
            test_type = TestType::SHARED_RW_MUTEX;
        } else if (arg == "long") {
            test_shared_memory_long_running();
            return 0;
        }
    }
    
    // 运行选定的测试
    switch (test_type) {
        case TestType::SHARED_MEMORY:
            test_shared_memory();
            break;
        case TestType::SHARED_MUTEX:
            test_shared_mutex();
            break;
        case TestType::MULTI_PROCESS:
            test_multi_process_shared_memory();
            break;
        case TestType::MULTI_PROCESS_MUTEX:
            test_multi_process_shared_mutex();
            break;
        case TestType::SHARED_RW_MUTEX:
            test_shared_rw_mutex();
            break;
        case TestType::ALL:
            test_shared_memory();
            test_shared_mutex();
            test_multi_process_shared_mutex();
            test_multi_process_shared_memory();
            test_shared_rw_mutex();
            break;
    }
    
    ilog("所有测试完成");
    return 0;
} 