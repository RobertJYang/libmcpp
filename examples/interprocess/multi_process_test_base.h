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
#ifndef MULTI_PROCESS_TEST_BASE_H
#define MULTI_PROCESS_TEST_BASE_H

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

} // namespace test
} // namespace interprocess
} // namespace mc

#endif // MULTI_PROCESS_TEST_BASE_H
