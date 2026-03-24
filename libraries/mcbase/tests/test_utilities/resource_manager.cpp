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

#include "test_utilities/resource_manager.h"
#include "mc/log/log.h"

#include <cstring>
#include <fstream>
#include <system_error>
#include <unistd.h>

namespace mc {
namespace test_utilities {

resource_manager::resource_manager(bool auto_cleanup) : m_auto_cleanup(auto_cleanup) {
    ilog("测试资源管理器初始化，自动清理: ${auto_cleanup}", ("auto_cleanup", auto_cleanup));
}

resource_manager::~resource_manager() {
    if (m_auto_cleanup) {
        cleanup();
    }
}

void resource_manager::add_temp_file(const mc::string& path) {
    if (!path.empty()) {
        m_temp_files.push_back(path);
        ilog("添加临时文件到清理列表: ${file}", ("file", path));
    }
}

bool resource_manager::create_temp_file(const mc::string& path, const mc::string& content) {
    try {
        std::ofstream file(path.c_str());
        if (!file) {
            elog("无法创建临时文件: ${path}, 错误: ${error}",
                 ("path", path)("error", std::strerror(errno)));
            return false;
        }

        if (!content.empty()) {
            file << content;
        }

        file.close();

        // 添加到清理列表
        add_temp_file(path);

        ilog("创建临时文件成功: ${path}", ("path", path));
        return true;
    } catch (const std::exception& e) {
        elog("创建临时文件异常: ${path}, 错误: ${error}", ("path", path)("error", e.what()));
        return false;
    }
}

void resource_manager::add_cleanup_function(std::function<void()> cleanup_func) {
    if (cleanup_func) {
        m_cleanup_funcs.push_back(std::move(cleanup_func));
        ilog("添加自定义清理函数到资源管理器");
    }
}

void resource_manager::cleanup() {
    // 执行所有自定义清理函数
    for (auto it = m_cleanup_funcs.rbegin(); it != m_cleanup_funcs.rend(); ++it) {
        try {
            (*it)();
        } catch (const std::exception& e) {
            wlog("执行清理函数时发生异常: ${error}", ("error", e.what()));
        }
    }
    m_cleanup_funcs.clear();

    // 删除所有临时文件
    for (const auto& file : m_temp_files) {
        if (unlink(file.c_str()) == 0) {
            ilog("临时文件已删除: ${file}", ("file", file));
        } else if (errno != ENOENT) { // 忽略文件不存在的错误
            wlog("删除临时文件失败: ${file}, 错误: ${error}",
                 ("file", file)("error", std::strerror(errno)));
        }
    }
    m_temp_files.clear();

    ilog("资源管理器清理完成");
}

void resource_manager::set_auto_cleanup(bool auto_cleanup) {
    m_auto_cleanup = auto_cleanup;
    ilog("设置资源管理器自动清理: ${auto_cleanup}", ("auto_cleanup", auto_cleanup));
}

} // namespace test_utilities
} // namespace mc