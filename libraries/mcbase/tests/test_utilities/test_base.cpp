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

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <optional>
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
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <mc/filesystem.h>
#include <mc/string.h>
#include <test_utilities/base.h>

namespace mc {
namespace test {

namespace {

mc::filesystem::path resolve_default_test_temp_root()
{
    return get_build_root() / "tmp";
}

mc::filesystem::path resolve_short_test_temp_root()
{
#ifdef _WIN32
    return mc::filesystem::temp_directory_path() / "mc_test_tmp";
#else
    const mc::filesystem::path tmp_root("/tmp");
    if (mc::filesystem::exists(tmp_root)) {
        return tmp_root / "mc_test_tmp";
    }
    return mc::filesystem::temp_directory_path() / "mc_test_tmp";
#endif
}

bool ensure_directory_exists(const mc::filesystem::path& dir)
{
    return mc::filesystem::exists(dir) || mc::filesystem::create_directories(dir);
}

bool path_fits_limit(const mc::filesystem::path& dir, const test_directory_options& options)
{
    if (options.max_path_length == 0) {
        return true;
    }

    for (const auto& child : options.required_children) {
        const auto candidate = dir / child;
        if (candidate.string().size() >= options.max_path_length) {
            return false;
        }
    }
    return true;
}

std::optional<mc::filesystem::path> create_unique_directory(const mc::filesystem::path& parent, mc::string_view prefix)
{
    if (!ensure_directory_exists(parent)) {
        return std::nullopt;
    }

#ifdef _WIN32
    char        temp_file[MAX_PATH];
    const auto  parent_string = std::string(parent.string());
    const auto  prefix_string = std::string(prefix);
    const char* temp_prefix   = prefix_string.empty() ? "mct" : prefix_string.c_str();

    if (GetTempFileNameA(parent_string.c_str(), temp_prefix, 0, temp_file) == 0) {
        return std::nullopt;
    }

    DeleteFileA(temp_file);
    mc::filesystem::path created_dir(temp_file);
    if (!mc::filesystem::create_directories(created_dir)) {
        return std::nullopt;
    }
    return created_dir;
#else
    mc::string        template_path = (parent / mc::filesystem::path(mc::string(prefix) + "XXXXXX")).string();
    std::vector<char> buffer(template_path.begin(), template_path.end());
    buffer.push_back('\0');

    if (mkdtemp(buffer.data()) == nullptr) {
        return std::nullopt;
    }

    return mc::filesystem::path(buffer.data());
#endif
}

} // namespace

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

scoped_test_directory::scoped_test_directory(const test_directory_options& options)
{
    create(options);
}

scoped_test_directory::~scoped_test_directory()
{
    reset();
}

scoped_test_directory::scoped_test_directory(scoped_test_directory&& other) noexcept
    : m_path(std::move(other.m_path)), m_using_fallback_root(other.m_using_fallback_root)
{
    other.m_path.clear();
    other.m_using_fallback_root = false;
}

scoped_test_directory& scoped_test_directory::operator=(scoped_test_directory&& other) noexcept
{
    if (this != &other) {
        reset();
        m_path                = std::move(other.m_path);
        m_using_fallback_root = other.m_using_fallback_root;
        other.m_path.clear();
        other.m_using_fallback_root = false;
    }
    return *this;
}

bool scoped_test_directory::create(const test_directory_options& options)
{
    reset();

    const auto preferred_root = options.preferred_root.empty() ? get_default_test_temp_root() : options.preferred_root;
    const auto fallback_root  = options.fallback_root.empty() ? get_short_test_temp_root() : options.fallback_root;

    auto preferred_path = create_unique_directory(preferred_root, options.preferred_prefix);
    if (preferred_path && path_fits_limit(*preferred_path, options)) {
        m_path                = *preferred_path;
        m_using_fallback_root = false;
        return true;
    }

    if (preferred_path) {
        uint64_t removed_count = 0;
        mc::filesystem::remove_all(*preferred_path, removed_count);
    }

    auto fallback_path = create_unique_directory(fallback_root, options.fallback_prefix);
    if (!fallback_path || !path_fits_limit(*fallback_path, options)) {
        if (fallback_path) {
            uint64_t removed_count = 0;
            mc::filesystem::remove_all(*fallback_path, removed_count);
        }
        return false;
    }

    m_path                = *fallback_path;
    m_using_fallback_root = true;
    return true;
}

void scoped_test_directory::reset() noexcept
{
    if (m_path.empty()) {
        return;
    }

    uint64_t removed_count = 0;
    mc::filesystem::remove_all(m_path, removed_count);
    m_path.clear();
    m_using_fallback_root = false;
}

bool scoped_test_directory::empty() const noexcept
{
    return m_path.empty();
}

const mc::filesystem::path& scoped_test_directory::path() const noexcept
{
    return m_path;
}

mc::filesystem::path scoped_test_directory::child_path(mc::string_view name) const
{
    return m_path / mc::filesystem::path(name);
}

bool scoped_test_directory::using_fallback_root() const noexcept
{
    return m_using_fallback_root;
}

MC_API mc::filesystem::path get_default_test_temp_root()
{
    return resolve_default_test_temp_root();
}

MC_API mc::filesystem::path get_short_test_temp_root()
{
    return resolve_short_test_temp_root();
}

MC_API scoped_test_directory make_test_directory(const test_directory_options& options)
{
    return scoped_test_directory(options);
}

} // namespace test
} // namespace mc
