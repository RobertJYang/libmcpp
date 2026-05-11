/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES of any kind,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include <mc/intrusive.h>

namespace {

struct set_node : mc::intrusive::set_base_hook<> {
    explicit set_node(int v) : value(v)
    {}

    int value;

    bool operator<(const set_node& other) const
    {
        return value < other.value;
    }
};

struct set_node_compare {
    bool operator()(const set_node& a, const set_node& b) const
    {
        return a.value < b.value;
    }

    bool operator()(int key, const set_node& b) const
    {
        return key < b.value;
    }

    bool operator()(const set_node& a, int key) const
    {
        return a.value < key;
    }
};

} // namespace

TEST(intrusive_set_test, insert_and_iterate_in_order)
{
    mc::intrusive::set<set_node, mc::intrusive::constant_time_size<true>> nodes;

    set_node first(1);
    set_node second(2);
    set_node third(3);
    set_node fourth(4);

    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);
    nodes.insert(fourth);

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }

    EXPECT_EQ(values, (std::vector<int>{1, 2, 3, 4}));
    EXPECT_EQ(nodes.size(), 4U);
}

TEST(intrusive_set_test, insert_rejects_duplicates)
{
    mc::intrusive::set<set_node, mc::intrusive::constant_time_size<true>> nodes;

    set_node first(1);
    set_node second(2);

    auto [it1, ok1] = nodes.insert(first);
    EXPECT_TRUE(ok1);
    auto [it2, ok2] = nodes.insert(second);
    EXPECT_TRUE(ok2);

    auto [it3, ok3] = nodes.insert(first);
    EXPECT_FALSE(ok3);
    EXPECT_EQ(it3->value, 1);

    EXPECT_EQ(nodes.size(), 2U);
}

TEST(intrusive_set_test, find_contains_count)
{
    mc::intrusive::set<set_node> nodes;

    set_node first(1);
    set_node second(2);
    set_node third(3);

    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);

    EXPECT_NE(nodes.find(first), nodes.end());
    EXPECT_NE(nodes.find(second), nodes.end());
    EXPECT_NE(nodes.find(third), nodes.end());
    EXPECT_EQ(nodes.find(set_node(99)), nodes.end());

    EXPECT_TRUE(nodes.contains(first));
    EXPECT_TRUE(nodes.contains(third));
    EXPECT_FALSE(nodes.contains(set_node(99)));

    EXPECT_EQ(nodes.count(first), 1U);
    EXPECT_EQ(nodes.count(set_node(99)), 0U);
}

TEST(intrusive_set_test, erase_by_iterator)
{
    mc::intrusive::set<set_node, mc::intrusive::constant_time_size<true>> nodes;

    set_node first(1);
    set_node second(2);
    set_node third(3);

    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);

    auto it = nodes.find(second);
    ASSERT_NE(it, nodes.end());
    nodes.erase(it);

    EXPECT_EQ(nodes.size(), 2U);
    EXPECT_EQ(nodes.find(second), nodes.end());

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }
    EXPECT_EQ(values, (std::vector<int>{1, 3}));
}

TEST(intrusive_set_test, erase_by_reference)
{
    mc::intrusive::set<set_node, mc::intrusive::constant_time_size<true>> nodes;

    set_node first(1);
    set_node second(2);
    set_node third(3);

    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);

    nodes.erase(second);

    EXPECT_EQ(nodes.size(), 2U);
    EXPECT_EQ(nodes.find(second), nodes.end());
}

TEST(intrusive_set_test, const_iterator_and_cbegin_cend)
{
    mc::intrusive::set<set_node> nodes;

    set_node first(1);
    set_node second(2);
    nodes.insert(first);
    nodes.insert(second);

    const auto& const_nodes = nodes;
    std::vector<int> values;
    for (auto it = const_nodes.begin(); it != const_nodes.end(); ++it) {
        values.push_back(it->value);
    }
    EXPECT_EQ(values, (std::vector<int>{1, 2}));

    values.clear();
    for (auto it = nodes.cbegin(); it != nodes.cend(); ++it) {
        values.push_back(it->value);
    }
    EXPECT_EQ(values, (std::vector<int>{1, 2}));
}

TEST(intrusive_set_test, reverse_iteration)
{
    mc::intrusive::set<set_node> nodes;

    set_node first(1);
    set_node second(2);
    set_node third(3);
    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);

    std::vector<int> values;
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        values.push_back(it->value);
    }
    EXPECT_EQ(values, (std::vector<int>{3, 2, 1}));
}

TEST(intrusive_set_test, nonconst_to_const_iterator_conversion)
{
    mc::intrusive::set<set_node> nodes;

    set_node first(1);
    nodes.insert(first);

    mc::intrusive::set<set_node>::iterator it = nodes.begin();
    mc::intrusive::set<set_node>::const_iterator cit(it);
    EXPECT_EQ(it == cit, true);
    EXPECT_EQ(cit->value, 1);
}

TEST(intrusive_set_test, iterator_to)
{
    mc::intrusive::set<set_node> nodes;

    set_node first(1);
    set_node second(2);
    nodes.insert(first);
    nodes.insert(second);

    auto it = nodes.iterator_to(first);
    ASSERT_NE(it, nodes.end());
    EXPECT_EQ(it->value, 1);

    const auto& const_nodes = nodes;
    auto cit = const_nodes.iterator_to(first);
    ASSERT_NE(cit, const_nodes.end());
    EXPECT_EQ(cit->value, 1);
}

TEST(intrusive_set_test, lower_bound_and_upper_bound)
{
    mc::intrusive::set<set_node> nodes;

    set_node n1(1);
    set_node n2(2);
    set_node n3(3);
    set_node n5(5);
    nodes.insert(n1);
    nodes.insert(n3);
    nodes.insert(n5);

    // lower_bound(2) => first element >= 2, which is 3
    auto lb = nodes.lower_bound(set_node(2));
    ASSERT_NE(lb, nodes.end());
    EXPECT_EQ(lb->value, 3);

    // upper_bound(2) => first element > 2, which is 3
    auto ub = nodes.upper_bound(set_node(2));
    ASSERT_NE(ub, nodes.end());
    EXPECT_EQ(ub->value, 3);

    // lower_bound(3) => 3 itself
    lb = nodes.lower_bound(n3);
    ASSERT_NE(lb, nodes.end());
    EXPECT_EQ(lb->value, 3);

    // upper_bound(3) => 5
    ub = nodes.upper_bound(n3);
    ASSERT_NE(ub, nodes.end());
    EXPECT_EQ(ub->value, 5);

    // lower_bound(0) => 1
    lb = nodes.lower_bound(set_node(0));
    ASSERT_NE(lb, nodes.end());
    EXPECT_EQ(lb->value, 1);

    // upper_bound(6) => end
    ub = nodes.upper_bound(set_node(6));
    EXPECT_EQ(ub, nodes.end());
}

TEST(intrusive_set_test, equal_range)
{
    mc::intrusive::set<set_node> nodes;

    set_node n1(1);
    set_node n2(2);
    set_node n3(3);
    nodes.insert(n1);
    nodes.insert(n2);
    nodes.insert(n3);

    auto [lo, hi] = nodes.equal_range(n2);
    EXPECT_EQ(lo->value, 2);
    EXPECT_EQ(hi->value, 3);

    // No duplicates of 4, so equal_range(4) is empty
    auto [lo2, hi2] = nodes.equal_range(set_node(4));
    EXPECT_EQ(lo2, hi2);
}

TEST(intrusive_set_test, clear_and_dispose)
{
    mc::intrusive::set<set_node, mc::intrusive::constant_time_size<true>> nodes;

    auto* first  = new set_node(1);
    auto* second = new set_node(2);
    auto* third  = new set_node(3);
    nodes.insert(*first);
    nodes.insert(*second);
    nodes.insert(*third);

    std::vector<int> disposed;
    nodes.clear_and_dispose([&disposed](set_node* node) {
        disposed.push_back(node->value);
        delete node;
    });

    EXPECT_TRUE(nodes.empty());
    EXPECT_EQ(nodes.size(), 0U);
    std::sort(disposed.begin(), disposed.end());
    EXPECT_EQ(disposed, (std::vector<int>{1, 2, 3}));
}

TEST(intrusive_set_test, erase_and_dispose)
{
    mc::intrusive::set<set_node, mc::intrusive::constant_time_size<true>> nodes;

    auto* first  = new set_node(1);
    auto* second = new set_node(2);
    nodes.insert(*first);
    nodes.insert(*second);

    set_node* disposed_node = nullptr;
    nodes.erase_and_dispose(nodes.find(*first), [&disposed_node](set_node* node) {
        disposed_node = node;
    });

    EXPECT_EQ(nodes.size(), 1U);
    ASSERT_NE(disposed_node, nullptr);
    EXPECT_EQ(disposed_node->value, 1);
    EXPECT_EQ(nodes.find(*first), nodes.end());

    delete disposed_node;
    nodes.clear_and_dispose([](set_node* node) { delete node; });
}

TEST(intrusive_set_test, swap_exchanges_contents)
{
    mc::intrusive::set<set_node, mc::intrusive::constant_time_size<true>> set_a;
    mc::intrusive::set<set_node, mc::intrusive::constant_time_size<true>> set_b;

    set_node a1(1);
    set_node a2(2);
    set_a.insert(a1);
    set_a.insert(a2);

    set_node b1(10);
    set_node b2(20);
    set_b.insert(b1);
    set_b.insert(b2);

    set_a.swap(set_b);

    EXPECT_EQ(set_a.size(), 2U);
    EXPECT_EQ(set_b.size(), 2U);
    EXPECT_NE(set_a.find(b1), set_a.end());
    EXPECT_NE(set_b.find(a1), set_b.end());
}

TEST(intrusive_set_test, constant_time_size_false)
{
    mc::intrusive::set<set_node, mc::intrusive::constant_time_size<false>> nodes;

    set_node first(1);
    set_node second(2);
    set_node third(3);
    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);

    EXPECT_EQ(nodes.size(), 3U);

    nodes.erase(second);
    EXPECT_EQ(nodes.size(), 2U);

    nodes.clear();
    EXPECT_EQ(nodes.size(), 0U);
    EXPECT_TRUE(nodes.empty());
}

TEST(intrusive_set_test, empty_set_operations)
{
    mc::intrusive::set<set_node> nodes;

    EXPECT_TRUE(nodes.empty());
    EXPECT_EQ(nodes.size(), 0U);
    EXPECT_EQ(nodes.begin(), nodes.end());
    EXPECT_EQ(nodes.find(set_node(1)), nodes.end());
}

TEST(intrusive_set_test, erase_end_iterator_is_noop)
{
    mc::intrusive::set<set_node> nodes;

    set_node first(1);
    nodes.insert(first);

    auto result = nodes.erase(nodes.end());
    EXPECT_EQ(result, nodes.end());
    EXPECT_EQ(nodes.size(), 1U);
}

TEST(intrusive_set_test, value_comp_accessor)
{
    mc::intrusive::set<set_node, mc::intrusive::compare<set_node_compare>> nodes;

    set_node first(1);
    nodes.insert(first);

    (void)nodes.value_comp();
}

TEST(intrusive_set_test, custom_compare)
{
    struct descending_compare {
        bool operator()(const set_node& a, const set_node& b) const
        {
            return a.value > b.value;
        }
    };

    mc::intrusive::set<set_node, mc::intrusive::compare<descending_compare>> nodes;

    set_node first(1);
    set_node second(2);
    set_node third(3);

    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }
    EXPECT_EQ(values, (std::vector<int>{3, 2, 1}));
}

TEST(intrusive_set_test, insert_many_maintains_balance)
{
    mc::intrusive::set<set_node, mc::intrusive::constant_time_size<true>> nodes;

    // Insert many nodes in various orders and verify order
    for (int i = 0; i < 100; ++i) {
        auto* n = new set_node(i);
        nodes.insert(*n);
    }

    EXPECT_EQ(nodes.size(), 100U);

    // Verify sorted order
    int expected = 0;
    for (const auto& node : nodes) {
        EXPECT_EQ(node.value, expected);
        ++expected;
    }

    nodes.clear_and_dispose([](set_node* node) { delete node; });
}

TEST(intrusive_set_test, key_comp_accessor)
{
    mc::intrusive::set<set_node, mc::intrusive::compare<set_node_compare>> nodes;

    set_node first(1);
    set_node second(2);
    nodes.insert(first);
    nodes.insert(second);

    auto cmp = nodes.key_comp();
    EXPECT_TRUE(cmp(first, second));
    EXPECT_FALSE(cmp(second, first));
}
