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

#include "multi_process_test_base.h"
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <iostream>

namespace mc {
namespace interprocess {
namespace test {

multi_process_test_base::multi_process_test_base(const std::string& test_name, size_t shm_size)
    : m_test_name(test_name), m_shm_size(shm_size), m_shm(nullptr), m_allocator(nullptr)
{
    ilog("===== ${test_name} 开始 =====", ("test_name", m_test_name));
}

multi_process_test_base::~multi_process_test_base()
{
    // 清理可能残留的子进程
    terminate_children();

    // 共享内存管理器的析构函数会负责清理共享内存
    ilog("===== ${test_name} 结束 =====", ("test_name", m_test_name));
}

bool multi_process_test_base::run_test()
{
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

bool multi_process_test_base::wait_for_children(int timeout_seconds)
{
    ilog("等待所有子进程完成...");

    // 设置超时时间
    time_t start_time        = time(NULL);
    bool   all_children_done = false;

    while (!all_children_done && (time(NULL) - start_time < timeout_seconds)) {
        all_children_done = true;

        for (auto it = m_child_pids.begin(); it != m_child_pids.end();) {
            int   status;
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

void multi_process_test_base::terminate_children()
{
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

}  // namespace test
}  // namespace interprocess
}  // namespace mc 