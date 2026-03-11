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
 * @file test_shared_memory_long_running.cpp
 * @brief 共享内存长时间运行测试
 */
#include "common.h"

namespace mc {
namespace interprocess {
namespace test {

// 测试共享内存长时间运行
void test_shared_memory_long_running()
{
    ilog("===== 测试共享内存长时间运行 =====");

    // 创建共享内存管理器
    shared_memory_manager shm_manager("long_running_test", 1 * 1024 * 1024,
                                      shared_memory_manager::REMOVE_ON_EXIT); // 1MB

    auto shm = shm_manager.get_shared_memory();
    if (!shm) {
        elog("创建共享内存失败!");
        return;
    }

    // 获取分配器
    auto& allocator = shm->get_allocator();

    // 写入一些测试数据
    for (int i = 0; i < 5; i++) {
        std::string data = "测试数据 #" + std::to_string(i);
        write_to_shared_memory(allocator, data);
    }

    // 展示内存使用情况
    ilog("共享内存总大小: ${total}字节, 已分配: ${used}字节, 可用: ${available}字节",
         ("total", allocator.get_total_size())("used", allocator.get_allocated_size())("available", allocator.get_available_size()));

    // 运行一段时间，可以按Ctrl+C终止
    ilog("程序正在运行中，按Ctrl+C终止...");
    int counter = 0;
    while (g_running && counter < 60) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        counter++;
        if (counter % 5 == 0) {
            ilog("运行中... ${counter}秒已过", ("counter", counter));
        }
    }

    // shared_memory_manager会在析构时自动清理资源
}

} // namespace test
} // namespace interprocess
} // namespace mc
