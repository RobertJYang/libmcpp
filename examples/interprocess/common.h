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
 * @file common.h
 * @brief 共享内存和互斥锁组件测试程序的公共定义
 */
#ifndef MC_INTERPROCESS_EXAMPLES_COMMON_H
#define MC_INTERPROCESS_EXAMPLES_COMMON_H

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <shared_mutex>
#include <string>
#include <sys/mman.h>
#include <sys/wait.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>

// 引入需要测试的组件
#include "mc/interprocess/mutex.h"
#include "mc/interprocess/shared_memory.h"
#include "mc/interprocess/shared_memory_allocator.h"
#include "mc/interprocess/shared_memory_manager.h"
#include "mc/log.h"
#include "mc/variant.h"

using namespace mc;
using namespace mc::interprocess;

// 测试功能的类型定义
enum class TestType {
    SHARED_MEMORY,              // 共享内存测试
    MULTI_PROCESS,              // 多进程共享内存测试
    MUTEX,                      // 进程间互斥锁测试
    MULTI_PROCESS_MUTEX,        // 多进程互斥锁测试
    SHARED_MUTEX,               // 进程间共享读写锁测试
    MULTI_PROCESS_SHARED_MUTEX, // 多进程共享读写锁测试
    ALL                         // 所有测试
};

// 信号处理函数变量声明
extern std::atomic<bool> g_running;
extern void              signal_handler(int signum);

// 共享内存辅助函数声明
extern void        write_to_shared_memory(shared_memory_allocator& allocator, const std::string& data);
extern std::string read_from_shared_memory(shared_memory_allocator& allocator, void* ptr);

// 测试函数声明
namespace mc {
namespace interprocess {
namespace test {

extern void test_shared_memory();
extern void test_multi_process_shared_memory();
extern void test_shared_memory_long_running();

extern void test_mutex();
extern void test_shared_mutex();
extern void test_multi_process_mutex();
extern void test_multi_process_shared_mutex();

} // namespace test
} // namespace interprocess
} // namespace mc

#endif // MC_INTERPROCESS_EXAMPLES_COMMON_H