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
 * @file multi_process_test_base.h
 * @brief 多进程测试基类，提供共享内存和进程管理的基础设施
 */
#pragma once

#include "common.h"

namespace mc {
namespace interprocess {
namespace test {

/**
 * @class multi_process_test_base
 * @brief 多进程测试基类，封装共享内存创建和进程管理的通用逻辑
 */
class multi_process_test_base {
public:
    /**
     * @brief 构造函数
     * @param test_name 测试名称，用于日志和共享内存名称
     * @param shm_size 共享内存大小，默认为256KB
     */
    multi_process_test_base(const std::string& test_name, size_t shm_size = 256 * 1024);

    /**
     * @brief 析构函数
     */
    virtual ~multi_process_test_base();

    /**
     * @brief 运行测试
     * @return 测试是否成功
     */
    bool run_test();

protected:
    /**
     * @brief 初始化测试环境，包括共享内存、锁和计数器
     * @return 初始化是否成功
     */
    virtual bool initialize() = 0;

    /**
     * @brief 创建子进程并执行测试
     * @return 创建子进程是否成功
     */
    virtual bool create_child_processes() = 0;

    /**
     * @brief 验证测试结果
     * @return 测试是否成功
     */
    virtual bool verify_results() = 0;

    /**
     * @brief 清理资源
     */
    virtual void cleanup() = 0;

    /**
     * @brief 等待所有子进程结束
     * @param timeout_seconds 超时时间（秒）
     * @return 所有子进程是否正常结束
     */
    bool wait_for_children(int timeout_seconds = 30);

    /**
     * @brief 强制终止所有子进程
     */
    void terminate_children();

protected:
    // 测试名称
    std::string m_test_name;
    
    // 共享内存大小
    size_t m_shm_size;
    
    // 共享内存管理器
    std::unique_ptr<shared_memory_manager> m_shm_manager;
    
    // 共享内存对象
    std::shared_ptr<shared_memory> m_shm;
    
    // 共享内存分配器引用
    shared_memory_allocator* m_allocator;
    
    // 子进程PID列表
    std::vector<pid_t> m_child_pids;
};

// 实现部分
multi_process_test_base::multi_process_test_base(const std::string& test_name, size_t shm_size)
    : m_test_name(test_name), m_shm_size(shm_size), m_shm(nullptr), m_allocator(nullptr) {
    
    ilog("===== ${test_name} 开始 =====", ("test_name", m_test_name));
}

multi_process_test_base::~multi_process_test_base() {
    // 清理可能残留的子进程
    terminate_children();
    
    // 共享内存管理器的析构函数会负责清理共享内存
    ilog("===== ${test_name} 结束 =====", ("test_name", m_test_name));
}

bool multi_process_test_base::run_test() {
    // 初始化测试环境
    if (!initialize()) {
        elog("测试初始化失败");
        return false;
    }
    
    // 创建子进程
    if (!create_child_processes()) {
        elog("创建子进程失败");
        return false;
    }
    
    // 等待子进程结束
    bool all_children_done = wait_for_children();
    
    // 验证结果
    bool result = verify_results();
    
    // 清理资源
    cleanup();
    
    return all_children_done && result;
}

bool multi_process_test_base::wait_for_children(int timeout_seconds) {
    ilog("等待所有子进程完成...");
    
    // 设置超时时间
    time_t start_time = time(NULL);
    bool all_children_done = false;
    
    while (!all_children_done && (time(NULL) - start_time < timeout_seconds)) {
        all_children_done = true;
        
        for (auto it = m_child_pids.begin(); it != m_child_pids.end();) {
            int status;
            pid_t result = waitpid(*it, &status, WNOHANG);
            
            if (result > 0) {
                // 子进程已结束
                if (WIFEXITED(status)) {
                    ilog("子进程(PID=${pid})正常退出，退出码: ${code}", 
                         ("pid", *it)("code", WEXITSTATUS(status)));
                } else if (WIFSIGNALED(status)) {
                    elog("子进程(PID=${pid})被信号${signal}终止", 
                         ("pid", *it)("signal", WTERMSIG(status)));
                }
                
                // 从列表中移除已结束的子进程
                it = m_child_pids.erase(it);
            } else if (result == 0) {
                // 子进程仍在运行
                all_children_done = false;
                ++it;
            } else {
                // waitpid出错
                elog("等待子进程${pid}时出错: ${error}", 
                     ("pid", *it)("error", strerror(errno)));
                ++it;
            }
        }
        
        if (!all_children_done) {
            // 短暂休眠避免CPU占用过高
            usleep(50000); // 50ms
        }
    }
    
    // 检查是否有子进程超时
    if (!m_child_pids.empty()) {
        elog("以下子进程超时未结束，将被强制终止:");
        terminate_children();
        return false;
    }
    
    return true;
}

void multi_process_test_base::terminate_children() {
    for (pid_t pid : m_child_pids) {
        elog("强制终止子进程 ${pid}", ("pid", pid));
        kill(pid, SIGTERM);
    }
    
    // 再等待一小段时间确保子进程终止
    sleep(1);
    for (pid_t pid : m_child_pids) {
        int status;
        waitpid(pid, &status, WNOHANG);
    }
    
    m_child_pids.clear();
}

} // namespace test
} // namespace interprocess
} // namespace mc 