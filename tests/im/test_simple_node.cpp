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

#include <gtest/gtest.h>
#include <mc/im/key_buffer.h>
#include <mc/im/radix_tree/node.h>
#include <string>

namespace mc::im::tests {

// 定义默认配置
using default_config    = tree_config<>;
using default_node_type = node<default_config>;

TEST(SimpleNodeTest, BasicNodeFunctionality) {
    // 使用非零值作为叶子值
    void* leaf_val = reinterpret_cast<void*>(1);

    // 创建一个简单的节点
    default_node_type node(leaf_val);

    // 测试基本属性
    EXPECT_TRUE(node.is_leaf());
    EXPECT_TRUE(node.m_leaf.has_value());
    EXPECT_EQ(node.m_leaf.value(), leaf_val);
    EXPECT_EQ(node.m_edges.size(), 0);
    EXPECT_EQ(node.m_prefix.size(), 0);

    // 测试前缀节点
    key_buffer<>      prefix("test");
    default_node_type prefix_node(leaf_val, prefix);
    EXPECT_TRUE(prefix_node.is_leaf());
    EXPECT_TRUE(prefix_node.m_leaf.has_value());
    EXPECT_EQ(prefix_node.m_leaf.value(), leaf_val);
    EXPECT_EQ(prefix_node.m_edges.size(), 0);
    EXPECT_EQ(prefix_node.m_prefix.size(), 4);
    EXPECT_EQ(std::string(prefix_node.m_prefix.data(), prefix_node.m_prefix.size()), "test");
}

TEST(SimpleNodeTest, GetKey) {
    // 使用非零值作为叶子值
    void* leaf1 = reinterpret_cast<void*>(1);
    void* leaf2 = reinterpret_cast<void*>(2);

    // 创建一个简单节点
    default_node_type node(leaf1);

    // 测试空键查找
    key_buffer<> empty_key;
    auto         result = node.get(empty_key);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), leaf1);

    // 创建一个前缀节点
    key_buffer<>      prefix_key("test");
    default_node_type prefix_node(leaf2, prefix_key);

    // 测试前缀匹配
    auto prefix_result = prefix_node.get(prefix_key);
    EXPECT_TRUE(prefix_result.has_value());
    EXPECT_EQ(prefix_result.value(), leaf2);

    // 测试前缀不匹配
    key_buffer<> wrong_key("wrong");
    auto         wrong_result = prefix_node.get(wrong_key);
    EXPECT_FALSE(wrong_result.has_value());
}

TEST(SimpleNodeTest, AddAndGetEdge) {
    // 使用非零值作为叶子值
    void* leaf1 = reinterpret_cast<void*>(1);

    // 创建一个简单的树：root -> a -> child
    auto root  = make_shared<default_node_type>(nullptr);
    auto child = make_shared<default_node_type>(leaf1);

    // 测试基本的边添加和获取
    typename default_node_type::edge_type edge('a', child);
    root->add_edge(edge);

    // 验证边数量
    EXPECT_EQ(root->m_edges.size(), 1);

    // 验证边查找功能
    auto [idx, node] = root->get_edge('a');
    EXPECT_EQ(idx, 0);
    EXPECT_EQ(node, child);

    // 创建更复杂的测试：带有单字符前缀
    auto         root2 = mc::make_shared<default_node_type>(nullptr);
    key_buffer<> prefix_b("b");
    auto         child2 = mc::make_shared<default_node_type>(leaf1, prefix_b); // 使用key_buffer而不是字符串字面量

    root2->add_edge(typename default_node_type::edge_type('b', child2));

    // 查找键 "b"
    key_buffer<> key_b("b");
    auto         result_b = root2->get(key_b);
    EXPECT_TRUE(result_b.has_value());
    EXPECT_EQ(result_b.value(), leaf1);
}

TEST(SimpleNodeTest, SimpleWalkPrefix) {
    // 使用非零值作为叶子值
    void* leaf1 = reinterpret_cast<void*>(1);

    // 创建前缀节点，使用make_ref
    key_buffer<> prefix("test");
    auto         node = mc::make_shared<default_node_type>(leaf1, prefix);

    // 测试遍历
    std::vector<std::pair<std::string, void*>> walk_results;
    auto                                       collect_fn = [&walk_results](key_view key, void* val) -> bool {
        walk_results.push_back({std::string(key.data(), key.size()), val});
        return true;
    };

    // 使用空前缀遍历（应该匹配所有节点）
    key_buffer<> empty_prefix;
    node->walk_prefix(empty_prefix, collect_fn);

    // 验证结果
    EXPECT_EQ(walk_results.size(), 1);
    EXPECT_EQ(walk_results[0].first, "test");
    EXPECT_EQ(walk_results[0].second, leaf1);

    // 使用完全匹配的前缀
    walk_results.clear();
    node->walk_prefix(prefix, collect_fn);

    // 验证结果
    EXPECT_EQ(walk_results.size(), 1);
    EXPECT_EQ(walk_results[0].first, "test");
    EXPECT_EQ(walk_results[0].second, leaf1);

    // 使用不匹配的前缀
    walk_results.clear();
    key_buffer<> wrong_prefix("wrong");
    node->walk_prefix(wrong_prefix, collect_fn);

    // 验证结果 - 应该为空
    EXPECT_TRUE(walk_results.empty());
}

} // namespace mc::im::tests