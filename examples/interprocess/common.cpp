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
 * @file common.cpp
 * @brief 数据库组件测试程序的公共实现
 */
#include "common.h"

// 全局变量
std::atomic<bool> g_running{true};

// 信号处理函数，用于处理Ctrl+C
void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        g_running = false;
        ilog("接收到终止信号，正在清理资源...");
    }
}

// 模拟写入共享内存
void write_to_shared_memory(shared_memory_allocator& allocator, const std::string& data)
{
    const size_t buf_size = data.size() + 1;
    // 分配内存
    void* mem = allocator.allocate(buf_size);
    if (mem) {
        // 写入数据
        (void)memcpy_s(mem, buf_size, data.c_str(), buf_size);
        ilog("已写入数据到共享内存: ${data}", ("data", data));
    } else {
        elog("无法分配共享内存");
    }
}

// 模拟从共享内存读取
std::string read_from_shared_memory(shared_memory_allocator& allocator, void* ptr)
{
    if (ptr) {
        return std::string(static_cast<char*>(ptr));
    }
    return "";
}