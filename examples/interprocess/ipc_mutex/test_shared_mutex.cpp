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
 * @brief 进程间共享读写锁测试
 */
#include "common.h"

namespace mc {
namespace interprocess {
namespace test {

// 测试进程间共享读写锁
void test_shared_mutex()
{
    ilog("===== 进程间共享读写锁测试 =====");

    // 创建共享内存管理器
    const std::string shm_name = "shm_shared_mutex_test_memory";
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

    // 创建共享读写锁
    ipc_shared_mutex* rw_mutex_ptr = allocator.create<ipc_shared_mutex>();
    ipc_shared_mutex& rw_mutex     = *rw_mutex_ptr;
    ilog("IPC共享读写锁创建成功");

    // 测试写锁（独占锁）
    ilog("尝试获取写锁...");
    rw_mutex.lock();
    ilog("写锁已获取");

    ilog("尝试释放写锁...");
    rw_mutex.unlock();
    ilog("写锁已释放");

    // 测试读锁（共享锁）
    ilog("尝试获取读锁...");
    rw_mutex.lock_shared();
    ilog("读锁已获取");

    // 测试多次读锁（从同一进程）
    ilog("尝试再次获取读锁（应该失败，因为同一进程不能重复加锁）...");
    bool second_read_lock = rw_mutex.try_lock_shared();
    ilog("再次获取读锁: ${result}", ("result", second_read_lock ? "成功" : "失败"));

    ilog("尝试释放读锁...");
    rw_mutex.unlock_shared();
    ilog("读锁已释放");

    // 测试读写互斥
    ilog("先获取读锁，再测试写锁...");
    rw_mutex.lock_shared();
    ilog("读锁已获取，尝试获取写锁（应该失败）...");
    bool write_while_read = rw_mutex.try_lock();
    ilog("写锁获取结果: ${result}", ("result", write_while_read ? "成功" : "失败"));
    rw_mutex.unlock_shared();

    // 测试写锁互斥性
    ilog("先获取写锁，再测试读锁...");
    rw_mutex.lock();
    ilog("写锁已获取，尝试获取读锁（应该失败）...");
    bool read_while_write = rw_mutex.try_lock_shared();
    ilog("读锁获取结果: ${result}", ("result", read_while_write ? "成功" : "失败"));
    rw_mutex.unlock();

    // 测试清理功能
    ilog("测试清理功能...");
    rw_mutex.cleanup_dead_locks();
    ilog("清理完成");

    // 显式析构
    rw_mutex.~ipc_shared_mutex();
    allocator.deallocate(rw_mutex_ptr);

    // shared_memory_manager会在析构时自动清理共享内存
    ilog("===== 进程间共享读写锁测试结束 =====");
}

} // namespace test
} // namespace interprocess
} // namespace mc
