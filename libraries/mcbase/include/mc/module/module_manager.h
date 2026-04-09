/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_MODULE_MANAGER_H
#define MC_MODULE_MANAGER_H

#include <mc/module/module_base.h>

namespace mc::module {
class module_manager_impl;

/**
 * @brief 模块管理器
 */
class MC_API module_manager {
public:
    static module_manager& instance();
    static void            reset_for_test();

    ~module_manager();

    /**
     * @brief 加载模块 - 供脚本的 require() 使用
     *
     * @param module_name 模块名称，如 "mc.devices"
     * @return 模块实例，失败返回 nullptr
     */
    module_ptr require(std::string_view module_name);

    /**
     * @brief 卸载模块
     *
     * @param module_name 模块名称，如 "mc.devices"
     * @note 如果模块是动态模块，卸载只是将内部模块引用计数减1，不会真正卸载动态库，动态库延迟到
     * 外部最后一个 module_ptr 被销毁时才会卸载
     */
    void unload(std::string_view module_name);

    /**
     * @brief 添加模块搜索路径 - 用于动态模块加载
     */
    void add_search_path(std::string_view path);

    /**
     * @brief 获取已加载的模块列表 - 供脚本查询
     */
    std::vector<std::string_view> loaded_modules() const;

    /**
     * @brief 检查模块是否已加载
     */
    bool is_loaded(std::string_view module_name) const;

private:
    module_manager();

    mc::shared_ptr<module_manager_impl> m_impl;
};

} // namespace mc::module

#endif // MC_MODULE_MANAGER_H