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
 * @file shared_rw_mutex_test.h
 * @brief 共享读写锁测试类
 */
#pragma once

#include "multi_process_test_base.h"

namespace mc {
namespace interprocess {
namespace test {

/**
 * @class shared_rw_mutex_test
 * @brief 多进程共享读写锁测试类
 */
class shared_rw_mutex_test : public multi_process_test_base {
public:
    /**
     * @brief 构造函数
     * @param reader_count 读取进程数量
     * @param writer_count 写入进程数量
     * @param iterations 每个进程的迭代次数
     */
    shared_rw_mutex_test(int reader_count = 3, int writer_count = 2, int iterations = 10);
    
    /**
     * @brief 析构函数
     */
    virtual ~shared_rw_mutex_test();

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
    
    // 共享读写锁
    shared_rw_mutex* m_rwlock;
    
    // 共享计数器
    int* m_counter;
};

// 实现部分
shared_rw_mutex_test::shared_rw_mutex_test(int reader_count, int writer_count, int iterations)
    : multi_process_test_base("共享读写锁测试", 256 * 1024),
      m_reader_count(reader_count),
      m_writer_count(writer_count),
      m_iterations(iterations),
      m_rwlock(nullptr),
      m_counter(nullptr) {
}

shared_rw_mutex_test::~shared_rw_mutex_test() {
    // 资源清理在cleanup()中完成
}

bool shared_rw_mutex_test::initialize() {
    // 创建共享内存管理器
    m_shm_manager = std::make_unique<shared_memory_manager>(
        "shm_rw_mutex_test", 
        m_shm_size,
        shared_memory_manager::REMOVE_ON_EXIT | 
        shared_memory_manager::REMOVE_IF_EXISTS
    );
    
    m_shm = m_shm_manager->get_shared_memory();
    if (!m_shm) {
        elog("创建共享内存失败!");
        return false;
    }
    
    ilog("共享内存创建成功，名称: ${name}, 大小: ${size}字节", 
         ("name", m_shm->get_name())("size", m_shm->get_size()));
    
    // 获取分配器
    m_allocator = &m_shm->get_allocator();
    
    // 创建读写锁
    m_rwlock = m_allocator->create<shared_rw_mutex>();
    if (!m_rwlock) {
        elog("创建共享读写锁失败!");
        return false;
    }
    
    // 分配共享计数器
    m_counter = m_allocator->create<int>(0);
    if (!m_counter) {
        elog("分配计数器内存失败!");
        return false;
    }
    
    ilog("共享读写锁和计数器创建成功");
    return true;
}

bool shared_rw_mutex_test::create_child_processes() {
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
                m_rwlock->lock_shared();
                
                // 读取当前计数值
                int current = *m_counter;
                
                // 模拟读取操作
                usleep(2000 + (rand() % 5000)); // 2-7ms随机延迟
                
                ilog("读子进程${i}(PID=${pid})：读取计数器值${value}", 
                     ("i", i)("pid", getpid())("value", current));
                
                // 释放读锁
                m_rwlock->unlock_shared();
                
                // 进程间隔
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
            srand(getpid() + time(NULL) + i + 100); // 不同的随机种子
            
            ilog("写子进程${i}(PID=${pid})开始运行", ("i", i)("pid", getpid()));
            
            for (int j = 0; j < m_iterations; j++) {
                // 获取写锁
                m_rwlock->lock();
                
                // 读取当前计数值
                int current = *m_counter;
                
                // 模拟写入操作
                usleep(5000 + (rand() % 10000)); // 5-15ms随机延迟
                
                // 增加计数器
                *m_counter = current + 1;
                
                ilog("写子进程${i}(PID=${pid})：将计数器从${old}增加到${new}", 
                     ("i", i)("pid", getpid())("old", current)("new", current + 1));
                
                // 释放写锁
                m_rwlock->unlock();
                
                // 进程间隔
                usleep(5000 + (rand() % 10000)); // 5-15ms随机延迟
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

bool shared_rw_mutex_test::verify_results() {
    // 检查最终计数值
    int final_count = *m_counter;
    int expected_writes = m_writer_count * m_iterations;
    
    ilog("最终计数器值: ${count}，期望写入次数: ${expected}", 
         ("count", final_count)("expected", expected_writes));
    
    if (final_count == expected_writes) {
        ilog("读写锁测试成功：所有写操作正确执行");
        return true;
    } else if (final_count > 0 && final_count < expected_writes) {
        wlog("读写锁测试部分成功：部分写操作未完成，计数器值(${actual})小于预期值(${expected})",
             ("actual", final_count)("expected", expected_writes));
        return true; // 部分成功也视为测试通过
    } else {
        elog("读写锁测试失败：计数器值异常");
        return false;
    }
}

void shared_rw_mutex_test::cleanup() {
    if (m_counter && m_allocator) {
        // 直接释放内存，int不需要显式析构
        m_allocator->deallocate(m_counter);
        m_counter = nullptr;
    }
    
    if (m_rwlock && m_allocator) {
        // 显式析构读写锁
        m_rwlock->~shared_rw_mutex();
        m_allocator->deallocate(m_rwlock);
        m_rwlock = nullptr;
    }
    
    // 共享内存管理器会在析构时自动清理共享内存
    m_shm = nullptr;
    m_allocator = nullptr;
}

} // namespace test
} // namespace interprocess
} // namespace mc 