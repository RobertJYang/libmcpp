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

#ifndef MC_TEST_UTILITIES_RESOURCE_MANAGER_H
#define MC_TEST_UTILITIES_RESOURCE_MANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace mc {
namespace test_utilities {

/**
 * @brief 测试资源管理类
 * 
 * 该类用于管理测试过程中创建的各种资源，确保测试结束后资源被正确清理。
 * 支持管理临时文件、共享内存和其他需要在测试后清理的资源。
 */
class resource_manager {
public:
    /**
     * @brief 构造函数
     * @param auto_cleanup 是否在析构时自动清理资源
     */
    resource_manager(bool auto_cleanup = true);
    
    /**
     * @brief 析构函数，根据设置自动清理资源
     */
    ~resource_manager();
    
    /**
     * @brief 添加临时文件路径，将在清理时删除
     * @param path 文件路径
     */
    void add_temp_file(const std::string& path);
    
    /**
     * @brief 创建临时文件并添加到清理列表
     * @param path 文件路径
     * @param content 文件内容，默认为空
     * @return 是否创建成功
     */
    bool create_temp_file(const std::string& path, const std::string& content = "");
    
    /**
     * @brief 添加自定义清理函数
     * @param cleanup_func 清理函数
     */
    void add_cleanup_function(std::function<void()> cleanup_func);
    
    /**
     * @brief 手动清理所有资源
     */
    void cleanup();
    
    /**
     * @brief 设置是否在析构时自动清理
     * @param auto_cleanup 是否自动清理
     */
    void set_auto_cleanup(bool auto_cleanup);
    
private:
    std::vector<std::string> m_temp_files;                // 临时文件列表
    std::vector<std::function<void()>> m_cleanup_funcs;   // 自定义清理函数列表
    bool m_auto_cleanup;                                  // 是否自动清理
};

} // namespace test_utilities
} // namespace mc

#endif // MC_TEST_UTILITIES_RESOURCE_MANAGER_H 