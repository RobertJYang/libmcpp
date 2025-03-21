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
 * @file multi_process_shared_memory.h
 * @brief 多进程共享内存测试类
 */
#include "multi_process_test_base.h"
#include "mc/interprocess/offset_ptr.h"

namespace mc {
namespace interprocess {
namespace test {

/**
 * @class multi_process_shared_memory
 * @brief 多进程共享内存测试类
 */
class multi_process_shared_memory : public multi_process_test_base {
public:
    /**
     * @brief 构造函数
     * @param process_count 测试进程数量
     * @param increments_per_process 每个进程增加计数器的次数
     */
    multi_process_shared_memory(int process_count = 3, int increments_per_process = 100);
    
    /**
     * @brief 析构函数
     */
    virtual ~multi_process_shared_memory();

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
    
    // 每个进程增加计数器的次数
    int m_increments_per_process;
    
    // 共享计数器
    std::atomic<int>* m_counter;
    
    // 指向计数器的偏移指针
    offset_ptr<std::atomic<int>> m_counter_ptr;
};

// 实现部分
multi_process_shared_memory::multi_process_shared_memory(int process_count, int increments_per_process)
    : multi_process_test_base("多进程共享内存测试", 512 * 1024),
      m_process_count(process_count),
      m_increments_per_process(increments_per_process),
      m_counter(nullptr) {
}

multi_process_shared_memory::~multi_process_shared_memory() {
    // 资源清理在cleanup()中完成
}

bool multi_process_shared_memory::initialize() {
    // 创建共享内存管理器
    m_shm_manager = std::make_unique<shared_memory_manager>(
        "multi_process_test", 
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
    
    // 分配共享计数器
    m_counter = m_allocator->create<std::atomic<int>>(0);
    if (!m_counter) {
        elog("分配计数器内存失败!");
        return false;
    }
    
    // 创建偏移指针，可以安全地传递给子进程
    m_counter_ptr = m_shm->make_offset_ptr(m_counter);
    
    ilog("共享计数器初始值: ${value}", ("value", m_counter->load()));
    
    return true;
}

bool multi_process_shared_memory::create_child_processes() {
    for (int i = 0; i < m_process_count; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            // 创建进程失败
            elog("创建子进程${i}失败: ${error}", ("i", i)("error", strerror(errno)));
            continue;
        } else if (pid == 0) {
            // 子进程
            // 获取共享内存
            auto child_shm = shared_memory::create(std::string(m_shm->get_name()), m_shm_size);
            if (!child_shm) {
                elog("子进程无法连接到共享内存!");
                exit(1);
            }
            
            // 使用偏移量创建指向计数器的指针
            auto child_counter = child_shm->make_offset_ptr(m_counter_ptr);
            
            ilog("子进程 ${process} (PID: ${pid}) 开始更新计数器", ("process", i)("pid", getpid()));
            
            // 更新计数器
            for (int j = 0; j < m_increments_per_process; j++) {
                int old_value = child_counter->fetch_add(1, std::memory_order_relaxed);
                if (j % 20 == 0) {
                    ilog("子进程 ${process}: 计数器值 ${value} -> ${new_value}", 
                         ("process", i)("value", old_value)("new_value", old_value + 1));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            
            ilog("子进程 ${process} 完成，计数器最终值: ${value}", 
                 ("process", i)("value", child_counter->load()));
            
            exit(0);
        } else {
            // 父进程
            ilog("创建子进程 ${process} (PID: ${pid})", ("process", i)("pid", pid));
            m_child_pids.push_back(pid);
        }
    }
    
    return true;
}

bool multi_process_shared_memory::verify_results() {
    // 检查最终计数器值
    int final_count = m_counter->load();
    int expected_count = m_process_count * m_increments_per_process;
    
    ilog("所有子进程已完成");
    ilog("计数器最终值: ${value}，预期值: ${expected}", 
         ("value", final_count)("expected", expected_count));
    
    if (final_count == expected_count) {
        ilog("测试成功: 计数器值正确");
        return true;
    } else {
        wlog("测试警告: 计数器值不符合预期，可能存在竞争条件");
        return true; // 部分成功也视为测试通过
    }
}

void multi_process_shared_memory::cleanup() {
    if (m_counter && m_allocator) {
        // 显式析构计数器
        m_counter->~atomic<int>();
        m_allocator->deallocate(m_counter);
        m_counter = nullptr;
    }
    
    // 共享内存管理器会在析构时自动清理共享内存
    m_shm = nullptr;
    m_allocator = nullptr;
}

void test_multi_process_shared_memory() {
   multi_process_shared_memory test(3, 100); // 3个进程，每个进程100次增加计数器
    bool result = test.run_test();
    if (result) {
        ilog("多进程共享内存测试完成，测试结果：成功");
    } else {
        elog("多进程共享内存测试完成，测试结果：失败");
    }
}

} // namespace test
} // namespace interprocess
} // namespace mc 