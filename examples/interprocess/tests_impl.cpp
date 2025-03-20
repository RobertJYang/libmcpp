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
 * @file tests_impl.cpp
 * @brief 使用测试基类实现的测试函数
 */
#include "shared_mutex_test.h"
#include "shared_rw_mutex_test.h"
#include "shared_memory_test.h"
#include "common.h"

namespace mc {
namespace interprocess {
namespace test {

// 测试多进程共享互斥锁（使用新的测试类）
void run_shared_mutex_test() {
    shared_mutex_test test(3, 10); // 3个进程，每个进程10次迭代
    bool result = test.run_test();
    if (result) {
        ilog("多进程共享互斥锁测试完成，测试结果：成功");
    } else {
        elog("多进程共享互斥锁测试完成，测试结果：失败");
    }
}

// 测试共享读写锁（使用新的测试类）
void run_shared_rw_mutex_test() {
    shared_rw_mutex_test test(3, 2, 10); // 3个读进程，2个写进程，每个进程10次迭代
    bool result = test.run_test();
    if (result) {
        ilog("共享读写锁测试完成，测试结果：成功");
    } else {
        elog("共享读写锁测试完成，测试结果：失败");
    }
}

// 测试多进程共享内存（使用新的测试类）
void run_shared_memory_test() {
    shared_memory_test test(3, 100); // 3个进程，每个进程100次增加计数器
    bool result = test.run_test();
    if (result) {
        ilog("多进程共享内存测试完成，测试结果：成功");
    } else {
        elog("多进程共享内存测试完成，测试结果：失败");
    }
}

} // namespace test
} // namespace interprocess
} // namespace mc

// 暴露给旧的接口，保持兼容
void test_multi_process_shared_mutex() {
    mc::interprocess::test::run_shared_mutex_test();
}

void test_shared_rw_mutex() {
    mc::interprocess::test::run_shared_rw_mutex_test();
}

void test_multi_process_shared_memory() {
    mc::interprocess::test::run_shared_memory_test();
} 