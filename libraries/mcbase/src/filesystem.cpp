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
#include <fstream>
#include <sstream>
#include <vector>

#if defined(__cpp_lib_filesystem)
#include <filesystem>
namespace fs = std::filesystem;
#elif defined(__cpp_lib_experimental_filesystem)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#elif defined(_MSC_VER) && _MSC_VER >= 1900
#include <filesystem>
namespace fs = std::filesystem;
#elif defined(__has_include)
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error "无法找到文件系统库，请确保编译器支持 C++17 std::filesystem 或 std::experimental::filesystem"
#endif
#else
#error "无法找到文件系统库，请确保编译器支持 C++17 std::filesystem 或 std::experimental::filesystem"
#endif

#include <mc/filesystem.h>

namespace mc {
namespace filesystem {

namespace {

fs::path to_native_path(const path& p)
{
    return fs::path(mc::to_std_string(p.string()));
}

path from_native_path(const fs::path& p)
{
    return path(mc::string(p.string()));
}

template <typename Predicate>
bool visit_directory_entries(const path& p, path_visit_fn visitor, void* userdata, Predicate predicate)
{
    if (visitor == nullptr) {
        return false;
    }

    try {
        const fs::path native_path = to_native_path(p);
        if (!fs::is_directory(native_path)) {
            return false;
        }

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(native_path, ec)) {
            if (ec) {
                return false;
            }

            if (!predicate(entry)) {
                continue;
            }

            if (!visitor(from_native_path(entry.path()), userdata)) {
                break;
            }
        }

        return !ec;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

path path::parent_path() const
{
    return dirname(*this);
}

path path::filename() const
{
    return basename(*this);
}

path path::root_path() const
{
    return from_native_path(to_native_path(*this).root_path());
}

path path::extension() const
{
    return filesystem::extension(*this);
}

path path::stem() const
{
    return filesystem::stem(*this);
}

path& path::append_path(const path& rhs)
{
    if (rhs.empty()) {
        return *this;
    }

    if (empty() || is_absolute_path(rhs.view())) {
        m_path = rhs.m_path;
        return *this;
    }

    const bool lhs_has_sep = is_separator(view().back());
    const bool rhs_has_sep = is_separator(rhs.view().front());

    if (!lhs_has_sep && !rhs_has_sep) {
        m_path += '/';
        m_path += rhs.m_path;
        return *this;
    }

    if (lhs_has_sep && rhs_has_sep) {
        m_path += rhs.m_path.substr(1);
        return *this;
    }

    m_path += rhs.m_path;
    return *this;
}

path basename(const path& p)
{
    if (p.empty()) {
        return {};
    }

    return from_native_path(to_native_path(p).filename());
}

path dirname(const path& p)
{
    if (p.empty()) {
        return path(".");
    }

    const fs::path parent = to_native_path(p).parent_path();
    return parent.empty() ? path(".") : from_native_path(parent);
}

path extension(const path& p)
{
    if (p.empty()) {
        return {};
    }

    mc::string ext(to_native_path(p).extension().string());
    if (!ext.empty() && ext.view().front() == '.') {
        ext = ext.substr(1);
    }

    return path(std::move(ext));
}

path stem(const path& p)
{
    if (p.empty()) {
        return {};
    }

    return from_native_path(to_native_path(p).stem());
}

bool exists(const path& p)
{
    try {
        return fs::exists(to_native_path(p));
    } catch (const std::exception&) {
        return false;
    }
}

bool is_regular_file(const path& p)
{
    try {
        return fs::is_regular_file(to_native_path(p));
    } catch (const std::exception&) {
        return false;
    }
}

bool is_directory(const path& p)
{
    try {
        return fs::is_directory(to_native_path(p));
    } catch (const std::exception&) {
        return false;
    }
}

bool file_size(const path& p, uint64_t& out_size)
{
    out_size = 0;
    try {
        out_size = static_cast<uint64_t>(fs::file_size(to_native_path(p)));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool visit_directory(const path& p, path_visit_fn visitor, void* userdata)
{
    return visit_directory_entries(p, visitor, userdata, [](const fs::directory_entry&) {
        return true;
    });
}

bool visit_files(const path& p, path_visit_fn visitor, void* userdata)
{
    return visit_directory_entries(p, visitor, userdata, [](const fs::directory_entry& entry) {
        return fs::is_regular_file(entry.path());
    });
}

bool visit_directories(const path& p, path_visit_fn visitor, void* userdata)
{
    return visit_directory_entries(p, visitor, userdata, [](const fs::directory_entry& entry) {
        return fs::is_directory(entry.path());
    });
}

bool create_directory(const path& p)
{
    try {
        return fs::create_directory(to_native_path(p));
    } catch (const std::exception&) {
        return false;
    }
}

bool create_directories(const path& p)
{
    try {
        return fs::create_directories(to_native_path(p));
    } catch (const std::exception&) {
        return false;
    }
}

bool remove(const path& p)
{
    try {
        return fs::remove(to_native_path(p));
    } catch (const std::exception&) {
        return false;
    }
}

bool remove_all(const path& p, uint64_t& out_removed_count)
{
    out_removed_count = 0;
    try {
        out_removed_count = static_cast<uint64_t>(fs::remove_all(to_native_path(p)));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool copy_file(const path& from, const path& to, bool overwrite)
{
    try {
        const fs::copy_options options = overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none;
        fs::copy_file(to_native_path(from), to_native_path(to), options);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void rename(const path& from, const path& to)
{
    fs::rename(to_native_path(from), to_native_path(to));
}

bool create_symlink(const path& target, const path& link)
{
    try {
        fs::create_symlink(to_native_path(target), to_native_path(link));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool read_symlink(const path& p, path& out_target)
{
    out_target.clear();
    try {
        out_target = from_native_path(fs::read_symlink(to_native_path(p)));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

path current_path()
{
    return from_native_path(fs::current_path());
}

void current_path(const path& p)
{
    fs::current_path(to_native_path(p));
}

path absolute(const path& p)
{
    try {
        return from_native_path(fs::absolute(to_native_path(p)));
    } catch (const std::exception&) {
        return p;
    }
}

path normalize(const path& p)
{
    if (p.empty()) {
        return path(".");
    }

    const fs::path native_path = to_native_path(p);

#if defined(__cpp_lib_filesystem) || defined(_MSC_VER)
    return from_native_path(native_path.lexically_normal());
#else
    fs::path              result;
    const bool            is_absolute_path = native_path.is_absolute();
    const fs::path        root_name        = native_path.root_name();
    const fs::path        root_directory   = native_path.root_directory();
    std::vector<fs::path> elements;

    if (!root_name.empty()) {
        result /= root_name;
    }

    if (!root_directory.empty()) {
        result /= root_directory;
    }

    for (const auto& part : native_path) {
        const std::string part_str = part.string();
        if (part_str == ".") {
            continue;
        }

        if (part_str == "..") {
            if (!elements.empty() && elements.back().string() != "..") {
                elements.pop_back();
            } else if (!is_absolute_path) {
                elements.push_back(part);
            }
            continue;
        }

        if (!part_str.empty() && part_str != root_name.string() && part_str != root_directory.string()) {
            elements.push_back(part);
        }
    }

    for (const auto& element : elements) {
        result /= element;
    }

    if (result.empty()) {
        return path(".");
    }

    return from_native_path(result);
#endif
}

path join(const path& base, const path& p)
{
    return base / p;
}

bool read_file(const path& p, mc::string& out_content)
{
    out_content.clear();
    if (p.empty()) {
        return false;
    }

    try {
        const path normalized_path = normalize(p);
        if (!fs::is_regular_file(to_native_path(normalized_path))) {
            return false;
        }

        std::ifstream file(to_native_path(normalized_path), std::ios::binary);
        if (!file) {
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        out_content = mc::string(buffer.str());
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool write_file(const path& p, mc::string_view content)
{
    if (p.empty()) {
        return false;
    }

    try {
        const path    normalized_path = normalize(p);
        std::ofstream file(to_native_path(normalized_path), std::ios::binary);
        if (!file) {
            return false;
        }

        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        return file.good();
    } catch (const std::exception&) {
        return false;
    }
}

bool append_file(const path& p, mc::string_view content)
{
    if (p.empty()) {
        return false;
    }

    try {
        const path    normalized_path = normalize(p);
        std::ofstream file(to_native_path(normalized_path), std::ios::binary | std::ios::app);
        if (!file) {
            return false;
        }

        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        return file.good();
    } catch (const std::exception&) {
        return false;
    }
}

bool last_modified_time(const path& p, int64_t& out_timestamp)
{
    out_timestamp = 0;
    try {
        const auto ftime = fs::last_write_time(to_native_path(p));
        const auto sctp  = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        out_timestamp = std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

space_info space(const path& p)
{
    try {
        const auto native_space = fs::space(to_native_path(p));
        return space_info{
            static_cast<uint64_t>(native_space.capacity),
            static_cast<uint64_t>(native_space.free),
            static_cast<uint64_t>(native_space.available),
        };
    } catch (const std::exception&) {
        return {};
    }
}

path temp_directory_path()
{
    return from_native_path(fs::temp_directory_path());
}

} // namespace filesystem
} // namespace mc
