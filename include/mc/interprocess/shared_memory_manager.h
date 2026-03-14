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

#ifndef MC_INTERPROCESS_SHARED_MEMORY_MANAGER_H
#define MC_INTERPROCESS_SHARED_MEMORY_MANAGER_H

#include <memory>
#include <string>

#include <mc/interprocess/shared_memory.h>

namespace mc {
namespace interprocess {

/**
 * @brief 共享内存管理器类
 *
 * 该类负责自动管理共享内存的生命周期。
 * 在析构时会自动释放共享内存资源。
 */
class MC_API shared_memory_manager {
public:
    /**
     * @brief 共享内存管理器选项常量
     */
    enum options {
        DEFAULT          = 0,      // 默认选项（无特殊行为）
        REMOVE_ON_EXIT   = 1 << 0, // 析构时自动删除共享内存
        REMOVE_IF_EXISTS = 1 << 1  // 启动时如果已存在则先删除
    };

    /**
     * @brief 构造函数
     * @param base_name 共享内存名称前缀
     * @param size 共享内存大小，如果小于最小值会使用最小值
     * @param opts 选项组合，可使用位或运算组合多个选项
     */
    shared_memory_manager(const std::string& base_name, size_t size = 0, uint32_t opts = REMOVE_ON_EXIT);

    /**
     * @brief 析构函数，自动清理资源
     */
    ~shared_memory_manager();

    /**
     * @brief 获取共享内存对象
     * @return 共享内存对象的智能指针
     */
    std::shared_ptr<shared_memory> get_shared_memory() const;

    /**
     * @brief 获取共享内存名称
     * @return 共享内存名称
     */
    std::string get_name() const;

    /**
     * @brief 手动清理共享内存资源
     */
    void cleanup();

    /**
     * @brief 设置退出时是否自动清理
     * @param remove_on_exit 是否自动清理
     */
    void set_remove_on_exit(bool remove_on_exit);

    /**
     * @brief 尝试移除指定名称的共享内存
     * @param name 共享内存名称
     * @return 是否成功移除
     */
    static bool remove_shared_memory(const std::string& name);

    /**
     * @brief 格式化共享内存名称，确保名称带有"/"前缀
     * @param name 原始共享内存名称
     * @return 格式化后的共享内存名称
     */
    static std::string format_shm_name(const std::string& name);

private:
    std::string                    m_name;           // 共享内存名称
    std::shared_ptr<shared_memory> m_shared_memory;  // 共享内存对象
    bool                           m_remove_on_exit; // 是否在析构时自动清理
};

} // namespace interprocess
} // namespace mc

#endif // MC_INTERPROCESS_SHARED_MEMORY_MANAGER_H