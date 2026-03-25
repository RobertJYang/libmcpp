/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>

#include <mc/filesystem.h>

#include <algorithm>
#include <vector>

namespace {

struct path_collect_context {
    std::vector<mc::filesystem::path> entries;
};

bool collect_path_entry(const mc::filesystem::path& entry, void* userdata)
{
    auto* context = static_cast<path_collect_context*>(userdata);
    context->entries.push_back(entry);
    return true;
}

TEST(filesystem_test, path_helpers_return_path_facade)
{
    const mc::filesystem::path input("/tmp/demo/test.txt");

    const auto base = mc::filesystem::basename(input);
    const auto dir  = mc::filesystem::dirname(input);
    const auto ext  = mc::filesystem::extension(input);
    const auto name = mc::filesystem::stem(input);

    EXPECT_EQ(base.string(), "test.txt");
    EXPECT_EQ(dir.string(), "/tmp/demo");
    EXPECT_EQ(ext.string(), "txt");
    EXPECT_EQ(name.string(), "test");
}

TEST(filesystem_test, bool_and_out_param_api_reports_file_content)
{
    const auto root = mc::filesystem::temp_directory_path() / "mc_filesystem_bool_api";
    const auto file = root / "sample.txt";

    ASSERT_TRUE(mc::filesystem::create_directories(root));
    ASSERT_TRUE(mc::filesystem::write_file(file, "hello filesystem"));

    mc::string content;
    EXPECT_TRUE(mc::filesystem::read_file(file, content));
    EXPECT_EQ(content, "hello filesystem");

    uint64_t file_bytes = 0;
    EXPECT_TRUE(mc::filesystem::file_size(file, file_bytes));
    EXPECT_EQ(file_bytes, 16);

    uint64_t removed_count = 0;
    EXPECT_TRUE(mc::filesystem::remove_all(root, removed_count));
    EXPECT_GE(removed_count, 2U);
}

TEST(filesystem_test, visit_directory_and_list_directory_can_coexist)
{
    const auto root     = mc::filesystem::temp_directory_path() / "mc_filesystem_visit_directory";
    const auto file     = root / "a.txt";
    const auto sub_dir  = root / "nested";

    ASSERT_TRUE(mc::filesystem::create_directories(sub_dir));
    ASSERT_TRUE(mc::filesystem::write_file(file, "a"));

    path_collect_context context;
    ASSERT_TRUE(mc::filesystem::visit_directory(root, &collect_path_entry, &context));

    std::sort(context.entries.begin(), context.entries.end());
                auto listed = mc::filesystem::list_directory(root);
                std::sort(listed.begin(), listed.end());

    ASSERT_EQ(context.entries.size(), listed.size());
    for (size_t i = 0; i < listed.size(); ++i) {
        EXPECT_EQ(context.entries[i], listed[i]);
    }

    uint64_t removed_count = 0;
    EXPECT_TRUE(mc::filesystem::remove_all(root, removed_count));
    EXPECT_GE(removed_count, 3U);
}

} // namespace
