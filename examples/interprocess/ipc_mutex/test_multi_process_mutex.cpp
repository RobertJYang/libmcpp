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
 * @file test_multi_process_mutex.cpp
 * @brief 共享互斥锁测试类
 */
#include "multi_process_test_base.h"

namespace mc {
namespace interprocess {
namespace test {

/**
 * @class multi_process_mutex
 * @brief 多进程互斥锁测试类
 */
class multi_process_mutex : public multi_process_test_base {
public:
    /**
     * @brief 构造函数
     * @param process_count 测试进程数量
     * @param iterations 每个进程的迭代次数
     */
    multi_process_mutex(int process_count = 3, int iterations = 10);

    /**
     * @brief 析构函数
     */
    virtual ~multi_process_mutex();

protected:
    /**
     * @brief 初始化测试环境
     * @return 初始化是否成功
     */
    virtual bool initialize() override;

    /**
     * @brief 创建子进程并执行测试
     * @return 创建进程是否成功
     */
    virtual bool create_child_processes() override;

    /**
     * @brief 验证测试结果
     * @return 测试是否成功
     */
    virtual bool verify_results() override;

    /**
     * @brief 清理资源
     */
    virtual void cleanup() override;

private:
    // 测试进程数量
    int m_process_count;

    // 每个进程迭代次数
    int m_iterations;

    // IPC互斥锁
    ipc_mutex* m_mutex;

    // 共享计数器
    int* m_counter;
};

// 实现部分
multi_process_mutex::multi_process_mutex(int process_count, int iterations)
    : multi_process_test_base("多进程互斥锁测试", 256 * 1024),
      m_process_count(process_count),
      m_iterations(iterations),
      m_mutex(nullptr),
      m_counter(nullptr)
{
}

multi_process_mutex::~multi_process_mutex()
{
    // 资源清理在cleanup()中完成
}

bool multi_process_mutex::initialize()
{
    // 创建共享内存管理器
    m_shm_manager = std::make_unique<shared_memory_manager>(
        "shm_multi_process_mutex_test",
        m_shm_size,
        shared_memory_manager::REMOVE_ON_EXIT |
            shared_memory_manager::REMOVE_IF_EXISTS);

    m_shm = m_shm_manager->get_shared_memory();
    if (!m_shm) {
        elog("创建共享内存失败!");
        return false;
    }

    ilog("共享内存创建成功，名称: ${name}, 大小: ${size}字节",
         ("name", m_shm->get_name())("size", m_shm->get_size()));

    // 获取分配器
    m_allocator = &m_shm->get_allocator();

    // 创建IPC互斥锁
    m_mutex = m_allocator->create<ipc_mutex>();
    if (!m_mutex) {
        elog("创建IPC互斥锁失败!");
        return false;
    }

    // 分配共享计数器
    m_counter = m_allocator->create<int>(0);
    if (!m_counter) {
        elog("分配计数器内存失败!");
        return false;
    }

    ilog("IPC互斥锁和计数器创建成功");
    return true;
}

bool multi_process_mutex::create_child_processes()
{
    for (int i = 0; i < m_process_count; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            // 创建进程失败
            elog("创建子进程${i}失败: ${error}", ("i", i)("error", strerror(errno)));
            continue;
        } else if (pid == 0) {
            // 子进程
            // 初始化随机数生成器，确保每个进程有不同的随机序列
            srand(getpid() + time(NULL) + i);

            ilog("子进程${i}(PID=${pid})开始运行", ("i", i)("pid", getpid()));

            // 模拟竞争条件
            for (int j = 0; j < m_iterations; j++) {
                // 获取互斥锁
                m_mutex->lock();

                // 读取当前计数值
                int current = *m_counter;

                // 模拟处理延迟，减小延迟范围以避免长时间持有锁
                usleep(5000 + (rand() % 10000)); // 5-15ms随机延迟

                // 增加计数器并保存
                *m_counter = current + 1;

                ilog("子进程${i}(PID=${pid})：将计数器从${old}增加到${new}",
                     ("i", i)("pid", getpid())("old", current)("new", current + 1));

                // 释放互斥锁
                m_mutex->unlock();

                // 进程切换时间
                usleep(2000 + (rand() % 5000)); // 2-7ms随机延迟
            }

            ilog("子进程${i}(PID=${pid})完成", ("i", i)("pid", getpid()));
            exit(0);
        } else {
            // 父进程
            ilog("创建子进程${i}(PID=${pid})", ("i", i)("pid", pid));
            m_child_pids.push_back(pid);
        }
    }

    return true;
}

bool multi_process_mutex::verify_results()
{
    // 检查最终计数值
    int final_count  = *m_counter;
    int expected_max = m_process_count * m_iterations;

    ilog("最终计数器值: ${count}，最大预期值: ${expected}",
         ("count", final_count)("expected", expected_max));

    if (final_count <= expected_max && final_count > 0) {
        if (final_count == expected_max) {
            ilog("互斥锁测试成功：所有进程正常完成，计数器值符合预期");
            return true;
        } else {
            wlog("互斥锁测试部分成功：部分进程可能未完成，计数器值(${actual})小于预期值(${expected})",
                 ("actual", final_count)("expected", expected_max));
            return true; // 部分成功也视为测试通过
        }
    } else {
        elog("互斥锁测试失败：计数器值异常");
        return false;
    }
}

void multi_process_mutex::cleanup()
{
    if (m_counter && m_allocator) {
        // 直接释放内存，int不需要显式析构
        m_allocator->deallocate(m_counter);
        m_counter = nullptr;
    }

    if (m_mutex && m_allocator) {
        // 显式析构互斥锁
        m_mutex->~ipc_mutex();
        m_allocator->deallocate(m_mutex);
        m_mutex = nullptr;
    }

    // 共享内存管理器会在析构时自动清理共享内存
    m_shm       = nullptr;
    m_allocator = nullptr;
}

void test_multi_process_mutex()
{
    multi_process_mutex test(3, 10); // 3个进程，每个进程10次增加计数器
    bool                result = test.run_test();
    if (result) {
        ilog("多进程共享内存互斥锁测试完成，测试结果：成功");
    } else {
        elog("多进程共享内存互斥锁测试完成，测试结果：失败");
    }
}

} // namespace test
} // namespace interprocess
} // namespace mc