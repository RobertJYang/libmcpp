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
 * @file test_multi_process_shared_mutex.cpp
 * @brief 共享读写锁测试类
 */
#include "multi_process_test_base.h"

namespace mc {
namespace interprocess {
namespace test {

/**
 * @class multi_process_shared_mutex
 * @brief 多进程共享读写锁测试类
 */
class multi_process_shared_mutex : public multi_process_test_base {
public:
    /**
     * @brief 构造函数
     * @param reader_count 读取进程数量
     * @param writer_count 写入进程数量
     * @param iterations 每个进程的迭代次数
     */
    multi_process_shared_mutex(int reader_count = 3, int writer_count = 2, int iterations = 10);

    /**
     * @brief 析构函数
     */
    virtual ~multi_process_shared_mutex();

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
    // 读取进程数量
    int m_reader_count;

    // 写入进程数量
    int m_writer_count;

    // 每个进程迭代次数
    int m_iterations;

    // IPC读写锁
    ipc_shared_mutex* m_rw_mutex;

    // 共享计数器
    int* m_data;

    // 读取进程数量
    int* m_read_count;
};

// 实现部分
multi_process_shared_mutex::multi_process_shared_mutex(int reader_count, int writer_count, int iterations)
    : multi_process_test_base("多进程共享读写锁测试", 256 * 1024), m_reader_count(reader_count),
      m_writer_count(writer_count), m_iterations(iterations), m_rw_mutex(nullptr), m_data(nullptr),
      m_read_count(nullptr)
{}

multi_process_shared_mutex::~multi_process_shared_mutex()
{
    // 资源清理在cleanup()中完成
}

bool multi_process_shared_mutex::initialize()
{
    // 创建共享内存管理器
    m_shm_manager = std::make_unique<shared_memory_manager>("shm_rw_mutex_test", m_shm_size,
                                                            shared_memory_manager::REMOVE_ON_EXIT |
                                                                shared_memory_manager::REMOVE_IF_EXISTS);

    m_shm = m_shm_manager->get_shared_memory();
    if (!m_shm) {
        elog("创建共享内存失败!");
        return false;
    }

    ilog("共享内存创建成功，名称: ${name}, 大小: ${size}字节", ("name", m_shm->get_name())("size", m_shm->get_size()));

    // 获取分配器
    m_allocator = &m_shm->get_allocator();

    // 创建IPC读写锁
    m_rw_mutex = m_allocator->create<ipc_shared_mutex>();
    if (!m_rw_mutex) {
        elog("创建IPC读写锁失败!");
        return false;
    }

    // 分配共享计数器
    m_data = m_allocator->create<int>(0);
    if (!m_data) {
        elog("分配计数器内存失败!");
        return false;
    }

    // 分配读取进程数量
    m_read_count = m_allocator->create<int>(0);
    if (!m_read_count) {
        elog("分配读取进程数量内存失败!");
        return false;
    }

    ilog("IPC读写锁和计数器创建成功");
    return true;
}

bool multi_process_shared_mutex::create_child_processes()
{
    // 先创建读进程
    for (int i = 0; i < m_reader_count; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            // 创建进程失败
            elog("创建读子进程${i}失败: ${error}", ("i", i)("error", strerror(errno)));
            continue;
        } else if (pid == 0) {
            // 读子进程
            srand(getpid() + time(NULL) + i);

            ilog("读子进程${i}(PID=${pid})开始运行", ("i", i)("pid", getpid()));

            for (int j = 0; j < m_iterations; j++) {
                // 获取读锁
                m_rw_mutex->lock_shared();

                // 读取当前计数值
                int current = *m_data;

                // 适当延迟以模拟读操作
                usleep(5000 + (rand() % 5000)); // 5-10ms随机延迟

                // 验证计数值没有被写者修改
                if (current != *m_data) {
                    elog("读者数据竞争：计数器值被写者修改: ${old} -> ${new}", ("old", current)("new", *m_data));
                }

                // 释放读锁
                m_rw_mutex->unlock_shared();

                // 进程切换时间
                usleep(1000 + (rand() % 3000)); // 1-4ms随机延迟
            }

            ilog("读子进程${i}(PID=${pid})完成", ("i", i)("pid", getpid()));
            exit(0);
        } else {
            // 父进程
            ilog("创建读子进程${i}(PID=${pid})", ("i", i)("pid", pid));
            m_child_pids.push_back(pid);
        }
    }

    // 再创建写进程
    for (int i = 0; i < m_writer_count; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            // 创建进程失败
            elog("创建写子进程${i}失败: ${error}", ("i", i)("error", strerror(errno)));
            continue;
        } else if (pid == 0) {
            // 写子进程
            srand(getpid() + time(NULL) + i + m_reader_count);

            ilog("写子进程${i}(PID=${pid})开始运行", ("i", i)("pid", getpid()));

            for (int j = 0; j < m_iterations; j++) {
                // 获取写锁
                m_rw_mutex->lock();

                // 读取当前计数值
                int current = *m_data;

                // 模拟写操作延迟
                usleep(8000 + (rand() % 7000)); // 8-15ms随机延迟

                // 递增计数器
                *m_data = current + 1;

                ilog("写子进程${i}(PID=${pid})：将计数器从${old}增加到${new}",
                     ("i", i)("pid", getpid())("old", current)("new", current + 1));

                // 释放写锁
                m_rw_mutex->unlock();

                // 进程切换时间
                usleep(3000 + (rand() % 5000)); // 3-8ms随机延迟
            }

            ilog("写子进程${i}(PID=${pid})完成", ("i", i)("pid", getpid()));
            exit(0);
        } else {
            // 父进程
            ilog("创建写子进程${i}(PID=${pid})", ("i", i)("pid", pid));
            m_child_pids.push_back(pid);
        }
    }

    return true;
}

bool multi_process_shared_mutex::verify_results()
{
    // 验证结果，计数器应等于写进程总共执行的次数
    int final_count = *m_data;
    int expected    = m_writer_count * m_iterations;

    ilog("最终计数器值: ${count}，预期值: ${expected}", ("count", final_count)("expected", expected));

    if (final_count == expected) {
        ilog("读写锁测试成功：所有写入正确完成");
        return true;
    } else {
        elog("读写锁测试失败：写入次数与预期不符");
        return false;
    }
}

void multi_process_shared_mutex::cleanup()
{
    if (m_data && m_allocator) {
        // 直接释放内存，int不需要显式析构
        m_allocator->deallocate(m_data);
        m_data = nullptr;
    }

    if (m_rw_mutex && m_allocator) {
        // 显式析构读写锁
        m_rw_mutex->~ipc_shared_mutex();
        m_allocator->deallocate(m_rw_mutex);
        m_rw_mutex = nullptr;
    }

    if (m_read_count && m_allocator) {
        // 直接释放内存，int不需要显式析构
        m_allocator->deallocate(m_read_count);
        m_read_count = nullptr;
    }

    // 共享内存管理器会在析构时自动清理共享内存
    m_shm       = nullptr;
    m_allocator = nullptr;
}

void test_multi_process_shared_mutex()
{
    multi_process_shared_mutex test(3, 2, 10); // 3个读进程，2个写进程，每个进程10次迭代
    bool                       result = test.run_test();
    if (result) {
        ilog("多进程共享内存读写锁测试完成，测试结果：成功");
    } else {
        elog("多进程共享内存读写锁测试完成，测试结果：失败");
    }
}

} // namespace test
} // namespace interprocess
} // namespace mc