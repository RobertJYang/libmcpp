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

// 这行代码创建了一个命名空间别名，将std::experimental::filesystem命名空间
// 简化为fs，这样在后续代码中可以直接使用fs::而不必每次都写完整的
// std::experimental::filesystem 是 C++17 标准之前的文件系统库的实验性实现
// 它是 C++17 中正式引入的 std::filesystem 的前身
namespace fs = std::experimental::filesystem;

// 获取路径中的文件名部分
std::string basename(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    // 使用std::filesystem::path的filename方法获取文件名
    return fs::path(path).filename().string();
}

// 获取路径中的目录部分
std::string dirname(const std::string& path) {
    if (path.empty()) {
        return ".";
    }

    // 使用std::filesystem::path的parent_path方法获取父目录
    auto parent = fs::path(path).parent_path().string();
    return parent.empty() ? "." : parent;
}

// 获取文件扩展名，不包含点号
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

// 获取文件名主干部分（不含扩展名）
std::string stem(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    // 获取文件名主干部分（不含扩展名）
    return fs::path(path).stem().string();
}

// 检查文件或目录是否存在
bool exists(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code ec; // 用于存储可能的错误码，避免抛出异常
    // 返回true当且仅当路径存在且没有发生错误
    //这种方式比直接调用fs::exists更安全，因为它不会在发生错误时抛出异常
    return fs::exists(path, ec) && !ec;
}

// 检查路径是否为普通文件
bool is_file(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    // is_regular_file检查是否为普通文件（不是目录、符号链接等）
    // 普通文件是指不是特殊文件的文件，主要包括以下几种类型：
    // 1. 文本文件：如.txt、.log、.conf等
    // 2. 二进制文件：如可执行文件、图像文件、音频文件等
    // 3. 数据文件：如.csv、.json、.xml等
    // 4. 配置文件：如.ini、.cfg、.properties等
    // 而特殊文件包括：目录、符号链接、设备文件、管道、套接字等
    return fs::is_regular_file(path, ec) && !ec;
}

// 检查路径是否为目录
bool is_directory(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    return fs::is_directory(path, ec) && !ec;
}

// 获取文件大小，返回std::optional表示可能无结果
// std::optional是C++17引入的类型，类似于可空指针但更安全
std::optional<uint64_t> file_size(const std::string& path) {
    if (!is_file(path)) {
        return std::nullopt; // 相当于返回"无值"
    }

    std::error_code ec;
    auto            size = fs::file_size(path, ec);
    if (ec) {
        return std::nullopt;
    }

    return size;
}

// 列出目录中的所有项目（文件和子目录）
std::vector<std::string> list_directory(const std::string& path) {
    std::vector<std::string> result; // 动态数组，类似于可自动扩容的数组

    if (!is_directory(path)) {
        return result;
    }

    std::error_code ec;
    // 使用迭代器遍历目录内容
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        if (!ec) {
            result.push_back(entry.path().string());
        }
    }

    return result;
}

// 列出目录中的所有普通文件
std::vector<std::string> list_files(const std::string& path) {
    // 创建一个字符串向量用于存储结果
    std::vector<std::string> result;

    if (!is_directory(path)) {
        return result;
    }

    std::error_code ec;
    // 遍历指定目录中的所有条目
    // const auto& entry: const表示常量引用，auto表示自动类型推导
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        // entry.status()获取文件状态，type()获取文件类型
        if (!ec && entry.status().type() == fs::file_type::regular && !ec) {
            result.push_back(entry.path().string());
        }
    }

    return result;
}

// 列出目录中的所有子目录
std::vector<std::string> list_directories(const std::string& path) {
    std::vector<std::string> result;

    if (!is_directory(path)) {
        return result;
    }

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        // 检查是否是目录
        if (!ec && entry.status().type() == fs::file_type::directory && !ec) {
            result.push_back(entry.path().string());
        }
    }

    return result;
}

// 创建单层目录
bool create_directory(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    // 只创建最后一级目录，父目录必须存在
    return fs::create_directory(path, ec) && !ec;
}

// 创建多层目录（包括不存在的父目录）
bool create_directories(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    // 递归创建所有不存在的目录层级，类似mkdir -p
    return fs::create_directories(path, ec) && !ec;
}

// 删除文件或空目录
bool remove(const std::string& path) {
    if (path.empty() || !exists(path)) {
        return false;
    }

    std::error_code ec;
    // 只能删除文件或空目录，非空目录会失败
    return fs::remove(path, ec) && !ec;
}

// 递归删除目录及其内容，返回删除的文件数量
std::optional<uint64_t> remove_all(const std::string& path) {
    if (path.empty() || !exists(path)) {
        return std::nullopt;
    }

    std::error_code ec;
    // 类似rm -rf，递归删除目录及其所有内容
    auto            count = fs::remove_all(path, ec);
    if (ec) {
        return std::nullopt;
    }

    return count;
}

// 复制文件，可选是否覆盖已存在的目标文件
bool copy_file(const std::string& src, const std::string& dst, bool overwrite) {
    if (src.empty() || dst.empty() || !is_file(src)) {
        return false;
    }

    std::error_code ec;
    // 根据overwrite参数决定是否允许覆盖目标文件
    auto options = overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none;
    fs::copy_file(src, dst, options, ec);

    return !ec;
}

// 重命名/移动文件或目录
bool rename(const std::string& src, const std::string& dst) {
    if (src.empty() || dst.empty() || !exists(src)) {
        return false;
    }

    std::error_code ec;
    // 相当于mv命令，可以跨分区移动
    fs::rename(src, dst, ec);

    return !ec;
}

// 创建符号链接，根据目标类型自动选择文件或目录链接
bool create_symlink(const std::string& target, const std::string& link) {
    if (target.empty() || link.empty()) {
        return false;
    }

    std::error_code ec;
    // 根据目标类型选择创建文件链接或目录链接
    if (is_directory(target)) {
        fs::create_directory_symlink(target, link, ec);
    } else {
        fs::create_symlink(target, link, ec);
    }

    return !ec;
}

// 读取符号链接指向的目标路径
std::optional<std::string> read_symlink(const std::string& path) {
    if (path.empty() || !fs::is_symlink(fs::path(path))) {
        return std::nullopt;
    }

    std::error_code ec;
    // 获取符号链接指向的目标路径
    auto            target = fs::read_symlink(path, ec);
    if (ec) {
        return std::nullopt;
    }

    return target.string();
}

// 获取当前工作目录
std::string current_path() {
    std::error_code ec;
    // 相当于getcwd，获取当前工作目录
    auto            path = fs::current_path(ec);

    return ec ? "" : path.string();
}

// 设置当前工作目录
bool current_path(const std::string& path) {
    if (path.empty() || !is_directory(path)) {
        return false;
    }

    std::error_code ec;
    // 相当于chdir，更改当前工作目录
    fs::current_path(path, ec);

    return !ec;
}

// 获取路径的绝对路径形式
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

// 规范化路径，处理"."和".."等特殊路径组件
std::string normalize(const std::string& path) {
    if (path.empty()) {
        return ".";
    }

    try {
        // 实现简单的路径规范化，因为experimental版本没有weakly_canonical和lexically_normal
        fs::path p(path);

        // 移除".."和"."
        fs::path result;
        // 遍历路径的每个组件（循环中的part是路径中的每一段）
        for (const auto& part : p) {
            if (part == "..") {
                if (!result.empty()) {
                    // 遇到".."时回退一级目录
                    result = result.parent_path();
                }
            } else if (part != ".") {
                // 忽略"."，添加其他正常路径组件
                result /= part; // 路径连接操作符
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

// 连接两个路径
std::string join(const std::string& base, const std::string& path) {
    if (base.empty()) {
        return path;
    }
    if (path.empty()) {
        return base;
    }

    return (fs::path(base) / fs::path(path)).string(); // 使用/操作符连接路径
}

// 连接多个路径
std::string join(const std::vector<std::string>& paths) {
    if (paths.empty()) {
        return "";
    }

    fs::path result = paths[0];
    // 从第二个路径开始，依次连接到结果路径
    for (size_t i = 1; i < paths.size(); ++i) {
        result /= paths[i];
    }

    return result.string();
}

// 读取整个文件内容
std::optional<std::string> read_file(const std::string& path) {
    if (!is_file(path)) {
        return std::nullopt;
    }

    // 以二进制模式打开文件
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    // 使用字符串流读取整个文件
    std::stringstream ss;
    ss << file.rdbuf(); // 一次性读取文件缓冲区，比循环读取更高效

    return ss.str();
}

// 写入内容到文件（覆盖模式）
bool write_file(const std::string& path, const std::string& content) {
    // 以二进制模式打开文件，trunc表示截断已存在的文件
    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }

    file << content;
    // 检查写入操作是否成功
    return !file.fail();
}

// 追加内容到文件末尾
bool append_file(const std::string& path, const std::string& content) {
    // app表示追加模式，不会截断现有内容
    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::app);
    if (!file) {
        return false;
    }

    file << content;
    return !file.fail();
}

// 获取文件最后修改时间（Unix时间戳，秒）
std::optional<int64_t> last_modified_time(const std::string& path) {
    if (!exists(path)) {
        return std::nullopt;
    }

    std::error_code ec;
    // 获取文件的最后修改时间
    auto            file_time = fs::last_write_time(path, ec);
    if (ec) {
        return std::nullopt;
    }

    // 以下代码将文件系统时间点转换为系统时钟时间点，再转为时间戳
    // 这两行代码实现了文件系统时间点到系统时钟时间点的转换
    // 1. file_time 是从文件系统获取的文件最后修改时间（fs::file_time_type类型）
    // 2. 计算时间差：file_time - fs::file_time_type::clock::now()，得到文件时间与当前文件系统时间的差值
    // 3. 将这个差值加到系统时钟的当前时间上：+ std::chrono::system_clock::now()
    // 4. 使用time_point_cast将结果转换为system_clock的时间点，确保时间单位兼容
    // 这种转换方法解决了文件系统时钟和系统时钟可能使用不同时间起点的问题
    auto sys_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        file_time - fs::file_time_type::clock::now() + std::chrono::system_clock::now());

    // 转换为时间戳（秒）
    auto epoch_time = std::chrono::system_clock::to_time_t(sys_time);
    return static_cast<int64_t>(epoch_time);
}

} // namespace filesystem
} // namespace mc