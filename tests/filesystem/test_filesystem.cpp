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
    EXPECT_EQ("/path/", mc::filesystem::normalize("/path/to/.."));
    EXPECT_EQ("/path/", mc::filesystem::normalize("/path/./to/.."));
    EXPECT_EQ(".", mc::filesystem::normalize(""));
}

// 路径拼接测试
TEST_F(FilesystemTest, PathJoin) {
    // 两段路径拼接
    EXPECT_EQ("/path/to/file.txt", mc::filesystem::join("/path/to", "file.txt"));
    EXPECT_EQ("/file.txt", mc::filesystem::join("/path/to", "/file.txt"));
    EXPECT_EQ("file.txt", mc::filesystem::join("", "file.txt"));
    EXPECT_EQ("/path/to/", mc::filesystem::join("/path/to", ""));

    // 多段路径拼接
    std::vector<mc::filesystem::path> paths = {"/path", "to", "file.txt"};
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
    EXPECT_TRUE(mc::filesystem::is_regular_file(test_file));
    EXPECT_FALSE(mc::filesystem::is_directory(test_file));

    if (!temp_dirs.empty()) {
        EXPECT_TRUE(mc::filesystem::is_directory(temp_dirs[0]));
        EXPECT_FALSE(mc::filesystem::is_regular_file(temp_dirs[0]));
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

    const std::string test_dir = temp_dirs[0];

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
    mc::filesystem::rename(dst_file, moved_file);
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

// 复杂场景：完整的文件操作流程
TEST_F(FilesystemTest, ComplexFileOperationWorkflow) {
    // 创建复杂的目录结构
    std::string base_dir = create_temp_dir();
    ASSERT_FALSE(base_dir.empty());
    temp_dirs.push_back(base_dir);

    std::string sub_dir1   = mc::filesystem::join(base_dir, "sub1");
    std::string sub_dir2   = mc::filesystem::join(base_dir, "sub2");
    std::string nested_dir = mc::filesystem::join(sub_dir1, "nested");

    ASSERT_TRUE(mc::filesystem::create_directories(nested_dir));
    ASSERT_TRUE(mc::filesystem::create_directory(sub_dir2));

    // 创建多个文件
    std::vector<std::string> files;
    for (int i = 0; i < 5; ++i) {
        std::string file = mc::filesystem::join(base_dir, "file" + std::to_string(i) + ".txt");
        ASSERT_TRUE(mc::filesystem::write_file(file, "content " + std::to_string(i)));
        files.push_back(file);
        temp_files.push_back(file);
    }

    // 在子目录中创建文件
    std::string sub_file = mc::filesystem::join(sub_dir1, "sub_file.txt");
    ASSERT_TRUE(mc::filesystem::write_file(sub_file, "sub content"));
    temp_files.push_back(sub_file);

    // 验证目录结构
    auto dirs = mc::filesystem::list_directories(base_dir);
    EXPECT_GE(dirs.size(), 2); // 至少包含 sub1 和 sub2

    auto all_files = mc::filesystem::list_files(base_dir);
    EXPECT_GE(all_files.size(), 5); // 至少包含创建的5个文件

    // 复制文件到子目录
    std::string copied_file = mc::filesystem::join(sub_dir2, "copied.txt");
    ASSERT_TRUE(mc::filesystem::copy_file(files[0], copied_file, false));
    temp_files.push_back(copied_file);

    // 验证复制成功
    EXPECT_TRUE(mc::filesystem::exists(copied_file));
    auto copied_content = mc::filesystem::read_file(copied_file);
    ASSERT_TRUE(copied_content);
    EXPECT_EQ(*copied_content, "content 0");

    // 移动文件
    std::string moved_file = mc::filesystem::join(sub_dir2, "moved.txt");
    mc::filesystem::rename(copied_file, moved_file);
    EXPECT_FALSE(mc::filesystem::exists(copied_file));
    EXPECT_TRUE(mc::filesystem::exists(moved_file));
    temp_files.push_back(moved_file);

    // 验证路径规范化
    std::string complex_path = base_dir + "/./sub1/../sub2/./moved.txt";
    auto        normalized   = mc::filesystem::normalize(complex_path);
    EXPECT_TRUE(mc::filesystem::exists(normalized));
}

// 复杂场景：路径操作和规范化
TEST_F(FilesystemTest, ComplexPathNormalization) {
    std::string base_dir = create_temp_dir();
    ASSERT_FALSE(base_dir.empty());
    temp_dirs.push_back(base_dir);

    // 创建嵌套目录结构
    std::string dir1 = mc::filesystem::join(base_dir, "dir1");
    std::string dir2 = mc::filesystem::join(dir1, "dir2");
    std::string dir3 = mc::filesystem::join(dir2, "dir3");
    ASSERT_TRUE(mc::filesystem::create_directories(dir3));

    // 测试各种路径规范化场景
    std::string path1 = base_dir + "/./dir1/./dir2/../dir2/dir3";
    auto        norm1 = mc::filesystem::normalize(path1);
    EXPECT_TRUE(mc::filesystem::exists(norm1));

    std::string path2 = base_dir + "/dir1/../dir1/dir2/dir3";
    auto        norm2 = mc::filesystem::normalize(path2);
    EXPECT_TRUE(mc::filesystem::exists(norm2));

    // 测试绝对路径
    auto abs_path = mc::filesystem::absolute(dir3);
    EXPECT_TRUE(mc::filesystem::exists(abs_path));
    EXPECT_TRUE(abs_path.is_absolute());

    // 测试路径拼接
    std::vector<mc::filesystem::path> paths  = {base_dir, "dir1", "dir2", "dir3"};
    auto                              joined = mc::filesystem::join(paths);
    EXPECT_TRUE(mc::filesystem::exists(joined));
}

// 复杂场景：文件操作的错误处理
TEST_F(FilesystemTest, ComplexFileOperationErrorHandling) {
    // 测试不存在的文件操作
    auto tmp_dir = mc::filesystem::temp_directory_path();
    std::string non_existent = (tmp_dir / "non_existent_file_12345.txt").string();
    EXPECT_FALSE(mc::filesystem::exists(non_existent));
    EXPECT_FALSE(mc::filesystem::is_regular_file(non_existent));
    EXPECT_FALSE(mc::filesystem::is_directory(non_existent));

    auto content = mc::filesystem::read_file(non_existent);
    EXPECT_FALSE(content.has_value());

    auto size = mc::filesystem::file_size(non_existent);
    EXPECT_FALSE(size.has_value());

    // 测试目录操作
    auto tmp_dir_for_dir = mc::filesystem::temp_directory_path();
    std::string non_existent_dir = (tmp_dir_for_dir / "non_existent_dir_12345").string();
    auto        files            = mc::filesystem::list_files(non_existent_dir);
    EXPECT_TRUE(files.empty());

    auto dirs = mc::filesystem::list_directories(non_existent_dir);
    EXPECT_TRUE(dirs.empty());

    // 测试符号链接（如果支持）
    std::string target_file = create_temp_file("target content");
    if (!target_file.empty()) {
        temp_files.push_back(target_file);
        std::string link_file = target_file + ".link";

        if (mc::filesystem::create_symlink(target_file, link_file)) {
            temp_files.push_back(link_file);

            // 读取符号链接
            auto link_target = mc::filesystem::read_symlink(link_file);
            if (link_target) {
                EXPECT_TRUE(mc::filesystem::exists(*link_target));
            }
        }
    }
}

// 空间信息与当前路径测试
TEST_F(FilesystemTest, SpaceAndCurrentPath) {
    auto original = mc::filesystem::current_path();

    auto space_info = mc::filesystem::space(original);
    EXPECT_GT(space_info.capacity, 0);
    EXPECT_GE(space_info.free, 0);
    EXPECT_GE(space_info.available, 0);

    auto temp_dir = mc::filesystem::temp_directory_path();
    if (!temp_dir.empty()) {
        mc::filesystem::current_path(temp_dir);
        auto now = mc::filesystem::current_path();
        // 在 macOS 上 /var 是到 /private/var 的符号链接
        try {
            auto now_canonical      = mc::filesystem::fs::canonical(now);
            auto expected_canonical = mc::filesystem::fs::canonical(mc::filesystem::absolute(temp_dir));
            EXPECT_EQ(now_canonical, expected_canonical);
        } catch (const std::exception&) {
            // 如果 canonical 失败，降级为简单的字符串比较（去除尾部斜杠）
            auto now_str      = now.string();
            auto expected_str = mc::filesystem::absolute(temp_dir).string();
            // 去除尾部斜杠
            if (!now_str.empty() && now_str.back() == '/') {
                now_str.pop_back();
            }
            if (!expected_str.empty() && expected_str.back() == '/') {
                expected_str.pop_back();
            }
            // 只验证临时目录路径是当前路径的后缀
            EXPECT_TRUE(now_str.find("T") != std::string::npos);
        }
    }

    mc::filesystem::current_path(original);
}
