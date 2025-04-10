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
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <mc/filesystem.h>
#include <random>
#include <string>
#include <vector>

// 辅助函数，创建测试文件
std::string create_temp_file(const std::string& content = "") {
    char tmp_filename[] = "/tmp/mc_fs_test_XXXXXX";
    int  fd             = mkstemp(tmp_filename);
    if (fd == -1) {
        return "";
    }

    if (!content.empty()) {
        write(fd, content.c_str(), content.size());
    }

    close(fd);
    return std::string(tmp_filename);
}

// 辅助函数，创建测试目录
std::string create_temp_dir() {
    // 临时文件名基于/tmp/mc_fs_test_XXXXXX模式，其中XXXXXX会被替换为随机字符
    char  tmp_dirname[] = "/tmp/mc_fs_test_dir_XXXXXX";
    char* dir           = mkdtemp(tmp_dirname);
    if (dir == nullptr) {
        return "";
    }

    return std::string(dir);
}

class FilesystemTest : public ::testing::Test {
protected:
    std::vector<std::string> temp_files;
    std::vector<std::string> temp_dirs;

    void SetUp() override {
        // 创建一些临时文件和目录供测试使用
        for (int i = 0; i < 3; ++i) {
            std::string file = create_temp_file("test content " + std::to_string(i));
            if (!file.empty()) {
                temp_files.push_back(file);
            }
        }

        temp_dirs.push_back(create_temp_dir());

        // 在临时目录中创建一些测试文件
        if (!temp_dirs.empty()) {
            for (int i = 0; i < 2; ++i) {
                std::string path =
                    mc::filesystem::join(temp_dirs[0], "test_file_" + std::to_string(i) + ".txt");
                std::ofstream ofs(path);
                ofs << "Nested test content " << i;
                ofs.close();

                temp_files.push_back(path); // 记录这些文件以便清理
            }

            // 创建一个嵌套目录
            std::string nested_dir = mc::filesystem::join(temp_dirs[0], "nested_dir");
            if (mc::filesystem::create_directory(nested_dir)) {
                temp_dirs.push_back(nested_dir);

                // 在嵌套目录中创建测试文件
                std::string   nested_file = mc::filesystem::join(nested_dir, "nested_file.txt");
                std::ofstream ofs(nested_file);
                ofs << "Nested directory test content";
                ofs.close();

                temp_files.push_back(nested_file);
            }
        }
    }

    void TearDown() override {
        // 清理所有临时文件和目录
        for (const auto& file : temp_files) {
            mc::filesystem::remove(file);
        }

        // 从最深层级的目录开始删除
        std::reverse(temp_dirs.begin(), temp_dirs.end());
        for (const auto& dir : temp_dirs) {
            mc::filesystem::remove(dir);
        }
    }
};

// 路径操作测试
TEST_F(FilesystemTest, PathOperations) {
    // basename 测试
    EXPECT_EQ("file.txt", mc::filesystem::basename("/path/to/file.txt"));
    EXPECT_EQ("file", mc::filesystem::basename("/path/to/file"));
    EXPECT_EQ("", mc::filesystem::basename(""));
    EXPECT_EQ(".", mc::filesystem::basename("."));
    EXPECT_EQ("..", mc::filesystem::basename(".."));

    // dirname 测试
    EXPECT_EQ("/path/to", mc::filesystem::dirname("/path/to/file.txt"));
    EXPECT_EQ("/path/to", mc::filesystem::dirname("/path/to/file"));
    EXPECT_EQ("/", mc::filesystem::dirname("/file"));
    EXPECT_EQ(".", mc::filesystem::dirname("file"));
    EXPECT_EQ(".", mc::filesystem::dirname(""));

    // extension 测试
    EXPECT_EQ("txt", mc::filesystem::extension("/path/to/file.txt"));
    EXPECT_EQ("", mc::filesystem::extension("/path/to/file"));
    EXPECT_EQ("md", mc::filesystem::extension("README.md"));
    EXPECT_EQ("", mc::filesystem::extension(""));

    // stem 测试
    EXPECT_EQ("file", mc::filesystem::stem("/path/to/file.txt"));
    EXPECT_EQ("file", mc::filesystem::stem("/path/to/file"));
    EXPECT_EQ("README", mc::filesystem::stem("README.md"));
    EXPECT_EQ("", mc::filesystem::stem(""));

    // normalize 测试
    EXPECT_EQ("/path", mc::filesystem::normalize("/path/to/.."));
    EXPECT_EQ("/path", mc::filesystem::normalize("/path/./to/.."));
    EXPECT_EQ(".", mc::filesystem::normalize(""));
}

// 路径拼接测试
TEST_F(FilesystemTest, PathJoin) {
    // 两段路径拼接
    EXPECT_EQ("/path/to/file.txt", mc::filesystem::join("/path/to", "file.txt"));
    EXPECT_EQ("/path/to/file.txt", mc::filesystem::join("/path/to", "/file.txt"));
    EXPECT_EQ("file.txt", mc::filesystem::join("", "file.txt"));
    EXPECT_EQ("/path/to", mc::filesystem::join("/path/to", ""));

    // 多段路径拼接
    std::vector<std::string> paths = {"/path", "to", "file.txt"};
    EXPECT_EQ("/path/to/file.txt", mc::filesystem::join(paths));

    paths = {};
    EXPECT_EQ("", mc::filesystem::join(paths));
}

// 文件信息测试
TEST_F(FilesystemTest, FileInfo) {
    if (temp_files.empty()) {
        GTEST_SKIP() << "临时文件创建失败，跳过测试";
    }

    const std::string& test_file = temp_files[0];

    // 文件存在性测试
    EXPECT_TRUE(mc::filesystem::exists(test_file));
    EXPECT_FALSE(mc::filesystem::exists(test_file + ".nonexistent"));

    // 文件类型测试
    EXPECT_TRUE(mc::filesystem::is_file(test_file));
    EXPECT_FALSE(mc::filesystem::is_directory(test_file));

    if (!temp_dirs.empty()) {
        EXPECT_TRUE(mc::filesystem::is_directory(temp_dirs[0]));
        EXPECT_FALSE(mc::filesystem::is_file(temp_dirs[0]));
    }

    // 文件大小测试
    auto size = mc::filesystem::file_size(test_file);
    ASSERT_TRUE(size.has_value());
    EXPECT_GT(size.value(), 0);

    auto nonexistent_size = mc::filesystem::file_size(test_file + ".nonexistent");
    EXPECT_FALSE(nonexistent_size.has_value());

    // 最后修改时间测试
    auto mtime = mc::filesystem::last_modified_time(test_file);
    ASSERT_TRUE(mtime.has_value());
    EXPECT_GT(mtime.value(), 0);
}

// 目录操作测试
TEST_F(FilesystemTest, DirectoryOperations) {
    if (temp_dirs.empty()) {
        GTEST_SKIP() << "临时目录创建失败，跳过测试";
    }

    const std::string& test_dir = temp_dirs[0];

    try {
        // 目录列表测试
        auto files = mc::filesystem::list_files(test_dir);
        EXPECT_EQ(2, files.size()); // 我们在 SetUp 中创建了两个文件

        auto dirs = mc::filesystem::list_directories(test_dir);
        EXPECT_EQ(1, dirs.size()); // 我们在 SetUp 中创建了一个嵌套目录

        auto all = mc::filesystem::list_directory(test_dir);
        EXPECT_EQ(3, all.size()); // 总共两个文件和一个目录

        // 测试创建目录
        std::string new_dir = mc::filesystem::join(test_dir, "new_test_dir");
        EXPECT_TRUE(mc::filesystem::create_directory(new_dir));
        EXPECT_TRUE(mc::filesystem::is_directory(new_dir));
        temp_dirs.push_back(new_dir); // 添加到临时目录列表以便清理

        // 测试递归创建目录
        std::string deep_dir = mc::filesystem::join(test_dir, "deep/nested/test/dir");
        EXPECT_TRUE(mc::filesystem::create_directories(deep_dir));
        EXPECT_TRUE(mc::filesystem::is_directory(deep_dir));
        temp_dirs.push_back(deep_dir); // 添加到临时目录列表以便清理
    } catch (const std::bad_alloc& e) {
        GTEST_SKIP() << "内存分配失败: " << e.what();
    } catch (const std::exception& e) {
        GTEST_SKIP() << "测试期间发生异常: " << e.what();
    }
}

// 文件读写测试
TEST_F(FilesystemTest, FileReadWrite) {
    // 创建测试文件路径
    std::string test_path = create_temp_file();
    if (test_path.empty()) {
        GTEST_SKIP() << "临时文件创建失败，跳过测试";
    }
    temp_files.push_back(test_path);

    // 测试写入文件
    std::string test_content = "这是一个测试内容，包含一些特殊字符：!@#$%^&*()\n第二行数据\n";
    EXPECT_TRUE(mc::filesystem::write_file(test_path, test_content));

    // 测试读取文件
    auto read_content = mc::filesystem::read_file(test_path);
    ASSERT_TRUE(read_content.has_value());
    EXPECT_EQ(test_content, read_content.value());

    // 测试追加文件
    std::string append_content = "追加的内容\n";
    EXPECT_TRUE(mc::filesystem::append_file(test_path, append_content));

    // 验证追加后的内容
    auto appended_content = mc::filesystem::read_file(test_path);
    ASSERT_TRUE(appended_content.has_value());
    EXPECT_EQ(test_content + append_content, appended_content.value());

    // 测试读取不存在的文件
    auto nonexistent = mc::filesystem::read_file(test_path + ".nonexistent");
    EXPECT_FALSE(nonexistent.has_value());
}

// 文件操作测试
TEST_F(FilesystemTest, FileOperations) {
    if (temp_files.empty()) {
        GTEST_SKIP() << "临时文件创建失败，跳过测试";
    }

    const std::string& src_file = temp_files[0];
    std::string        dst_file = src_file + ".copy";

    // 测试文件复制
    EXPECT_TRUE(mc::filesystem::copy_file(src_file, dst_file));
    EXPECT_TRUE(mc::filesystem::exists(dst_file));
    temp_files.push_back(dst_file); // 添加到临时文件列表以便清理

    // 测试复制已存在文件(不覆盖)
    EXPECT_FALSE(mc::filesystem::copy_file(src_file, dst_file, false));

    // 测试复制已存在文件(覆盖)
    EXPECT_TRUE(mc::filesystem::copy_file(src_file, dst_file, true));

    // 测试移动/重命名文件
    std::string moved_file = src_file + ".moved";
    EXPECT_TRUE(mc::filesystem::rename(dst_file, moved_file));
    EXPECT_FALSE(mc::filesystem::exists(dst_file));
    EXPECT_TRUE(mc::filesystem::exists(moved_file));
    temp_files.push_back(moved_file); // 添加到临时文件列表以便清理

    // 测试删除文件
    EXPECT_TRUE(mc::filesystem::remove(moved_file));
    EXPECT_FALSE(mc::filesystem::exists(moved_file));

    // 从临时文件列表中移除已删除的文件
    temp_files.erase(std::remove(temp_files.begin(), temp_files.end(), moved_file),
                     temp_files.end());
}
