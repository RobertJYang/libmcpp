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
#include <optional>
#include <string_view>
#include <vector>

#include <mc/string.h>

// 检测文件系统库的可用性
#if defined(__cpp_lib_filesystem)
#define MC_HAS_STD_FILESYSTEM 1
#include <filesystem>
#elif defined(__cpp_lib_experimental_filesystem)
#define MC_HAS_EXPERIMENTAL_FILESYSTEM 1
#include <experimental/filesystem>
#elif defined(_MSC_VER) && _MSC_VER >= 1900
#define MC_HAS_STD_FILESYSTEM 1
#include <filesystem>
#elif defined(__has_include)
#if __has_include(<filesystem>)
#define MC_HAS_STD_FILESYSTEM 1
#include <filesystem>
#elif __has_include(<experimental/filesystem>)
#define MC_HAS_EXPERIMENTAL_FILESYSTEM 1
#include <experimental/filesystem>
#endif
#endif

// 如果仍然无法检测到文件系统库，则报错
#if !defined(MC_HAS_STD_FILESYSTEM) && !defined(MC_HAS_EXPERIMENTAL_FILESYSTEM)
#error "无法找到文件系统库，请确保编译器支持 C++17 std::filesystem 或 std::experimental::filesystem"
#endif

namespace mc {
namespace filesystem {

// 创建统一的别名
#if defined(MC_HAS_STD_FILESYSTEM)
namespace fs = std::filesystem;
#else
namespace fs = std::experimental::filesystem;
#endif

// 常用类型和函数的便捷别名
using path                         = fs::path;
using directory_iterator           = fs::directory_iterator;
using recursive_directory_iterator = fs::recursive_directory_iterator;
using file_status                  = fs::file_status;
using space_info                   = fs::space_info;
using file_time_type               = fs::file_time_type;
using file_type                    = fs::file_type;
using filesystem_error             = fs::filesystem_error;

/**
 * @brief 从路径中提取文件名部分（不含目录）
 *
 * @param path 文件路径
 * @return mc::string 文件名部分
 */
MC_API mc::string basename(const fs::path& path);

/**
 * @brief 从 mc::string 路径提取文件名部分（不含目录）
 */
inline mc::string basename(const mc::string& path)
{
    return basename(fs::path(mc::to_std_string(path)));
}

/**
 * @brief 从路径中提取目录部分
 *
 * @param path 文件路径
 * @return mc::string 目录部分
 */
MC_API mc::string dirname(const fs::path& path);

/**
 * @brief 获取文件扩展名
 *
 * @param path 文件路径
 * @return mc::string 扩展名（不含点）
 */
MC_API mc::string extension(const fs::path& path);

/**
 * @brief 获取不带扩展名的文件名
 *
 * @param path 文件路径
 * @return mc::string 不带扩展名的文件名
 */
MC_API mc::string stem(const fs::path& path);

/**
 * @brief 检查路径是否存在
 *
 * @param path 要检查的路径
 * @return bool 如果路径存在返回true，否则返回false
 */
MC_API bool exists(const path& p);

/**
 * @brief 检查路径是否是文件
 *
 * @param path 要检查的路径
 * @return bool 如果路径是文件返回true，否则返回false
 */
MC_API bool is_regular_file(const path& p);

/**
 * @brief 检查路径是否是目录
 *
 * @param path 要检查的路径
 * @return bool 如果路径是目录返回true，否则返回false
 */
MC_API bool is_directory(const path& p);

/**
 * @brief 获取文件大小
 *
 * @param path 文件路径
 * @return std::optional<uint64_t> 文件大小（字节），如果文件不存在或无法访问返回空
 */
MC_API std::optional<uint64_t> file_size(const path& p);

/**
 * @brief 列出目录中的所有文件和子目录
 *
 * @param path 目录路径
 * @return std::vector<fs::path> 文件和子目录的路径列表
 */
MC_API std::vector<fs::path> list_directory(const fs::path& path);

/**
 * @brief 列出目录中的所有文件（不包括子目录）
 *
 * @param path 目录路径
 * @return std::vector<fs::path> 文件路径列表
 */
MC_API std::vector<fs::path> list_files(const fs::path& path);

/**
 * @brief 列出目录中的所有子目录（不包括文件）
 *
 * @param path 目录路径
 * @return std::vector<fs::path> 子目录路径列表
 */
MC_API std::vector<fs::path> list_directories(const fs::path& path);

/**
 * @brief 创建目录
 *
 * @param path 要创建的目录路径
 * @return bool 成功返回true，失败返回false
 */
MC_API bool create_directory(const path& p);

/**
 * @brief 递归创建目录
 *
 * @param path 要创建的目录路径
 * @return bool 成功返回true，失败返回false
 */
MC_API bool create_directories(const path& p);

/**
 * @brief 删除文件或空目录
 *
 * @param path 要删除的路径
 * @return bool 成功返回true，失败返回false
 */
MC_API bool remove(const path& p);

/**
 * @brief 递归删除目录及其内容
 *
 * @param path 要删除的目录路径
 * @return std::optional<uint64_t> 删除的文件数量，失败返回空
 */
MC_API std::optional<uint64_t> remove_all(const path& p);

/**
 * @brief 复制文件
 *
 * @param src 源文件路径
 * @param dst 目标文件路径
 * @param overwrite 如果目标文件已存在是否覆盖
 * @return bool 成功返回true，失败返回false
 */
MC_API bool copy_file(const path& from, const path& to, bool overwrite = false);

/**
 * @brief 移动文件或目录
 *
 * @param src 源路径
 * @param dst 目标路径
 * @return bool 成功返回true，失败返回false
 */
MC_API void rename(const path& from, const path& to);

/**
 * @brief 创建符号链接
 *
 * @param target 链接目标
 * @param link 链接路径
 * @return bool 成功返回true，失败返回false
 */
MC_API bool create_symlink(const path& target, const path& link);

/**
 * @brief 读取符号链接的目标
 *
 * @param path 符号链接路径
 * @return std::optional<fs::path> 链接目标，如果失败返回空
 */
MC_API std::optional<fs::path> read_symlink(const path& path);

/**
 * @brief 获取当前工作目录
 *
 * @return fs::path 当前工作目录的绝对路径
 */
MC_API fs::path current_path();

/**
 * @brief 设置当前工作目录
 *
 * @param path 要设置的目录路径
 * @return bool 成功返回true，失败返回false
 */
MC_API void current_path(const path& p);

/**
 * @brief 获取绝对路径
 *
 * @param path 相对或绝对路径
 * @return fs::path 绝对路径，如果失败返回空
 */
MC_API fs::path absolute(const path& p);

/**
 * @brief 规范化路径，解析"."和".."
 *
 * @param path 要规范化的路径
 * @return fs::path 规范化后的路径
 */
MC_API fs::path normalize(const fs::path& path);

/**
 * @brief 拼接路径
 *
 * @param base 基础路径
 * @param path 要添加的路径
 * @return fs::path 拼接后的路径
 */
MC_API fs::path join(const path& base, const path& path);

/**
 * @brief 拼接多个路径段
 *
 * @param paths 要拼接的路径段
 * @return fs::path 拼接后的路径
 */
MC_API fs::path join(const std::vector<path>& paths);

/**
 * @brief 读取整个文件内容到字符串
 *
 * @param path 文件路径
 * @return std::optional<mc::string> 文件内容，如果失败返回空
 */
MC_API std::optional<mc::string> read_file(const path& p);

/**
 * @brief 将内容写入文件（覆盖已有内容）
 *
 * @param path 文件路径
 * @param content 要写入的内容
 * @return bool 成功返回true，失败返回false
 */
MC_API bool write_file(const path& p, mc::string_view content);

/**
 * @brief 将内容追加到文件
 *
 * @param path 文件路径
 * @param content 要追加的内容
 * @return bool 成功返回true，失败返回false
 */
MC_API bool append_file(const path& p, mc::string_view content);

/**
 * @brief 获取文件最后修改时间（Unix时间戳）
 *
 * @param path 文件路径
 * @return std::optional<int64_t> 最后修改时间，如果失败返回空
 */
MC_API std::optional<int64_t> last_modified_time(const path& p);

/**
 * @brief 获取文件系统空间信息
 *
 * @param path 文件路径
 * @return space_info 文件系统空间信息
 */
MC_API space_info space(const path& p);

/**
 * @brief 获取临时目录路径
 *
 * @return fs::path 临时目录路径
 */
MC_API fs::path temp_directory_path();

} // namespace filesystem
} // namespace mc

#endif // MC_FILESYSTEM_H
