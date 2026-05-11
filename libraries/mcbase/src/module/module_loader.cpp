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

#include "module/include/module_loader.h"
#include <mc/log/log.h>
#include <mc/string_utils.h>

#include <dlfcn.h>
#include <limits.h>
#include <string>
#include <string_view>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

using split_iterator = mc::strings::split_iterator;

namespace {
mc::filesystem::path to_filesystem_path(mc::string_view path)
{
    return mc::filesystem::path(std::string(path.data(), path.size()));
}
} // namespace

#ifndef MC_MODULE_PATH_SEP
#define MC_MODULE_PATH_SEP ";"
#endif

#ifndef MC_MODULE_PATH
#if defined(__APPLE__)
#define MC_MODULE_PATH                                                                                                 \
    "./?.dylib;"                                                                                                       \
    "./?/init.dylib;"                                                                                                  \
    "./modules/?.dylib;"                                                                                               \
    "./modules/?/init.dylib;"
#else
#define MC_MODULE_PATH                                                                                                 \
    "./?.so;"                                                                                                          \
    "./?/init.so;"                                                                                                     \
    "./modules/?.so;"                                                                                                  \
    "./modules/?/init.so;"
#endif
#endif

namespace mc::module {

#if defined(__APPLE__)
static constexpr mc::string_view k_module_ext(".dylib", 6);
#else
static constexpr mc::string_view k_module_ext(".so", 3);
#endif

static mc::string get_executable_path()
{
    char path[PATH_MAX];
#ifdef __APPLE__
    uint32_t size = PATH_MAX;
    if (_NSGetExecutablePath(path, &size) == 0) {
        return mc::string(path);
    }
#else
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count > 0) {
        return mc::string(path, count);
    }
#endif
    return "";
}

static mc::string make_lib_name(mc::string_view module_name)
{
    mc::string lib_name;
    for (auto it = split_iterator(module_name, ".:"); it != split_iterator(); ++it) {
        if (!lib_name.empty()) {
            lib_name += "_";
        }
        lib_name.append(*it);
    }

    return lib_name;
}

static void unloadlib(void* lib)
{
    dlclose(lib);
}

static void* loadlib(mc::string_view path, bool glb = false)
{
    // dlopen 需要以空字符结尾的路径；mc::string_view 不保证 data()[size()] 之后可安全读取
    const std::string path_z(path.data(), path.size());
    return dlopen(path_z.c_str(), RTLD_NOW | (glb ? RTLD_GLOBAL : RTLD_LOCAL));
}

static void* sym(void* lib, mc::string_view sym_name)
{
    const std::string sym_z(sym_name.data(), sym_name.size());
    return dlsym(lib, sym_z.c_str());
}

static bool is_readable_impl(mc::string_view path)
{
    try {
        mc::filesystem::path fs_path = to_filesystem_path(path);
        return mc::filesystem::exists(fs_path) && mc::filesystem::is_regular_file(fs_path);
    } catch (const std::exception&) {
        return false;
    }
}

module_loader::module_loader()
{
    m_load_lib_func.unload      = unloadlib;
    m_load_lib_func.load        = loadlib;
    m_load_lib_func.sym         = sym;
    m_load_lib_func.is_readable = is_readable_impl;

    // 1. 从环境变量加载搜索路径（最高优先级）
    if (const char* env_path = std::getenv("MC_MODULE_PATH")) {
        add_load_paths(env_path);
    }

    // 2. 添加可执行文件相对路径
    try {
        mc::string      exe_str  = get_executable_path();
        mc::string_view exe_sv   = exe_str.view();
        auto            exe_path = to_filesystem_path(exe_sv);
        auto            exe_dir  = exe_path.parent_path();

        // 可执行文件目录下的模块路径
        const mc::string ext(k_module_ext);
        auto             join_exe = [&exe_dir](const mc::string& rel) {
            return (exe_dir / mc::to_std_string(rel)).string();
        };
        add_load_paths(join_exe(mc::string("?") + ext));
        add_load_paths(join_exe(mc::string("?/init") + ext));
        add_load_paths(join_exe(mc::string("modules/?") + ext));
        add_load_paths(join_exe(mc::string("modules/?/init") + ext));
        // mcbase 测试：mock 位于可执行文件同级的 module/（如 .../tests/module/mc.dylib）
        add_load_paths(join_exe(mc::string("module/?") + ext));
        add_load_paths(join_exe(mc::string("module/?/init") + ext));
    } catch (const std::exception& e) {
        wlog("get executable path failed: ${error}", ("error", e.what()));
    }

    // 3. 添加当前工作目录相对路径
    add_load_paths(MC_MODULE_PATH);
}

module_loader::~module_loader() = default;

void module_loader::add_load_paths(const mc::string& paths)
{
    for (auto it = split_iterator(paths.view(), MC_MODULE_PATH_SEP);
         it != split_iterator(); ++it) {
        auto path = mc::strings::trim(*it);
        if (!path.empty()) {
            add_load_path(path);
        }
    }
}

void module_loader::add_load_path(const mc::string& path)
{
    if (path.find('?') == mc::string::npos) {
        wlog("invalid module path: ${path}, need '?'", ("path", path));
        return;
    }

    auto it = std::find(m_search_paths.begin(), m_search_paths.end(), path);
    if (it == m_search_paths.end()) {
        dlog("add module search path: ${path}", ("path", path));
        m_search_paths.push_back(path);
    }
}

// 检查文件是否存在且可读
bool module_loader::is_readable(const fs::path& path) const
{
    try {
        return fs::exists(path) && fs::is_regular_file(path);
    } catch (const std::exception&) {
        return false;
    }
}

bool module_loader::load_path(const fs::path& lib_path, const mc::string& export_name, const mc::string& template_path,
                              load_callback callback) const
{
    // 构建完整路径
    mc::string actual_path = template_path;
    {
        const std::string lib_path_text = lib_path.string();
        actual_path = mc::strings::replace_all(actual_path.view(), "?", mc::string_view(lib_path_text.data(), lib_path_text.size()));
    }

    // 检查文件是否存在且可读
    if (!m_load_lib_func.is_readable(actual_path)) {
        return false;
    }

    dlog("try load module: ${path}", ("path", actual_path));

    // 加载动态库
    void* handle = m_load_lib_func.load(actual_path, false);
    if (!handle) {
        return false;
    }

    auto open_func = m_load_lib_func.sym(handle, "mc_open_" + export_name);
    if (!open_func) {
        m_load_lib_func.unload(handle);
        return false;
    }

    auto close_func = m_load_lib_func.sym(handle, "mc_close_" + export_name);
    if (!close_func) {
        m_load_lib_func.unload(handle);
        return false;
    }

    bool is_reused = false;
    try {
        auto info        = mc::make_shared<library_info>();
        info->path       = actual_path;
        info->handle     = handle;
        info->open_func  = reinterpret_cast<open_func_t>(open_func);
        info->close_func = reinterpret_cast<close_func_t>(close_func);
        if (callback(std::move(info), is_reused)) {
            dlog("load module success: ${path}", ("path", actual_path));
            if (is_reused) {
                // 如果模块是复用的，说明之前已经调用过 dlopen，这里需要 dlclose 减少本次的引用计数
                m_load_lib_func.unload(handle);
            }
            return true;
        }
    } catch (const std::exception& e) {
        elog("load module failed: ${path} - ${error}", ("path", actual_path)("error", e.what()));
    }

    // 如果加载失败，则卸载动态库
    m_load_lib_func.unload(handle);
    return false;
}

bool module_loader::load_module(mc::string_view module_name, load_callback callback) const
{
    fs::path   base_path;
    mc::string export_name = make_lib_name(module_name);

    dlog("start load module: ${name}, export function prefix: mc_*_${export_name}",
         ("name", module_name)("export_name", export_name));

    // 遍历所有可能的路径组合
    for (auto it = split_iterator(module_name, ".:"); it != split_iterator(); ++it) {
        base_path /= to_filesystem_path(*it);

        // 遍历所有搜索路径模板
        for (const auto& template_path : m_search_paths) {
            if (load_path(base_path, export_name, template_path, callback)) {
                return true;
            }
        }
    }

    wlog("not found module: ${name}", ("name", module_name));
    return false;
}

void module_loader::add_search_path(mc::string_view path)
{
    auto it = std::find(m_search_paths.begin(), m_search_paths.end(), path);
    if (it == m_search_paths.end()) {
        m_search_paths.push_back(mc::string(path));
    }
}

void module_loader::clear_search_paths()
{
    m_search_paths.clear();
}

const std::vector<mc::string>& module_loader::search_paths() const
{
    return m_search_paths;
}

void module_loader::set_load_lib_func(load_lib_func_t func)
{
    m_load_lib_func = func;
}

load_lib_func_t& module_loader::get_load_lib_func()
{
    return m_load_lib_func;
}

} // namespace mc::module
