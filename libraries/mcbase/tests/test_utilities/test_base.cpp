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

#include <cstdlib>
#include <limits.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#elif defined(_WIN32)
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#else
#include <unistd.h>
#endif

#include <mc/filesystem.h>
#include <mc/string.h>
#include <test_utilities/base.h>

namespace mc {
namespace test {

MC_API mc::string get_executable_path()
{
#ifdef __APPLE__
    char     path[PATH_MAX];
    uint32_t size = PATH_MAX;
    if (_NSGetExecutablePath(path, &size) == 0) {
        return mc::string(path);
    }
#elif defined(_WIN32)
    char  path[MAX_PATH];
    DWORD size = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (size > 0) {
        return mc::string(path, size);
    }
#else
    char    path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count > 0) {
        return mc::string(path, count);
    }
#endif
    return "";
}

MC_API mc::filesystem::path get_build_root()
{
    // 优先通过可执行文件路径推断
    auto exe_path_str = get_executable_path();
    if (!exe_path_str.empty()) {
        auto exe_path = mc::filesystem::path(exe_path_str.c_str());
        if (exe_path.is_absolute()) {
            auto exe_dir = exe_path.parent_path();
#if defined(__APPLE__)
            constexpr const char* k_mock_module_file = "mc.dylib";
#else
            constexpr const char* k_mock_module_file = "mc.so";
#endif
            // mcbase_test 与 mock：.../tests/mcbase_test 与 .../tests/module/mc.dylib
            if (mc::filesystem::exists(exe_dir / "module" / k_mock_module_file)) {
                return exe_dir.parent_path();
            }
            // 如果可执行文件在 tests 目录下，构建根目录是 tests 的父目录
            if (exe_dir.filename() == "tests") {
                auto build_root = exe_dir.parent_path();
                // 验证构建根目录是否包含 tests 目录
                if (mc::filesystem::exists(build_root / "tests")) {
                    return build_root;
                }
            }
        }
    }

    // 尝试使用环境变量
    const char* env_build_root = std::getenv("MC_BUILD_ROOT");
    if (env_build_root != nullptr) {
        mc::filesystem::path build_root(env_build_root);
        if (mc::filesystem::exists(build_root / "tests")) {
            return build_root;
        }
    }

    // 回退到当前路径
    return mc::filesystem::current_path();
}

} // namespace test
} // namespace mc
