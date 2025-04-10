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
#include <experimental/filesystem>
#include <fstream>
#include <mc/filesystem.h>
#include <sstream>
#include <stdexcept>

namespace mc {
namespace filesystem {

namespace fs = std::experimental::filesystem;

std::string basename(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    // 使用std::filesystem::path的filename方法获取文件名
    return fs::path(path).filename().string();
}

std::string dirname(const std::string& path) {
    if (path.empty()) {
        return ".";
    }

    // 使用std::filesystem::path的parent_path方法获取父目录
    auto parent = fs::path(path).parent_path().string();
    return parent.empty() ? "." : parent;
}

std::string extension(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    // 获取扩展名（包括点）
    std::string ext = fs::path(path).extension().string();

    // 移除开头的点
    if (!ext.empty() && ext[0] == '.') {
        return ext.substr(1);
    }

    return ext;
}

std::string stem(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    // 获取文件名主干部分（不含扩展名）
    return fs::path(path).stem().string();
}

bool exists(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    return fs::exists(path, ec) && !ec;
}

bool is_file(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    return fs::is_regular_file(path, ec) && !ec;
}

bool is_directory(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    return fs::is_directory(path, ec) && !ec;
}

std::optional<uint64_t> file_size(const std::string& path) {
    if (!is_file(path)) {
        return std::nullopt;
    }

    std::error_code ec;
    auto            size = fs::file_size(path, ec);
    if (ec) {
        return std::nullopt;
    }

    return size;
}

std::vector<std::string> list_directory(const std::string& path) {
    std::vector<std::string> result;

    if (!is_directory(path)) {
        return result;
    }

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        if (!ec) {
            result.push_back(entry.path().string());
        }
    }

    return result;
}

std::vector<std::string> list_files(const std::string& path) {
    std::vector<std::string> result;

    if (!is_directory(path)) {
        return result;
    }

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        if (!ec && entry.status().type() == fs::file_type::regular && !ec) {
            result.push_back(entry.path().string());
        }
    }

    return result;
}

std::vector<std::string> list_directories(const std::string& path) {
    std::vector<std::string> result;

    if (!is_directory(path)) {
        return result;
    }

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        if (!ec && entry.status().type() == fs::file_type::directory && !ec) {
            result.push_back(entry.path().string());
        }
    }

    return result;
}

bool create_directory(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    return fs::create_directory(path, ec) && !ec;
}

bool create_directories(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    return fs::create_directories(path, ec) && !ec;
}

bool remove(const std::string& path) {
    if (path.empty() || !exists(path)) {
        return false;
    }

    std::error_code ec;
    return fs::remove(path, ec) && !ec;
}

std::optional<uint64_t> remove_all(const std::string& path) {
    if (path.empty() || !exists(path)) {
        return std::nullopt;
    }

    std::error_code ec;
    auto            count = fs::remove_all(path, ec);
    if (ec) {
        return std::nullopt;
    }

    return count;
}

bool copy_file(const std::string& src, const std::string& dst, bool overwrite) {
    if (src.empty() || dst.empty() || !is_file(src)) {
        return false;
    }

    std::error_code ec;
    auto options = overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none;
    fs::copy_file(src, dst, options, ec);

    return !ec;
}

bool rename(const std::string& src, const std::string& dst) {
    if (src.empty() || dst.empty() || !exists(src)) {
        return false;
    }

    std::error_code ec;
    fs::rename(src, dst, ec);

    return !ec;
}

bool create_symlink(const std::string& target, const std::string& link) {
    if (target.empty() || link.empty()) {
        return false;
    }

    std::error_code ec;
    if (is_directory(target)) {
        fs::create_directory_symlink(target, link, ec);
    } else {
        fs::create_symlink(target, link, ec);
    }

    return !ec;
}

std::optional<std::string> read_symlink(const std::string& path) {
    if (path.empty() || !fs::is_symlink(fs::path(path))) {
        return std::nullopt;
    }

    std::error_code ec;
    auto            target = fs::read_symlink(path, ec);
    if (ec) {
        return std::nullopt;
    }

    return target.string();
}

std::string current_path() {
    std::error_code ec;
    auto            path = fs::current_path(ec);

    return ec ? "" : path.string();
}

bool current_path(const std::string& path) {
    if (path.empty() || !is_directory(path)) {
        return false;
    }

    std::error_code ec;
    fs::current_path(path, ec);

    return !ec;
}

std::optional<std::string> absolute(const std::string& path) {
    if (path.empty()) {
        return std::nullopt;
    }

    try {
        // GCC 7.3.0的experimental/filesystem不支持带error_code参数的版本
        auto abs_path = fs::absolute(path);
        return abs_path.string();
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string normalize(const std::string& path) {
    if (path.empty()) {
        return ".";
    }

    try {
        // 实现简单的路径规范化，因为experimental版本没有weakly_canonical和lexically_normal
        fs::path p(path);

        // 移除".."和"."
        fs::path result;
        for (const auto& part : p) {
            if (part == "..") {
                if (!result.empty()) {
                    result = result.parent_path();
                }
            } else if (part != ".") {
                result /= part;
            }
        }

        // 确保结果不为空
        if (result.empty()) {
            return ".";
        }

        return result.string();
    } catch (const std::exception&) {
        // 如果失败，简单返回原路径
        return path;
    }
}

std::string join(const std::string& base, const std::string& path) {
    if (base.empty()) {
        return path;
    }
    if (path.empty()) {
        return base;
    }

    return (fs::path(base) / fs::path(path)).string();
}

std::string join(const std::vector<std::string>& paths) {
    if (paths.empty()) {
        return "";
    }

    fs::path result = paths[0];
    for (size_t i = 1; i < paths.size(); ++i) {
        result /= paths[i];
    }

    return result.string();
}

std::optional<std::string> read_file(const std::string& path) {
    if (!is_file(path)) {
        return std::nullopt;
    }

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::stringstream ss;
    ss << file.rdbuf();

    return ss.str();
}

bool write_file(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }

    file << content;
    return !file.fail();
}

bool append_file(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::app);
    if (!file) {
        return false;
    }

    file << content;
    return !file.fail();
}

std::optional<int64_t> last_modified_time(const std::string& path) {
    if (!exists(path)) {
        return std::nullopt;
    }

    std::error_code ec;
    auto            file_time = fs::last_write_time(path, ec);
    if (ec) {
        return std::nullopt;
    }

    // 转换为Unix时间戳（秒）
    auto sys_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        file_time - fs::file_time_type::clock::now() + std::chrono::system_clock::now());

    auto epoch_time = std::chrono::system_clock::to_time_t(sys_time);
    return static_cast<int64_t>(epoch_time);
}

} // namespace filesystem
} // namespace mc