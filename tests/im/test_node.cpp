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
#include <cstdint>
#include <gtest/gtest.h>
#include <mc/im/key_buffer.h>
#include <mc/im/node.h>
#include <string>
#include <vector>

namespace mc::im::tests {

class NodeTest : public ::testing::Test {
protected:
    // 定义默认配置
    using default_config = tree_config<>;
    using node_type      = node<default_config>;
    using edge_type      = typename node_type::edge_type;
    using ref_ptr_type   = typename node_type::ref_ptr_type;

    void SetUp() override {
        // 创建一些测试用的叶子值
        for (int i = 0; i < 5; i++) {
            leaves.push_back(reinterpret_cast<void*>(static_cast<intptr_t>(i + 1)));
        }
    }

    void TearDown() override {
        // 清理测试资源
        nodes.clear();
        leaves.clear();
    }

    // 辅助方法：创建一个带有前缀的节点
    ref_ptr_type create_node_with_prefix(std::optional<void*> leaf, const std::string& prefix) {
        key_buffer<> key_buf(prefix);
        ref_ptr_type node = make_ref<node_type>(leaf, key_buf);
        nodes.push_back(node); // 保存引用以避免内存泄漏
        return node;
    }

    // 辅助方法：验证节点边数量
    void verify_edge_count(const ref_ptr_type& node, size_t expected_count) {
        EXPECT_EQ(node->m_edges.size(), expected_count);
    }

    // 辅助方法：验证节点是否有特定标签的边
    bool has_edge(const ref_ptr_type& node, uint8_t label) {
        auto [idx, child] = node->get_edge(label);
        return idx >= 0;
    }

    // 辅助方法：验证遍历回调收集到的数据
    bool verify_walk_results(const std::vector<std::pair<std::string, void*>>& expected,
                             const std::vector<std::pair<std::string, void*>>& actual) {
        if (expected.size() != actual.size()) {
            return false;
        }

        for (size_t i = 0; i < expected.size(); i++) {
            if (expected[i].first != actual[i].first || expected[i].second != actual[i].second) {
                return false;
            }
        }

        return true;
    }

    // 保存创建的节点，以避免内存泄漏
    std::vector<ref_ptr_type> nodes;

    // 测试用的叶子值
    std::vector<void*> leaves;
};

// 测试节点的基本构造函数
TEST_F(NodeTest, BasicConstruction) {
    // 创建空节点
    auto empty_node = make_ref<node_type>();
    EXPECT_FALSE(empty_node->is_leaf());
    EXPECT_FALSE(empty_node->m_leaf.has_value());
    EXPECT_EQ(empty_node->m_edges.size(), 0);
    EXPECT_EQ(empty_node->m_prefix.size(), 0);

    // 创建带叶子的节点
    auto leaf_node = make_ref<node_type>(leaves[0]);
    EXPECT_TRUE(leaf_node->is_leaf());
    EXPECT_TRUE(leaf_node->m_leaf.has_value());
    EXPECT_EQ(leaf_node->m_leaf.value(), leaves[0]);
    EXPECT_EQ(leaf_node->m_edges.size(), 0);
    EXPECT_EQ(leaf_node->m_prefix.size(), 0);

    // 创建带前缀的节点
    std::string prefix      = "test";
    auto        prefix_node = create_node_with_prefix(leaves[1], prefix);
    EXPECT_TRUE(prefix_node->is_leaf());
    EXPECT_TRUE(prefix_node->m_leaf.has_value());
    EXPECT_EQ(prefix_node->m_leaf.value(), leaves[1]);
    EXPECT_EQ(prefix_node->m_edges.size(), 0);
    EXPECT_EQ(prefix_node->m_prefix.size(), prefix.size());
    EXPECT_EQ(std::string(prefix_node->m_prefix.data(), prefix_node->m_prefix.size()), prefix);
}

// 测试添加和获取边
TEST_F(NodeTest, AddAndGetEdge) {
    // 创建根节点
    auto root = make_ref<node_type>(nullptr);

    // 创建子节点
    auto child1 = make_ref<node_type>(leaves[0]);
    auto child2 = make_ref<node_type>(leaves[1]);
    auto child3 = make_ref<node_type>(leaves[2]);

    // 添加边
    root->add_edge(edge_type('a', child1));
    root->add_edge(edge_type('b', child2));
    root->add_edge(edge_type('c', child3));

    // 验证边数量
    verify_edge_count(root, 3);

    // 验证边是否按升序排列
    EXPECT_LT(root->m_edges[0].m_label, root->m_edges[1].m_label);
    EXPECT_LT(root->m_edges[1].m_label, root->m_edges[2].m_label);

    // 验证获取边
    auto [idx1, node1] = root->get_edge('a');
    EXPECT_EQ(idx1, 0);
    EXPECT_EQ(node1, child1);

    auto [idx2, node2] = root->get_edge('b');
    EXPECT_EQ(idx2, 1);
    EXPECT_EQ(node2, child2);

    auto [idx3, node3] = root->get_edge('c');
    EXPECT_EQ(idx3, 2);
    EXPECT_EQ(node3, child3);

    // 验证获取不存在的边
    auto [idx4, node4] = root->get_edge('d');
    EXPECT_EQ(idx4, -1);
    EXPECT_EQ(node4, nullptr);
}

// 测试替换边
TEST_F(NodeTest, ReplaceEdge) {
    auto root   = make_ref<node_type>(nullptr);
    auto child1 = make_ref<node_type>(leaves[0]);
    auto child2 = make_ref<node_type>(leaves[1]);

    // 添加边
    root->add_edge(edge_type('a', child1));
    verify_edge_count(root, 1);

    // 获取边确认
    auto [idx1, node1] = root->get_edge('a');
    EXPECT_EQ(idx1, 0);
    EXPECT_EQ(node1, child1);

    // 替换边
    root->replace_edge(edge_type('a', child2));
    verify_edge_count(root, 1); // 边数量不变

    // 确认替换成功
    auto [idx2, node2] = root->get_edge('a');
    EXPECT_EQ(idx2, 0);
    EXPECT_EQ(node2, child2);
}

// 测试删除边
TEST_F(NodeTest, DeleteEdge) {
    auto root = make_ref<node_type>(nullptr);

    // 添加多个边
    for (int i = 0; i < 3; i++) {
        auto child = make_ref<node_type>(leaves[i]);
        root->add_edge(edge_type('a' + i, child));
    }

    verify_edge_count(root, 3);

    // 删除中间的边
    root->del_edge('b');
    verify_edge_count(root, 2);
    EXPECT_TRUE(has_edge(root, 'a'));
    EXPECT_FALSE(has_edge(root, 'b'));
    EXPECT_TRUE(has_edge(root, 'c'));

    // 删除首边
    root->del_edge('a');
    verify_edge_count(root, 1);
    EXPECT_FALSE(has_edge(root, 'a'));
    EXPECT_TRUE(has_edge(root, 'c'));

    // 删除尾边
    root->del_edge('c');
    verify_edge_count(root, 0);
    EXPECT_FALSE(has_edge(root, 'c'));

    // 删除不存在的边
    root->del_edge('d');
    verify_edge_count(root, 0); // 不应改变
}

// 测试查找键
TEST_F(NodeTest, GetKey) {
    // 创建一个空的根节点（不是叶子）
    auto root = make_ref<node_type>(std::nullopt);

    // 创建一级节点，a为叶子节点，b不是叶子节点
    auto node_a = create_node_with_prefix(leaves[0], "a");
    auto node_b = create_node_with_prefix(std::nullopt, "b");

    // 创建二级节点 - 只保存相对前缀
    // 注意：对于"ap"路径，p是相对于a的，所以节点只保存"p"
    auto node_p = create_node_with_prefix(leaves[1], "p");
    auto node_c = create_node_with_prefix(leaves[2], "c");

    // 构建树
    root->add_edge(edge_type('a', node_a));
    root->add_edge(edge_type('b', node_b));
    node_a->add_edge(edge_type('p', node_p));
    node_b->add_edge(edge_type('c', node_c));

    // 验证节点结构
    EXPECT_TRUE(node_a->is_leaf());
    EXPECT_TRUE(node_a->m_leaf.has_value());
    EXPECT_EQ(node_a->m_leaf.value(), leaves[0]);
    EXPECT_EQ(std::string(node_a->m_prefix.data(), node_a->m_prefix.size()), "a");

    EXPECT_TRUE(node_p->is_leaf());
    EXPECT_TRUE(node_p->m_leaf.has_value());
    EXPECT_EQ(node_p->m_leaf.value(), leaves[1]);
    EXPECT_EQ(std::string(node_p->m_prefix.data(), node_p->m_prefix.size()), "p");

    // 测试直接查找a节点
    key_buffer<> key_a("a");
    auto         result_a = root->get(key_a);
    EXPECT_TRUE(result_a.has_value());
    EXPECT_EQ(result_a.value(), leaves[0]);

    // 测试查找ap路径 (组合路径)
    key_buffer<> key_ap("ap");
    auto         result_ap = root->get(key_ap);
    EXPECT_TRUE(result_ap.has_value());
    EXPECT_EQ(result_ap.value(), leaves[1]);

    // 测试查找bc路径 (组合路径)
    key_buffer<> key_bc("bc");
    auto         result_bc = root->get(key_bc);
    EXPECT_TRUE(result_bc.has_value());
    EXPECT_EQ(result_bc.value(), leaves[2]);

    // 测试查找不存在的键
    key_buffer<> key_ac("ac");
    auto         result_ac = root->get(key_ac);
    EXPECT_FALSE(result_ac.has_value());

    // b不是叶子节点，所以找不到
    key_buffer<> key_b("b");
    auto         result_b = root->get(key_b);
    EXPECT_FALSE(result_b.has_value());
}

// 测试遍历树
TEST_F(NodeTest, WalkTree) {
    // 构建一个树:
    //     (root)
    //     /    \
    //   (a)    (b)
    //   / \     \
    // (an) (at) (bc)

    auto root    = make_ref<node_type>(nullptr);
    auto node_a  = create_node_with_prefix(leaves[0], "a");
    auto node_b  = create_node_with_prefix(nullptr, "b");
    auto node_an = create_node_with_prefix(leaves[1], "an");
    auto node_at = create_node_with_prefix(leaves[2], "at");
    auto node_bc = create_node_with_prefix(leaves[3], "bc");

    // 构建树
    root->add_edge(edge_type('a', node_a));
    root->add_edge(edge_type('b', node_b));
    node_a->add_edge(edge_type('n', node_an));
    node_a->add_edge(edge_type('t', node_at));
    node_b->add_edge(edge_type('c', node_bc));

    // 收集遍历结果
    std::vector<std::pair<std::string, void*>> walk_results;
    auto collect_fn = [&walk_results](key_view key, void* val) -> bool {
        if (val != nullptr) { // 只收集叶子节点
            walk_results.push_back({std::string(key.data(), key.size()), val});
        }
        return true;
    };

    // 遍历整个树
    root->walk(collect_fn);

    // 预期结果
    std::vector<std::pair<std::string, void*>> expected = {
        {"a", leaves[0]}, {"an", leaves[1]}, {"at", leaves[2]}, {"bc", leaves[3]}};

    EXPECT_TRUE(verify_walk_results(expected, walk_results));

    // 测试条件中止遍历
    walk_results.clear();
    auto limited_fn = [&walk_results](key_view key, const void* val) -> bool {
        walk_results.push_back({std::string(key.data(), key.size()), const_cast<void*>(val)});
        return walk_results.size() < 2; // 只收集前两个结果
    };

    root->walk(limited_fn);
    EXPECT_EQ(walk_results.size(), 2);
}

// 测试使用前缀遍历 - 使用正确的相对前缀设计
TEST_F(NodeTest, WalkPrefix) {
    // 创建根节点 (空前缀)
    auto root = make_ref<node_type>(nullptr);

    // 创建一级节点 - 设置相对前缀
    auto node_a = create_node_with_prefix(leaves[0], "a");
    auto node_b = create_node_with_prefix(nullptr, "b");

    // 创建二级节点 - 设置相对前缀
    auto node_n = create_node_with_prefix(leaves[1], "n"); // 只包含'n'，不是'an'
    auto node_t = create_node_with_prefix(leaves[2], "t"); // 只包含't'，不是'at'
    auto node_c = create_node_with_prefix(leaves[3], "c"); // 只包含'c'，不是'bc'

    // 构建树
    root->add_edge(edge_type('a', node_a));
    root->add_edge(edge_type('b', node_b));
    node_a->add_edge(edge_type('n', node_n)); // 'a' -> 'n' 代表 "an"
    node_a->add_edge(edge_type('t', node_t)); // 'a' -> 't' 代表 "at"
    node_b->add_edge(edge_type('c', node_c)); // 'b' -> 'c' 代表 "bc"

    // 收集遍历结果
    std::vector<std::pair<std::string, void*>> walk_results;
    auto collect_fn = [&walk_results](key_view key, void* val) -> bool {
        if (val != nullptr) { // 只收集叶子节点
            walk_results.push_back({std::string(key.data(), key.size()), val});
        }
        return true;
    };

    // 测试以'a'为前缀的遍历
    walk_results.clear();
    key_buffer<> prefix_a("a");
    root->walk_prefix(prefix_a, collect_fn);

    // 验证结果 - 由于前缀树的特性，使用"a"查询可能会匹配所有以"a"开头的路径
    // 所以可能会找到a节点本身以及它的子节点 a->n 和 a->t
    EXPECT_GT(walk_results.size(), 0);

    // 检查是否包含a节点
    bool found_a = false;
    for (const auto& result : walk_results) {
        if (result.second == leaves[0]) {
            found_a = true;
            break;
        }
    }
    EXPECT_TRUE(found_a);

    // 现在测试完整的树遍历，应该收集所有叶子节点
    walk_results.clear();
    root->walk(collect_fn);

    // 应该有4个叶子节点
    EXPECT_EQ(walk_results.size(), 4);

    // 测试输入具体的路径
    walk_results.clear();
    key_buffer<> key_an("an");
    auto         result_an = root->get(key_an);
    EXPECT_TRUE(result_an.has_value());
    EXPECT_EQ(result_an.value(), leaves[1]);

    walk_results.clear();
    key_buffer<> key_at("at");
    auto         result_at = root->get(key_at);
    EXPECT_TRUE(result_at.has_value());
    EXPECT_EQ(result_at.value(), leaves[2]);

    walk_results.clear();
    key_buffer<> key_bc("bc");
    auto         result_bc = root->get(key_bc);
    EXPECT_TRUE(result_bc.has_value());
    EXPECT_EQ(result_bc.value(), leaves[3]);
}

// 测试降序节点
TEST_F(NodeTest, ReverseNodeOrder) {
    // 创建降序排序的节点
    using reverse_config    = tree_config<void*, std::allocator<char>, false>;
    using reverse_node_type = node<reverse_config>;
    auto root               = make_ref<reverse_node_type>(nullptr);

    // 添加多个边
    for (int i = 0; i < 3; i++) {
        auto child = make_ref<reverse_node_type>(leaves[i]);
        root->add_edge(typename reverse_node_type::edge_type('a' + i, child));
    }

    EXPECT_EQ(root->m_edges.size(), 3);

    // 验证降序排列
    EXPECT_GT(root->m_edges[0].m_label, root->m_edges[1].m_label);
    EXPECT_GT(root->m_edges[1].m_label, root->m_edges[2].m_label);

    // 验证边顺序
    EXPECT_EQ(root->m_edges[0].m_label, 'c');
    EXPECT_EQ(root->m_edges[1].m_label, 'b');
    EXPECT_EQ(root->m_edges[2].m_label, 'a');

    // 测试查找
    auto [idx_a, node_a] = root->get_edge('a');
    EXPECT_EQ(idx_a, 2);

    auto [idx_b, node_b] = root->get_edge('b');
    EXPECT_EQ(idx_b, 1);

    auto [idx_c, node_c] = root->get_edge('c');
    EXPECT_EQ(idx_c, 0);
}

} // namespace mc::im::tests