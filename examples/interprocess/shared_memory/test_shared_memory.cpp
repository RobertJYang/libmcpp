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
 * @file test_shared_memory.cpp
 * @brief 共享内存基本功能测试
 */
#include "common.h"

namespace mc {
namespace interprocess {
namespace test {

// 测试共享内存
void test_shared_memory() {
    ilog("===== 测试共享内存基本功能 =====");
    
    // 创建共享内存管理器，如果存在则先删除
    const std::string shm_name = "test_shared_memory";
    size_t shm_size = 256 * 1024; // 256KB
    
    // 使用选项组合：退出时自动删除 | 启动时如存在则删除
    shared_memory_manager shm_manager(shm_name, shm_size, 
                                     shared_memory_manager::REMOVE_ON_EXIT | 
                                     shared_memory_manager::REMOVE_IF_EXISTS);
    
    auto shm = shm_manager.get_shared_memory();
    if (!shm) {
        elog("创建共享内存失败!");
        return;
    }
    
    ilog("共享内存创建成功，名称: ${name}, 大小: ${size}字节", 
         ("name", shm->get_name())("size", shm->get_size()));
    
    // 获取分配器
    auto& allocator = shm->get_allocator();
    
    // 分配和写入一些数据
    std::string test_data = "Hello from shared memory!";
    write_to_shared_memory(allocator, test_data);
    
    // 展示内存使用情况
    ilog("共享内存总大小: ${total}字节, 已分配: ${used}字节, 可用: ${available}字节",
         ("total", allocator.get_total_size())
         ("used", allocator.get_allocated_size())
         ("available", allocator.get_available_size()));
    
    ilog("资源将在程序结束时自动清理");
} 

} // namespace test
} // namespace interprocess
} // namespace mc
