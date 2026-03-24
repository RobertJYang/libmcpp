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

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <mc/filesystem.h>
#include <sstream>
#include <stdexcept>

namespace mc {
namespace filesystem {

// 获取路径中的文件名部分
mc::string basename(const fs::path& path)
{
    if (path.empty()) {
        return "";
    }

    auto native_name = fs::path(path).filename().string();
    return mc::string(std::move(native_name));
}

// 获取路径中的目录部分
mc::string dirname(const fs::path& path)
{
    if (path.empty()) {
        return ".";
    }

    auto parent = fs::path(path).parent_path().string();
    return parent.empty() ? mc::string(".") : mc::string(std::move(parent));
}

// 获取文件扩展名，不包含点号
mc::string extension(const fs::path& path)
{
    if (path.empty()) {
        return "";
    }

    // 获取扩展名（包括点）
    mc::string ext = mc::string(fs::path(path).extension().string());
    // 移除开头的点
    if (!ext.empty() && ext.view()[0] == '.') {
        return ext.substr(1);
    }

    return ext;
}

// 获取文件名主干部分（不含扩展名）
mc::string stem(const fs::path& path)
{
    if (path.empty()) {
        return "";
    }

    // 获取文件名主干部分（不含扩展名）
    auto native_stem = fs::path(path).stem().string();
    return mc::string(std::move(native_stem));
}

// 检查文件或目录是否存在
bool exists(const fs::path& p)
{
    try {
        return fs::exists(p);
    } catch (const std::exception& e) {
        return false;
    }
}

bool is_regular_file(const fs::path& p)
{
    try {
        return fs::is_regular_file(p);
    } catch (const std::exception& e) {
        return false;
    }
}

bool is_directory(const fs::path& p)
{
    try {
        return fs::is_directory(p);
    } catch (const std::exception& e) {
        return false;
    }
}

std::optional<uint64_t> file_size(const fs::path& p)
{
    try {
        return static_cast<uint64_t>(fs::file_size(p));
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

// 列出目录中的所有项目（文件和子目录）
std::vector<fs::path> list_directory(const fs::path& path)
{
    std::vector<fs::path> result;

    if (!fs::is_directory(path)) {
        return result;
    }

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        if (!ec) {
            result.push_back(entry.path());
        }
    }

    return result;
}

// 列出目录中的所有普通文件
std::vector<path> list_files(const path& path)
{
    std::vector<fs::path> result;

    if (!fs::is_directory(path)) {
        return result;
    }

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        if (!ec && entry.status().type() == fs::file_type::regular && !ec) {
            result.push_back(entry.path());
        }
    }

    return result;
}

// 列出目录中的所有子目录
std::vector<path> list_directories(const path& path)
{
    std::vector<fs::path> result;

    if (!fs::is_directory(path)) {
        return result;
    }

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        if (!ec && entry.status().type() == fs::file_type::directory && !ec) {
            result.push_back(entry.path());
        }
    }

    return result;
}

bool create_directory(const path& p)
{
    try {
        return fs::create_directory(p);
    } catch (const std::exception& e) {
        return false;
    }
}

bool create_directories(const path& p)
{
    try {
        return fs::create_directories(p);
    } catch (const std::exception& e) {
        return false;
    }
}

bool remove(const path& p)
{
    try {
        return fs::remove(p);
    } catch (const std::exception& e) {
        return false;
    }
}

std::optional<uint64_t> remove_all(const path& p)
{
    try {
        return static_cast<uint64_t>(fs::remove_all(p));
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

bool copy_file(const path& from, const path& to, bool overwrite)
{
    try {
        fs::copy_options options = overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none;
        fs::copy_file(from, to, options);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

void rename(const path& from, const path& to)
{
    fs::rename(from, to);
}

bool create_symlink(const path& target, const path& link)
{
    try {
        fs::create_symlink(target, link);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

std::optional<fs::path> read_symlink(const path& path)
{
    try {
        return fs::read_symlink(path);
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

path current_path()
{
    return fs::current_path();
}

void current_path(const path& p)
{
    fs::current_path(p);
}

path absolute(const path& p)
{
    try {
        return fs::absolute(p);
    } catch (const std::exception& e) {
        return p;
    }
}
#if defined(MC_HAS_STD_FILESYSTEM)
fs::path normalize(const fs::path& path)
{
    if (path.empty()) {
        return ".";
    }
    return path.lexically_normal();
}
#else
fs::path normalize(const fs::path& path)
{
    if (path.empty()) {
        return ".";
    }

    // 自定义路径规范化实现，用于不支持lexically_normal的编译器
    fs::path result;
    bool     is_absolute        = path.is_absolute();
    bool     has_root_name      = !path.root_name().empty();
    bool     has_root_directory = !path.root_directory().empty();

    // 保留根部分
    if (has_root_name) {
        result /= path.root_name();
    }

    if (has_root_directory) {
        result /= path.root_directory();
    }

    std::vector<fs::path> elements;

    // 处理每个路径片段
    for (const auto& part : path) {
        mc::string part_str = part.string();
        if (part_str == ".") {
            // 跳过当前目录
            continue;
        } else if (part_str == "..") {
            // 处理父目录
            if (!elements.empty() && elements.back().string() != "..") {
                elements.pop_back();
            } else if (!is_absolute) {
                // 相对路径中保留前导的".."
                elements.push_back(part);
            }
            // 绝对路径中忽略多余的".."
        } else if (!part_str.empty() && part_str != path.root_name().string() &&
                   part_str != path.root_directory().string()) {
            // 添加有效路径片段
            elements.push_back(part);
        }
    }

    // 构建最终路径
    for (const auto& element : elements) {
        result /= element;
    }

    // 处理空路径的情况
    if (result.empty()) {
        return ".";
    }

    return result;
}
#endif
path join(const path& base, const path& p)
{
    return base / p;
}

path join(const std::vector<path>& paths)
{
    if (paths.empty()) {
        return path();
    }

    path result = paths[0];
    for (size_t i = 1; i < paths.size(); ++i) {
        result /= paths[i];
    }
    return result;
}

std::optional<mc::string> read_file(const path& p)
{
    if (p.empty()) {
        return std::nullopt;
    }
    try {
        path normalized_p = normalize(p);
        if (!fs::is_regular_file(normalized_p)) {
            return std::nullopt;
        }
        std::ifstream file(normalized_p, std::ios::binary);
        if (!file) {
            return std::nullopt;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return mc::string(buffer.str());
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

bool write_file(const path& p, mc::string_view content)
{
    if (p.empty()) {
        return false;
    }
    try {
        path normalized_p = normalize(p);
        std::ofstream file(normalized_p, std::ios::binary);
        if (!file) {
            return false;
        }
        file << content;
        return file.good();
    } catch (const std::exception& e) {
        return false;
    }
}

bool append_file(const path& p, mc::string_view content)
{
    if (p.empty()) {
        return false;
    }
    try {
        path normalized_p = normalize(p);
        std::ofstream file(normalized_p, std::ios::binary | std::ios::app);
        if (!file) {
            return false;
        }
        file << content;
        return file.good();
    } catch (const std::exception& e) {
        return false;
    }
}

std::optional<int64_t> last_modified_time(const path& p)
{
    try {
        auto ftime = fs::last_write_time(p);
        // 转换文件系统时间点到时间戳
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        return std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

space_info space(const path& p)
{
    try {
        return fs::space(p);
    } catch (const std::exception& e) {
        return space_info{0, 0, 0}; // 返回所有值为0的space_info
    }
}

fs::path temp_directory_path()
{
    return fs::temp_directory_path();
}

} // namespace filesystem
} // namespace mc
