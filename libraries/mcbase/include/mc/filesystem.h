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

/**
 * @file filesystem.h
 * @brief 提供文件系统相关操作的工具函数
 */
#ifndef MC_FILESYSTEM_H
#define MC_FILESYSTEM_H

#include <cctype>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string_view>
#include <utility>
#include <vector>

#include <mc/string.h>

namespace mc {
namespace filesystem {

class MC_API path {
public:
    path() = default;

    path(const mc::string& value) : m_path(value)
    {}

    path(mc::string&& value) : m_path(std::move(value))
    {}

    path(const std::string& value) : m_path(value)
    {}

    path(std::string_view value) : m_path(mc::string_view(value.data(), value.size()))
    {}

    path(mc::string_view value) : m_path(value)
    {}

    path(const char* value) : m_path(value)
    {}

    bool empty() const noexcept
    {
        return m_path.empty();
    }

    void clear() noexcept
    {
        m_path.clear();
    }

    const mc::string& string() const noexcept
    {
        return m_path;
    }

    mc::string_view view() const noexcept
    {
        return m_path.view();
    }

    const char* c_str() const noexcept
    {
        return m_path.c_str();
    }

    bool is_absolute() const noexcept
    {
        return is_absolute_path(view());
    }

    path parent_path() const;
    path filename() const;
    path root_path() const;
    path extension() const;
    path stem() const;

    operator mc::string_view() const noexcept
    {
        return view();
    }

    operator std::string() const
    {
        return std::string(m_path);
    }

    path& operator/=(mc::string_view rhs)
    {
        return append_path(path(rhs));
    }

    path& operator/=(const path& rhs)
    {
        return append_path(rhs);
    }

    friend path operator/(path lhs, const path& rhs)
    {
        lhs /= rhs;
        return lhs;
    }

    friend bool operator==(const path& lhs, const path& rhs) noexcept
    {
        return lhs.m_path == rhs.m_path;
    }

    friend bool operator!=(const path& lhs, const path& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    friend bool operator<(const path& lhs, const path& rhs) noexcept
    {
        return lhs.view() < rhs.view();
    }

private:
    static bool is_separator(char ch) noexcept
    {
        return ch == '/' || ch == '\\';
    }

    static bool is_absolute_path(mc::string_view value) noexcept
    {
        if (value.empty()) {
            return false;
        }

        if (is_separator(value.front())) {
            return true;
        }

        return value.size() >= 2 && std::isalpha(static_cast<unsigned char>(value[0])) != 0 && value[1] == ':';
    }

    path& append_path(const path& rhs);

    mc::string m_path;
};

struct space_info {
    uint64_t capacity{0};
    uint64_t free{0};
    uint64_t available{0};
};

using path_visit_fn = bool (*)(const path& entry, void* userdata);

inline std::ostream& operator<<(std::ostream& os, const path& p)
{
    os << p.string();
    return os;
}

MC_API path basename(const path& p);
inline mc::string basename(mc::string_view p)
{
    return basename(path(p)).string();
}
inline mc::string basename(const mc::string& p)
{
    return basename(p.view());
}
inline std::string basename(std::string_view p)
{
    return mc::to_std_string(basename(path(p)).string());
}
inline std::string basename(const std::string& p)
{
    return basename(std::string_view(p.data(), p.size()));
}
inline mc::string basename(const char* p)
{
    return basename(mc::string_view(p));
}

MC_API path dirname(const path& p);
inline mc::string dirname(mc::string_view p)
{
    return dirname(path(p)).string();
}
inline mc::string dirname(const mc::string& p)
{
    return dirname(p.view());
}
inline std::string dirname(std::string_view p)
{
    return mc::to_std_string(dirname(path(p)).string());
}
inline std::string dirname(const std::string& p)
{
    return dirname(std::string_view(p.data(), p.size()));
}
inline mc::string dirname(const char* p)
{
    return dirname(mc::string_view(p));
}

MC_API path extension(const path& p);
inline mc::string extension(mc::string_view p)
{
    return extension(path(p)).string();
}
inline mc::string extension(const mc::string& p)
{
    return extension(p.view());
}
inline std::string extension(std::string_view p)
{
    return mc::to_std_string(extension(path(p)).string());
}
inline std::string extension(const std::string& p)
{
    return extension(std::string_view(p.data(), p.size()));
}
inline mc::string extension(const char* p)
{
    return extension(mc::string_view(p));
}

MC_API path stem(const path& p);
inline mc::string stem(mc::string_view p)
{
    return stem(path(p)).string();
}
inline mc::string stem(const mc::string& p)
{
    return stem(p.view());
}
inline std::string stem(std::string_view p)
{
    return mc::to_std_string(stem(path(p)).string());
}
inline std::string stem(const std::string& p)
{
    return stem(std::string_view(p.data(), p.size()));
}
inline mc::string stem(const char* p)
{
    return stem(mc::string_view(p));
}

MC_API bool exists(const path& p);
MC_API bool is_regular_file(const path& p);
MC_API bool is_directory(const path& p);

MC_API bool file_size(const path& p, uint64_t& out_size);
inline std::optional<uint64_t> file_size(const path& p)
{
    uint64_t out_size = 0;
    if (!file_size(p, out_size)) {
        return std::nullopt;
    }
    return out_size;
}

MC_API bool visit_directory(const path& p, path_visit_fn visitor, void* userdata);
MC_API bool visit_files(const path& p, path_visit_fn visitor, void* userdata);
MC_API bool visit_directories(const path& p, path_visit_fn visitor, void* userdata);

namespace detail {
struct path_list_collector {
    std::vector<path> entries;

    static bool collect(const path& entry, void* userdata)
    {
        auto* self = static_cast<path_list_collector*>(userdata);
        self->entries.push_back(entry);
        return true;
    }
};
} // namespace detail

inline std::vector<path> list_directory(const path& p)
{
    detail::path_list_collector collector;
    if (!visit_directory(p, &detail::path_list_collector::collect, &collector)) {
        return {};
    }
    return std::move(collector.entries);
}

inline std::vector<path> list_files(const path& p)
{
    detail::path_list_collector collector;
    if (!visit_files(p, &detail::path_list_collector::collect, &collector)) {
        return {};
    }
    return std::move(collector.entries);
}

inline std::vector<path> list_directories(const path& p)
{
    detail::path_list_collector collector;
    if (!visit_directories(p, &detail::path_list_collector::collect, &collector)) {
        return {};
    }
    return std::move(collector.entries);
}

MC_API bool create_directory(const path& p);
MC_API bool create_directories(const path& p);
MC_API bool remove(const path& p);

MC_API bool remove_all(const path& p, uint64_t& out_removed_count);
inline std::optional<uint64_t> remove_all(const path& p)
{
    uint64_t out_removed_count = 0;
    if (!remove_all(p, out_removed_count)) {
        return std::nullopt;
    }
    return out_removed_count;
}

MC_API bool copy_file(const path& from, const path& to, bool overwrite = false);
MC_API void rename(const path& from, const path& to);
MC_API bool create_symlink(const path& target, const path& link);

MC_API bool read_symlink(const path& p, path& out_target);
inline std::optional<path> read_symlink(const path& p)
{
    path out_target;
    if (!read_symlink(p, out_target)) {
        return std::nullopt;
    }
    return out_target;
}

MC_API path current_path();
MC_API void current_path(const path& p);
MC_API path absolute(const path& p);
MC_API path normalize(const path& p);

MC_API path join(const path& base, const path& p);
inline mc::string join(mc::string_view base, mc::string_view p)
{
    return join(path(base), path(p)).string();
}
inline std::string join(std::string_view base, std::string_view p)
{
    return mc::to_std_string(join(path(base), path(p)).string());
}
inline mc::string join(const mc::string& base, const mc::string& p)
{
    return join(base.view(), p.view());
}
inline std::string join(const std::string& base, const std::string& p)
{
    return join(std::string_view(base.data(), base.size()), std::string_view(p.data(), p.size()));
}
inline path join(const std::vector<path>& paths)
{
    if (paths.empty()) {
        return {};
    }

    path result = paths[0];
    for (std::size_t i = 1; i < paths.size(); ++i) {
        result /= paths[i];
    }
    return result;
}

MC_API bool read_file(const path& p, mc::string& out_content);
inline std::optional<mc::string> read_file(const path& p)
{
    mc::string out_content;
    if (!read_file(p, out_content)) {
        return std::nullopt;
    }
    return out_content;
}
inline std::optional<mc::string> read_file(mc::string_view p)
{
    return read_file(path(p));
}
inline std::optional<mc::string> read_file(const mc::string& p)
{
    return read_file(p.view());
}
inline std::optional<mc::string> read_file(std::string_view p)
{
    return read_file(path(p));
}
inline std::optional<mc::string> read_file(const std::string& p)
{
    return read_file(std::string_view(p.data(), p.size()));
}
inline std::optional<mc::string> read_file(const char* p)
{
    return read_file(mc::string_view(p));
}

MC_API bool write_file(const path& p, mc::string_view content);
inline bool write_file(mc::string_view p, mc::string_view content)
{
    return write_file(path(p), content);
}
inline bool write_file(const mc::string& p, mc::string_view content)
{
    return write_file(p.view(), content);
}
inline bool write_file(std::string_view p, mc::string_view content)
{
    return write_file(path(p), content);
}
inline bool write_file(const std::string& p, mc::string_view content)
{
    return write_file(std::string_view(p.data(), p.size()), content);
}
inline bool write_file(const char* p, mc::string_view content)
{
    return write_file(mc::string_view(p), content);
}

MC_API bool append_file(const path& p, mc::string_view content);
inline bool append_file(mc::string_view p, mc::string_view content)
{
    return append_file(path(p), content);
}
inline bool append_file(const mc::string& p, mc::string_view content)
{
    return append_file(p.view(), content);
}
inline bool append_file(std::string_view p, mc::string_view content)
{
    return append_file(path(p), content);
}
inline bool append_file(const std::string& p, mc::string_view content)
{
    return append_file(std::string_view(p.data(), p.size()), content);
}
inline bool append_file(const char* p, mc::string_view content)
{
    return append_file(mc::string_view(p), content);
}

MC_API bool last_modified_time(const path& p, int64_t& out_timestamp);
inline std::optional<int64_t> last_modified_time(const path& p)
{
    int64_t out_timestamp = 0;
    if (!last_modified_time(p, out_timestamp)) {
        return std::nullopt;
    }
    return out_timestamp;
}

MC_API space_info space(const path& p);
MC_API path       temp_directory_path();

} // namespace filesystem
} // namespace mc

#endif // MC_FILESYSTEM_H
