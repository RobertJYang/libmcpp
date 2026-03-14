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
#include <vector>
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

namespace mc {
namespace test {

MC_API std::string get_executable_path()
{
#ifdef __APPLE__
    char     path[PATH_MAX];
    uint32_t size = PATH_MAX;
    if (_NSGetExecutablePath(path, &size) == 0) {
        return std::string(path);
    }
#elif defined(_WIN32)
    char  path[MAX_PATH];
    DWORD size = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (size > 0) {
        return std::string(path, size);
    }
#else
    char    path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count > 0) {
        return std::string(path, count);
    }
#endif
    return "";
}

MC_API mc::filesystem::path get_build_root()
{
    // 优先通过可执行文件路径推断
    auto exe_path_str = get_executable_path();
    if (!exe_path_str.empty()) {
        auto exe_path = mc::filesystem::path(exe_path_str);
        if (exe_path.is_absolute()) {
            // 向上查找包含 tests 目录的构建根目录
            // 先收集所有可能的构建根目录候选
            std::vector<mc::filesystem::path> candidates;
            auto                              current = exe_path.parent_path();
            for (int i = 0; i < 10 && !current.empty() && current != current.root_path(); ++i) {
                // 检查当前目录是否包含 tests 目录
                if (mc::filesystem::exists(current / "tests")) {
                    auto tests_dir      = current / "tests";
                    auto dynamic_module = tests_dir / "libmc_test_dynamic_module.so";
                    // 如果找到动态模块文件，这是最可靠的构建目录标识
                    if (mc::filesystem::exists(dynamic_module)) {
                        return current;
                    }
                    // 记录候选目录
                    candidates.push_back(current);
                }
                current = current.parent_path();
            }

            // 如果没有找到包含动态模块的目录，从候选目录中选择最可能的
            for (const auto& candidate : candidates) {
                auto dir_name = candidate.filename().string();
                // 优先选择包含 build 相关关键词的目录
                if (dir_name.find("build") != std::string::npos || dir_name == "builddir" ||
                    dir_name.find("temp") != std::string::npos) {
                    return candidate;
                }
            }

            // 如果都没有，返回第一个候选（可能是构建目录）
            if (!candidates.empty()) {
                return candidates[0];
            }

            // 继续查找其他可能的路径模式
            current = exe_path.parent_path();
            for (int i = 0; i < 10 && !current.empty() && current != current.root_path(); ++i) {
                // 如果当前目录名是 tests，父目录可能是构建根目录
                if (current.filename() == "tests") {
                    auto build_root = current.parent_path();
                    if (mc::filesystem::exists(build_root / "tests")) {
                        return build_root;
                    }
                }
                // 如果当前目录名是 bin，可能是构建目录的 bin 子目录
                if (current.filename() == "bin") {
                    auto build_root = current.parent_path();
                    if (mc::filesystem::exists(build_root / "tests")) {
                        return build_root;
                    }
                }
                current = current.parent_path();
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

    // 尝试从当前工作目录向上查找
    auto current = mc::filesystem::current_path();
    for (int i = 0; i < 10 && !current.empty() && current != current.root_path(); ++i) {
        if (mc::filesystem::exists(current / "tests")) {
            // 优先验证是否有动态模块文件（构建目录的特征）
            auto dynamic_module = current / "tests" / "libmc_test_dynamic_module.so";
            if (mc::filesystem::exists(dynamic_module)) {
                return current;
            }
            // 检查目录名是否包含构建相关关键词
            auto dir_name = current.filename().string();
            if (dir_name.find("build") != std::string::npos || dir_name == "builddir" ||
                dir_name.find("temp") != std::string::npos) {
                return current;
            }
            // 即使没有动态模块，也返回（可能是构建目录）
            return current;
        }
        current = current.parent_path();
    }

    // 回退到当前路径
    return mc::filesystem::current_path();
}

} // namespace test
} // namespace mc
