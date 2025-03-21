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
 * @file test_shared_mutex.cpp
 * @brief 共享互斥锁测试
 */
#include "common.h"

// 测试共享互斥锁
void test_shared_mutex() {
    ilog("===== 共享互斥锁测试 =====");

    // 创建共享内存管理器
    const std::string shm_name = "shm_mutex_test_memory";
    size_t            shm_size = 256 * 1024; // 256KB

    // 使用shared_memory_manager管理共享内存
    shared_memory_manager shm_manager(shm_name, shm_size,
                                      shared_memory_manager::REMOVE_ON_EXIT);

    auto shm = shm_manager.get_shared_memory();
    if (!shm) {
        elog("创建共享内存失败!");
        return;
    }

    ilog("共享内存创建成功，名称: ${name}, 大小: ${size}字节",
         ("name", shm->get_name())("size", shm->get_size()));

    // 获取分配器
    auto& allocator = shm->get_allocator();

    // 创建IPC互斥锁
    ipc_mutex* mutex_ptr = allocator.create<ipc_mutex>();
    ipc_mutex& mutex = *mutex_ptr;
    ilog("IPC互斥锁创建成功");

    // 测试锁定和解锁
    ilog("尝试锁定互斥锁...");
    mutex.lock();
    ilog("互斥锁已锁定");

    ilog("尝试解锁互斥锁...");
    mutex.unlock();
    ilog("互斥锁已解锁");

    // 测试多次锁定和解锁
    for (int i = 0; i < 5; i++) {
        ilog("循环 ${i} - 锁定", ("i", i));
        mutex.lock();
        ilog("循环 ${i} - 解锁", ("i", i));
        mutex.unlock();
    }

    // 测试try_lock功能
    ilog("测试try_lock（应该成功）...");
    bool try_result = mutex.try_lock();
    ilog("try_lock() 返回: ${result}", ("result", try_result ? "成功" : "失败"));
    if (try_result) {
        ilog("解锁互斥锁...");
        mutex.unlock();
    }

    // 测试带超时的锁定
    ilog("测试带超时的锁定（应该成功）...");
    bool lock_success = mutex.try_lock_for(std::chrono::milliseconds(100));
    ilog("try_lock_for(100ms) 返回: ${result}", ("result", lock_success ? "成功" : "超时"));
    if (lock_success) {
        mutex.unlock();
    }

    // 显式析构
    mutex.~ipc_mutex();
    allocator.deallocate(mutex_ptr);

    // shared_memory_manager会在析构时自动清理共享内存
    ilog("===== 共享互斥锁测试结束 =====");
}