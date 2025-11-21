/*
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
#include <test_utilities/test_base.h>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "core/include/dependency_sorter.h"

using namespace mc::core::internal;

class dependency_sorter_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        TestBase::SetUp();
    }

    void TearDown() override {
        TestBase::TearDown();
    }
};

// 测试构建依赖图 - 简单情况
TEST_F(dependency_sorter_test, build_dependency_graph_simple) {
    struct test_item {
        std::string name;
        std::vector<std::string> dependencies;
    };

    std::unordered_map<std::string, test_item> items{
        {"A", {"A", {}}},
        {"B", {"B", {"A"}}},
        {"C", {"C", {"B"}}},
    };

    auto graph = dependency_sorter::build_dependency_graph<test_item>(
        items, [](const test_item& item) { return item.dependencies; });

    EXPECT_EQ(graph.size(), 3);
    EXPECT_EQ(graph["A"].dependencies.size(), 0);
    EXPECT_EQ(graph["B"].dependencies.size(), 1);
    EXPECT_EQ(graph["C"].dependencies.size(), 1);
}

// 测试启动顺序排序 - 简单依赖链
TEST_F(dependency_sorter_test, sort_for_startup_simple_chain) {
    std::unordered_map<std::string, dependency_sorter::dependency_node> graph;

    dependency_sorter::dependency_node node_a;
    node_a.name = "A";
    graph["A"] = node_a;

    dependency_sorter::dependency_node node_b;
    node_b.name = "B";
    node_b.dependencies.insert("A");
    graph["B"] = node_b;

    dependency_sorter::dependency_node node_c;
    node_c.name = "C";
    node_c.dependencies.insert("B");
    graph["C"] = node_c;

    // 设置被依赖关系
    graph["A"].dependents.insert("B");
    graph["B"].dependents.insert("C");

    auto result = dependency_sorter::sort_for_startup(graph);
    EXPECT_EQ(result.size(), 3);

    auto a_pos = std::find(result.begin(), result.end(), "A");
    auto b_pos = std::find(result.begin(), result.end(), "B");
    auto c_pos = std::find(result.begin(), result.end(), "C");

    EXPECT_LT(a_pos - result.begin(), b_pos - result.begin());
    EXPECT_LT(b_pos - result.begin(), c_pos - result.begin());
}

// 测试启动顺序排序 - 循环依赖
TEST_F(dependency_sorter_test, sort_for_startup_circular_dependency) {
    std::unordered_map<std::string, dependency_sorter::dependency_node> graph;

    dependency_sorter::dependency_node node_a;
    node_a.name = "A";
    node_a.dependencies.insert("C");
    graph["A"] = node_a;

    dependency_sorter::dependency_node node_b;
    node_b.name = "B";
    node_b.dependencies.insert("A");
    graph["B"] = node_b;

    dependency_sorter::dependency_node node_c;
    node_c.name = "C";
    node_c.dependencies.insert("B");
    graph["C"] = node_c;

    graph["A"].dependents.insert("B");
    graph["B"].dependents.insert("C");
    graph["C"].dependents.insert("A");

    EXPECT_THROW(dependency_sorter::sort_for_startup(graph), mc::parse_error_exception);
}

// 测试停止顺序排序
TEST_F(dependency_sorter_test, sort_for_shutdown_simple_chain) {
    std::unordered_map<std::string, dependency_sorter::dependency_node> graph;

    dependency_sorter::dependency_node node_a;
    node_a.name = "A";
    graph["A"] = node_a;

    dependency_sorter::dependency_node node_b;
    node_b.name = "B";
    node_b.dependencies.insert("A");
    graph["B"] = node_b;

    dependency_sorter::dependency_node node_c;
    node_c.name = "C";
    node_c.dependencies.insert("B");
    graph["C"] = node_c;

    graph["A"].dependents.insert("B");
    graph["B"].dependents.insert("C");

    auto result = dependency_sorter::sort_for_shutdown(graph);
    EXPECT_EQ(result.size(), 3);

    auto a_pos = std::find(result.begin(), result.end(), "A");
    auto b_pos = std::find(result.begin(), result.end(), "B");
    auto c_pos = std::find(result.begin(), result.end(), "C");

    EXPECT_LT(c_pos - result.begin(), b_pos - result.begin());
    EXPECT_LT(b_pos - result.begin(), a_pos - result.begin());
}

// 测试检测循环依赖
TEST_F(dependency_sorter_test, has_circular_dependency) {
    std::unordered_map<std::string, dependency_sorter::dependency_node> graph;

    dependency_sorter::dependency_node node_a;
    node_a.name = "A";
    node_a.dependencies.insert("C");
    graph["A"] = node_a;

    dependency_sorter::dependency_node node_b;
    node_b.name = "B";
    node_b.dependencies.insert("A");
    graph["B"] = node_b;

    dependency_sorter::dependency_node node_c;
    node_c.name = "C";
    node_c.dependencies.insert("B");
    graph["C"] = node_c;

    graph["A"].dependents.insert("B");
    graph["B"].dependents.insert("C");
    graph["C"].dependents.insert("A");

    EXPECT_TRUE(dependency_sorter::has_circular_dependency(graph));
}

// 测试检测无循环依赖
TEST_F(dependency_sorter_test, has_no_circular_dependency) {
    std::unordered_map<std::string, dependency_sorter::dependency_node> graph;

    dependency_sorter::dependency_node node_a;
    node_a.name = "A";
    graph["A"] = node_a;

    dependency_sorter::dependency_node node_b;
    node_b.name = "B";
    node_b.dependencies.insert("A");
    graph["B"] = node_b;

    graph["A"].dependents.insert("B");

    EXPECT_FALSE(dependency_sorter::has_circular_dependency(graph));
}

// 测试构建依赖图时依赖项不在映射中的场景（外部依赖）
TEST_F(dependency_sorter_test, build_dependency_graph_with_external_dependency) {
    struct test_item {
        std::string name;
        std::vector<std::string> dependencies;
    };

    // 创建项目映射，其中项目 "B" 依赖 "A"，但 "A" 不在 items 中（外部依赖）
    std::unordered_map<std::string, test_item> items{
        {"B", {"B", {"A"}}},  // B 依赖 A，但 A 不在 items 中
        {"C", {"C", {"B"}}},  // C 依赖 B
    };

    auto graph = dependency_sorter::build_dependency_graph<test_item>(
        items, [](const test_item& item) { return item.dependencies; });

    // 验证图的大小（只包含 items 中的项目）
    EXPECT_EQ(graph.size(), 2);
    EXPECT_EQ(graph["B"].dependencies.size(), 1);
    EXPECT_EQ(graph["C"].dependencies.size(), 1);

    // 验证外部依赖 "A" 不在图中，所以 "A" 的 dependents 中不应该有 "B"
    // 由于 "A" 不在 graph 中，所以不会添加到 dependents
    EXPECT_EQ(graph["B"].dependents.size(), 1);  // C 依赖 B
    EXPECT_EQ(graph["C"].dependents.size(), 0);

    // 验证 B 的依赖关系包含 A（即使 A 不在图中）
    EXPECT_TRUE(graph["B"].dependencies.find("A") != graph["B"].dependencies.end());
}
