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

#include <cstdint>
#include <experimental/filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace mc {
namespace filesystem {

/**
 * @brief 从路径中提取文件名部分（不含目录）
 *
 * @param path 文件路径
 * @return std::string 文件名部分
 */
std::string basename(const std::string& path);

/**
 * @brief 从路径中提取目录部分
 *
 * @param path 文件路径
 * @return std::string 目录部分
 */
std::string dirname(const std::string& path);

/**
 * @brief 获取文件扩展名
 *
 * @param path 文件路径
 * @return std::string 扩展名（不含点）
 */
std::string extension(const std::string& path);

/**
 * @brief 获取不带扩展名的文件名
 *
 * @param path 文件路径
 * @return std::string 不带扩展名的文件名
 */
std::string stem(const std::string& path);

/**
 * @brief 检查路径是否存在
 *
 * @param path 要检查的路径
 * @return bool 如果路径存在返回true，否则返回false
 */
bool exists(const std::string& path);

/**
 * @brief 检查路径是否是文件
 *
 * @param path 要检查的路径
 * @return bool 如果路径是文件返回true，否则返回false
 */
bool is_file(const std::string& path);

/**
 * @brief 检查路径是否是目录
 *
 * @param path 要检查的路径
 * @return bool 如果路径是目录返回true，否则返回false
 */
bool is_directory(const std::string& path);

/**
 * @brief 获取文件大小
 *
 * @param path 文件路径
 * @return std::optional<uint64_t> 文件大小（字节），如果文件不存在或无法访问返回空
 */
std::optional<uint64_t> file_size(const std::string& path);

/**
 * @brief 列出目录中的所有文件和子目录
 *
 * @param path 目录路径
 * @return std::vector<std::string> 文件和子目录的路径列表
 */
std::vector<std::string> list_directory(const std::string& path);

/**
 * @brief 列出目录中的所有文件（不包括子目录）
 *
 * @param path 目录路径
 * @return std::vector<std::string> 文件路径列表
 */
std::vector<std::string> list_files(const std::string& path);

/**
 * @brief 列出目录中的所有子目录（不包括文件）
 *
 * @param path 目录路径
 * @return std::vector<std::string> 子目录路径列表
 */
std::vector<std::string> list_directories(const std::string& path);

/**
 * @brief 创建目录
 *
 * @param path 要创建的目录路径
 * @return bool 成功返回true，失败返回false
 */
bool create_directory(const std::string& path);

/**
 * @brief 递归创建目录
 *
 * @param path 要创建的目录路径
 * @return bool 成功返回true，失败返回false
 */
bool create_directories(const std::string& path);

/**
 * @brief 删除文件或空目录
 *
 * @param path 要删除的路径
 * @return bool 成功返回true，失败返回false
 */
bool remove(const std::string& path);

/**
 * @brief 递归删除目录及其内容
 *
 * @param path 要删除的目录路径
 * @return std::optional<uint64_t> 删除的文件数量，失败返回空
 */
std::optional<uint64_t> remove_all(const std::string& path);

/**
 * @brief 复制文件
 *
 * @param src 源文件路径
 * @param dst 目标文件路径
 * @param overwrite 如果目标文件已存在是否覆盖
 * @return bool 成功返回true，失败返回false
 */
bool copy_file(const std::string& src, const std::string& dst, bool overwrite = false);

/**
 * @brief 移动文件或目录
 *
 * @param src 源路径
 * @param dst 目标路径
 * @return bool 成功返回true，失败返回false
 */
bool rename(const std::string& src, const std::string& dst);

/**
 * @brief 创建符号链接
 *
 * @param target 链接目标
 * @param link 链接路径
 * @return bool 成功返回true，失败返回false
 */
bool create_symlink(const std::string& target, const std::string& link);

/**
 * @brief 读取符号链接的目标
 *
 * @param path 符号链接路径
 * @return std::optional<std::string> 链接目标，如果失败返回空
 */
std::optional<std::string> read_symlink(const std::string& path);

/**
 * @brief 获取当前工作目录
 *
 * @return std::string 当前工作目录的绝对路径
 */
std::string current_path();

/**
 * @brief 设置当前工作目录
 *
 * @param path 要设置的目录路径
 * @return bool 成功返回true，失败返回false
 */
bool current_path(const std::string& path);

/**
 * @brief 获取绝对路径
 *
 * @param path 相对或绝对路径
 * @return std::optional<std::string> 绝对路径，如果失败返回空
 */
std::optional<std::string> absolute(const std::string& path);

/**
 * @brief 规范化路径，解析"."和".."
 *
 * @param path 要规范化的路径
 * @return std::string 规范化后的路径
 */
std::string normalize(const std::string& path);

/**
 * @brief 拼接路径
 *
 * @param base 基础路径
 * @param path 要添加的路径
 * @return std::string 拼接后的路径
 */
std::string join(const std::string& base, const std::string& path);

/**
 * @brief 拼接多个路径段
 *
 * @param paths 要拼接的路径段
 * @return std::string 拼接后的路径
 */
std::string join(const std::vector<std::string>& paths);

/**
 * @brief 读取整个文件内容到字符串
 *
 * @param path 文件路径
 * @return std::optional<std::string> 文件内容，如果失败返回空
 */
std::optional<std::string> read_file(const std::string& path);

/**
 * @brief 将内容写入文件（覆盖已有内容）
 *
 * @param path 文件路径
 * @param content 要写入的内容
 * @return bool 成功返回true，失败返回false
 */
bool write_file(const std::string& path, const std::string& content);

/**
 * @brief 将内容追加到文件
 *
 * @param path 文件路径
 * @param content 要追加的内容
 * @return bool 成功返回true，失败返回false
 */
bool append_file(const std::string& path, const std::string& content);

/**
 * @brief 获取文件最后修改时间（Unix时间戳）
 *
 * @param path 文件路径
 * @return std::optional<int64_t> 最后修改时间，如果失败返回空
 */
std::optional<int64_t> last_modified_time(const std::string& path);

} // namespace filesystem
} // namespace mc

#endif // MC_FILESYSTEM_H