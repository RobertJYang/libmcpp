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

#ifndef MC_MODULE_LOADER_H
#define MC_MODULE_LOADER_H

#include <mc/filesystem.h>
#include <mc/memory.h>

#include <functional>
#include <string>
#include <vector>

namespace mc {
namespace reflect {
class reflection_factory;
} // namespace reflect
} // namespace mc

namespace mc::module {
namespace fs = mc::filesystem;

using open_func_t  = mc::reflect::reflection_factory* (*)();
using close_func_t = void (*)();
struct library_info : mc::shared_base {
    std::string                      module_name;         // 动态库文件名
    std::string                      path;                // 动态库文件路径
    void*                            handle{nullptr};     // 动态库句柄
    open_func_t                      open_func{nullptr};  // 导出函数名
    close_func_t                     close_func{nullptr}; // 清理函数名
    mc::reflect::reflection_factory* factory{nullptr};    // 工厂指针
};
using library_info_ptr = mc::shared_ptr<library_info>;

using unload_func_t      = void (*)(void*);
using load_func_t        = void* (*)(std::string_view, bool glb);
using sym_func_t         = void* (*)(void*, std::string_view);
using is_readable_func_t = bool (*)(std::string_view);
struct load_lib_func_t {
    unload_func_t      unload;
    load_func_t        load;
    sym_func_t         sym;
    is_readable_func_t is_readable;
};

/**
 * @brief 模块路径查找器
 *
 * 负责根据模块名称查找对应的动态库文件路径。
 * 搜索路径优先级：
 * 1. MC_MODULE_PATH环境变量指定的路径
 * 2. 默认搜索路径：
 *    - ./?.so
 *    - ./?/init.so
 *    - ./modules/?.so
 *    - ./modules/?/init.so
 *
 * 模块名到路径的转换规则：
 * 1. 将模块名中的点号转换为路径分隔符，如：
 *    mc.test.modules.db -> mc/test/modules/db.so
 * 2. 将最后一个组件作为库名，如：
 *    mc.test.modules.db -> mc/test/modules_db.so
 * 3. 将倒数第二个组件作为库名，如：
 *    mc.test.modules.db -> mc/test_modules_db.so
 *
 * 打开动态库后，会检查导出函数 mc_open_<module_name> 和 mc_close_<module_name>，
 * 其中 module_name 是模块名空间将 . 换成 _ 的名字，例如：
 * 模块名称为 "mc.devices"，那导出函数名称为 "mc_open_mc_devices" 和 "mc_close_mc_devices"
 * 模块名称为 "mc.test.module"，那导出函数名称为 "mc_open_mc_test_module" 和 "mc_close_mc_test_module"
 */
class MC_API module_loader {
public:
    module_loader();
    ~module_loader();

    using load_callback = std::function<bool(library_info_ptr, bool& is_resuse)>;
    bool load_module(std::string_view module_name, load_callback callback) const;

    // 添加自定义搜索路径
    void add_search_path(std::string_view path);

    // 清除所有搜索路径
    void clear_search_paths();

    // 获取当前的搜索路径列表
    const std::vector<std::string>& search_paths() const;

    // 设置加载库函数，默认使用 dlopen/dlsym/dlclose
    void             set_load_lib_func(load_lib_func_t func);
    load_lib_func_t& get_load_lib_func();

private:
    void add_load_paths(const std::string& paths);
    void add_load_path(const std::string& path);
    bool is_readable(const mc::filesystem::path& path) const;

    bool load_path(const fs::path&    lib_path,
                   const std::string& lib_name,
                   const std::string& template_path,
                   load_callback      callback) const;

    std::vector<std::string> m_search_paths;
    load_lib_func_t          m_load_lib_func;
};

} // namespace mc::module

#endif // MC_MODULE_LOADER_H