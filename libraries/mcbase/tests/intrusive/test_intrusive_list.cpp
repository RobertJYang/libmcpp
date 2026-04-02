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

#include <mc/intrusive.h>

#include <vector>

namespace {

struct list_node : mc::intrusive::list_base_hook<> {
    explicit list_node(int v) : value(v)
    {}

    int value;
};

struct slist_node : mc::intrusive::slist_base_hook<> {
    explicit slist_node(int v) : value(v)
    {}

    int value;
};

} // namespace

TEST(intrusive_list_test, preserves_order_and_supports_reverse_iteration)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<false>> nodes;

    list_node first(1);
    list_node second(2);
    list_node third(3);

    nodes.push_back(first);
    nodes.push_back(second);
    nodes.push_front(third);

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }

    EXPECT_EQ((std::vector<int>{3, 1, 2}), values);
    EXPECT_EQ(nodes.size(), 3U);
    EXPECT_FALSE(nodes.empty());

    std::vector<int> reverse_values;
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        reverse_values.push_back(it->value);
    }

    EXPECT_EQ((std::vector<int>{2, 1, 3}), reverse_values);
    nodes.clear();
}

TEST(intrusive_list_test, supports_iterator_to_erase_and_clear_and_dispose)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<false>> nodes;

    auto* first  = new list_node(1);
    auto* second = new list_node(2);
    auto* third  = new list_node(3);

    nodes.push_back(*first);
    nodes.push_back(*second);
    nodes.push_back(*third);

    nodes.erase(nodes.iterator_to(*second));

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }
    EXPECT_EQ((std::vector<int>{1, 3}), values);

    int disposed = 0;
    delete second;
    nodes.clear_and_dispose([&](list_node* node) {
        ++disposed;
        delete node;
    });

    EXPECT_EQ(disposed, 2);
    EXPECT_TRUE(nodes.empty());
    EXPECT_EQ(nodes.size(), 0U);
}

TEST(intrusive_list_test, insert_at_position)
{
    mc::intrusive::list<list_node> nodes;

    list_node first(1);
    list_node second(2);
    list_node third(3);

    nodes.push_back(first);
    nodes.push_back(third);
    // Insert second before third
    nodes.insert(nodes.iterator_to(third), second);

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }
    EXPECT_EQ((std::vector<int>{1, 2, 3}), values);
}

TEST(intrusive_list_test, const_iterator_and_cbegin_cend)
{
    mc::intrusive::list<list_node> nodes;

    list_node first(1);
    list_node second(2);
    nodes.push_back(first);
    nodes.push_back(second);

    const auto& const_nodes = nodes;
    std::vector<int> values;
    for (auto it = const_nodes.begin(); it != const_nodes.end(); ++it) {
        values.push_back(it->value);
    }
    EXPECT_EQ((std::vector<int>{1, 2}), values);

    values.clear();
    for (auto it = nodes.cbegin(); it != nodes.cend(); ++it) {
        values.push_back(it->value);
    }
    EXPECT_EQ((std::vector<int>{1, 2}), values);
}

TEST(intrusive_list_test, crbegin_crend)
{
    mc::intrusive::list<list_node> nodes;

    list_node first(1);
    list_node second(2);
    nodes.push_back(first);
    nodes.push_back(second);

    std::vector<int> values;
    for (auto it = nodes.crbegin(); it != nodes.crend(); ++it) {
        values.push_back(it->value);
    }
    EXPECT_EQ((std::vector<int>{2, 1}), values);
}

TEST(intrusive_list_test, nonconst_to_const_iterator_conversion)
{
    mc::intrusive::list<list_node> nodes;

    list_node first(1);
    nodes.push_back(first);

    mc::intrusive::list<list_node>::iterator it = nodes.begin();
    mc::intrusive::list<list_node>::const_iterator cit(it);
    EXPECT_EQ(it == cit, true);
    EXPECT_EQ(cit->value, 1);
}

TEST(intrusive_list_test, const_iterator_to)
{
    mc::intrusive::list<list_node> nodes;

    list_node first(1);
    list_node second(2);
    nodes.push_back(first);
    nodes.push_back(second);

    const auto& const_nodes = nodes;
    auto cit = const_nodes.iterator_to(first);
    ASSERT_NE(cit, const_nodes.end());
    EXPECT_EQ(cit->value, 1);
}

TEST(intrusive_list_test, constant_time_size_true)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> nodes;

    list_node first(1);
    list_node second(2);
    nodes.push_back(first);
    nodes.push_back(second);
    EXPECT_EQ(nodes.size(), 2U);

    nodes.erase(nodes.iterator_to(first));
    EXPECT_EQ(nodes.size(), 1U);

    nodes.clear();
    EXPECT_EQ(nodes.size(), 0U);
    EXPECT_TRUE(nodes.empty());
}

TEST(intrusive_list_test, erase_end_iterator_is_noop)
{
    mc::intrusive::list<list_node> nodes;

    list_node first(1);
    nodes.push_back(first);

    auto result = nodes.erase(nodes.end());
    EXPECT_EQ(result, nodes.end());
    EXPECT_EQ(nodes.size(), 1U);
}

TEST(intrusive_slist_test, supports_push_front_iteration_and_clear)
{
    mc::intrusive::slist<slist_node, mc::intrusive::constant_time_size<false>> nodes;

    slist_node first(1);
    slist_node second(2);
    slist_node third(3);

    nodes.push_front(first);
    nodes.push_front(second);
    nodes.push_front(third);

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }

    EXPECT_EQ((std::vector<int>{3, 2, 1}), values);
    EXPECT_EQ(nodes.size(), 3U);
    EXPECT_FALSE(nodes.empty());

    nodes.clear();
    EXPECT_TRUE(nodes.empty());
    EXPECT_EQ(nodes.size(), 0U);
}

TEST(intrusive_list_test, pop_front)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> nodes;

    list_node first(1);
    list_node second(2);
    list_node third(3);

    nodes.push_back(first);
    nodes.push_back(second);
    nodes.push_back(third);

    nodes.pop_front();
    EXPECT_EQ(nodes.size(), 2U);

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }
    EXPECT_EQ(values, (std::vector<int>{2, 3}));
}

TEST(intrusive_list_test, pop_back)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> nodes;

    list_node first(1);
    list_node second(2);
    list_node third(3);

    nodes.push_back(first);
    nodes.push_back(second);
    nodes.push_back(third);

    nodes.pop_back();
    EXPECT_EQ(nodes.size(), 2U);

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }
    EXPECT_EQ(values, (std::vector<int>{1, 2}));
}

TEST(intrusive_list_test, pop_front_empty_is_noop)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> nodes;
    nodes.pop_front();
    EXPECT_TRUE(nodes.empty());
    EXPECT_EQ(nodes.size(), 0U);
}

TEST(intrusive_list_test, pop_back_empty_is_noop)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> nodes;
    nodes.pop_back();
    EXPECT_TRUE(nodes.empty());
    EXPECT_EQ(nodes.size(), 0U);
}

TEST(intrusive_list_test, pop_front_single_element)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> nodes;
    list_node single(42);
    nodes.push_back(single);

    nodes.pop_front();
    EXPECT_TRUE(nodes.empty());
    EXPECT_EQ(nodes.size(), 0U);
}

TEST(intrusive_list_test, pop_back_single_element)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> nodes;
    list_node single(42);
    nodes.push_back(single);

    nodes.pop_back();
    EXPECT_TRUE(nodes.empty());
    EXPECT_EQ(nodes.size(), 0U);
}

TEST(intrusive_list_test, splice_before_position)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> list_a;
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> list_b;

    list_node a1(1);
    list_node a2(2);
    list_a.push_back(a1);
    list_a.push_back(a2);

    list_node b1(10);
    list_node b2(20);
    list_b.push_back(b1);
    list_b.push_back(b2);

    // Splice b before a2: expected a = [1, 10, 20, 2]
    auto it = list_a.begin();
    ++it; // points to a2
    list_a.splice(it, list_b);

    EXPECT_EQ(list_a.size(), 4U);
    EXPECT_TRUE(list_b.empty());

    std::vector<int> values;
    for (const auto& node : list_a) {
        values.push_back(node.value);
    }
    EXPECT_EQ(values, (std::vector<int>{1, 10, 20, 2}));
}

TEST(intrusive_list_test, splice_at_beginning)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> list_a;
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> list_b;

    list_node a1(1);
    list_node a2(2);
    list_a.push_back(a1);
    list_a.push_back(a2);

    list_node b1(10);
    list_node b2(20);
    list_b.push_back(b1);
    list_b.push_back(b2);

    // Splice b before head: expected a = [10, 20, 1, 2]
    list_a.splice(list_a.begin(), list_b);

    EXPECT_EQ(list_a.size(), 4U);
    EXPECT_TRUE(list_b.empty());

    std::vector<int> values;
    for (const auto& node : list_a) {
        values.push_back(node.value);
    }
    EXPECT_EQ(values, (std::vector<int>{10, 20, 1, 2}));
}

TEST(intrusive_list_test, splice_at_end)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> list_a;
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> list_b;

    list_node a1(1);
    list_node a2(2);
    list_a.push_back(a1);
    list_a.push_back(a2);

    list_node b1(10);
    list_node b2(20);
    list_b.push_back(b1);
    list_b.push_back(b2);

    // Splice b at end: expected a = [1, 2, 10, 20]
    list_a.splice(list_b);

    EXPECT_EQ(list_a.size(), 4U);
    EXPECT_TRUE(list_b.empty());

    std::vector<int> values;
    for (const auto& node : list_a) {
        values.push_back(node.value);
    }
    EXPECT_EQ(values, (std::vector<int>{1, 2, 10, 20}));
}

TEST(intrusive_list_test, splice_empty_source_is_noop)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> list_a;
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> list_b;

    list_node a1(1);
    list_a.push_back(a1);

    list_a.splice(list_b);
    EXPECT_EQ(list_a.size(), 1U);
    EXPECT_TRUE(list_b.empty());
}

TEST(intrusive_list_test, splice_into_empty_list)
{
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> list_a;
    mc::intrusive::list<list_node, mc::intrusive::constant_time_size<true>> list_b;

    list_node b1(10);
    list_node b2(20);
    list_b.push_back(b1);
    list_b.push_back(b2);

    list_a.splice(list_b);

    EXPECT_EQ(list_a.size(), 2U);
    EXPECT_TRUE(list_b.empty());

    std::vector<int> values;
    for (const auto& node : list_a) {
        values.push_back(node.value);
    }
    EXPECT_EQ(values, (std::vector<int>{10, 20}));
}

// ==================== slist additional tests ====================

TEST(intrusive_slist_test, reverse_reverses_order)
{
    mc::intrusive::slist<slist_node> nodes;

    slist_node first(1);
    slist_node second(2);
    slist_node third(3);

    nodes.push_front(first);
    nodes.push_front(second);
    nodes.push_front(third);

    // Order: 3, 2, 1
    nodes.reverse();

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }
    EXPECT_EQ((std::vector<int>{1, 2, 3}), values);
}

TEST(intrusive_slist_test, const_iterator_and_cbegin_cend)
{
    mc::intrusive::slist<slist_node> nodes;

    slist_node first(1);
    slist_node second(2);
    nodes.push_front(first);
    nodes.push_front(second);

    const auto& const_nodes = nodes;
    std::vector<int> values;
    for (auto it = const_nodes.begin(); it != const_nodes.end(); ++it) {
        values.push_back(it->value);
    }
    EXPECT_EQ((std::vector<int>{2, 1}), values);

    values.clear();
    for (auto it = nodes.cbegin(); it != nodes.cend(); ++it) {
        values.push_back(it->value);
    }
    EXPECT_EQ((std::vector<int>{2, 1}), values);
}

TEST(intrusive_slist_test, nonconst_to_const_iterator_conversion)
{
    mc::intrusive::slist<slist_node> nodes;

    slist_node first(1);
    nodes.push_front(first);

    mc::intrusive::slist<slist_node>::iterator it = nodes.begin();
    mc::intrusive::slist<slist_node>::const_iterator cit(it);
    EXPECT_EQ(it == cit, true);
    EXPECT_EQ(cit->value, 1);
}

TEST(intrusive_slist_test, constant_time_size_true)
{
    mc::intrusive::slist<slist_node, mc::intrusive::constant_time_size<true>> nodes;

    slist_node first(1);
    slist_node second(2);
    nodes.push_front(first);
    nodes.push_front(second);
    EXPECT_EQ(nodes.size(), 2U);

    nodes.clear();
    EXPECT_EQ(nodes.size(), 0U);
    EXPECT_TRUE(nodes.empty());
}

TEST(intrusive_slist_test, pop_front)
{
    mc::intrusive::slist<slist_node, mc::intrusive::constant_time_size<true>> nodes;

    slist_node first(1);
    slist_node second(2);
    slist_node third(3);

    nodes.push_front(third);
    nodes.push_front(second);
    nodes.push_front(first);

    // Order: 1 -> 2 -> 3
    nodes.pop_front();
    EXPECT_EQ(nodes.size(), 2U);

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }
    EXPECT_EQ(values, (std::vector<int>{2, 3}));
}

TEST(intrusive_slist_test, pop_front_empty_is_noop)
{
    mc::intrusive::slist<slist_node, mc::intrusive::constant_time_size<true>> nodes;
    nodes.pop_front();
    EXPECT_TRUE(nodes.empty());
    EXPECT_EQ(nodes.size(), 0U);
}

TEST(intrusive_slist_test, erase_by_iterator)
{
    mc::intrusive::slist<slist_node, mc::intrusive::constant_time_size<true>> nodes;

    slist_node first(1);
    slist_node second(2);
    slist_node third(3);

    nodes.push_front(third);
    nodes.push_front(second);
    nodes.push_front(first);

    // Order: 1 -> 2 -> 3, erase 2
    auto it = nodes.begin();
    ++it;
    auto next_it = nodes.erase(it);

    EXPECT_EQ(nodes.size(), 2U);
    EXPECT_EQ(next_it->value, 3);

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }
    EXPECT_EQ(values, (std::vector<int>{1, 3}));
}

TEST(intrusive_slist_test, erase_head)
{
    mc::intrusive::slist<slist_node, mc::intrusive::constant_time_size<true>> nodes;

    slist_node first(1);
    slist_node second(2);

    nodes.push_front(second);
    nodes.push_front(first);

    // Order: 1 -> 2, erase head (1)
    auto next_it = nodes.erase(nodes.begin());
    EXPECT_EQ(nodes.size(), 1U);
    EXPECT_EQ(next_it->value, 2);
    EXPECT_EQ(nodes.begin()->value, 2);
}

TEST(intrusive_slist_test, erase_tail)
{
    mc::intrusive::slist<slist_node, mc::intrusive::constant_time_size<true>> nodes;

    slist_node first(1);
    slist_node second(2);
    slist_node third(3);

    nodes.push_front(third);
    nodes.push_front(second);
    nodes.push_front(first);

    // Order: 1 -> 2 -> 3, erase tail (3)
    auto it = nodes.begin();
    ++it; // -> 2
    ++it; // -> 3
    auto next_it = nodes.erase(it);
    EXPECT_EQ(nodes.size(), 2U);
    EXPECT_EQ(next_it, nodes.end());

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }
    EXPECT_EQ(values, (std::vector<int>{1, 2}));
}

TEST(intrusive_slist_test, erase_by_reference)
{
    mc::intrusive::slist<slist_node, mc::intrusive::constant_time_size<true>> nodes;

    slist_node first(1);
    slist_node second(2);
    slist_node third(3);

    nodes.push_front(third);
    nodes.push_front(second);
    nodes.push_front(first);

    nodes.erase(second);
    EXPECT_EQ(nodes.size(), 2U);

    std::vector<int> values;
    for (const auto& node : nodes) {
        values.push_back(node.value);
    }
    EXPECT_EQ(values, (std::vector<int>{1, 3}));
}

TEST(intrusive_slist_test, erase_end_is_noop)
{
    mc::intrusive::slist<slist_node, mc::intrusive::constant_time_size<true>> nodes;

    slist_node first(1);
    nodes.push_front(first);

    auto result = nodes.erase(nodes.end());
    EXPECT_EQ(result, nodes.end());
    EXPECT_EQ(nodes.size(), 1U);
}

TEST(intrusive_slist_test, clear_and_dispose)
{
    mc::intrusive::slist<slist_node, mc::intrusive::constant_time_size<true>> nodes;

    auto* first  = new slist_node(1);
    auto* second = new slist_node(2);
    auto* third  = new slist_node(3);

    nodes.push_front(*third);
    nodes.push_front(*second);
    nodes.push_front(*first);

    std::vector<int> disposed;
    nodes.clear_and_dispose([&disposed](slist_node* node) {
        disposed.push_back(node->value);
        delete node;
    });

    EXPECT_TRUE(nodes.empty());
    EXPECT_EQ(nodes.size(), 0U);
    EXPECT_EQ(disposed.size(), 3U);
}

TEST(intrusive_slist_test, erase_and_dispose_by_iterator)
{
    mc::intrusive::slist<slist_node, mc::intrusive::constant_time_size<true>> nodes;

    auto* first  = new slist_node(1);
    auto* second = new slist_node(2);
    nodes.push_front(*second);
    nodes.push_front(*first);

    slist_node* disposed_node = nullptr;
    nodes.erase_and_dispose(nodes.begin(), [&disposed_node](slist_node* node) {
        disposed_node = node;
    });

    EXPECT_EQ(nodes.size(), 1U);
    ASSERT_NE(disposed_node, nullptr);
    EXPECT_EQ(disposed_node->value, 1);
    delete disposed_node;

    nodes.clear_and_dispose([](slist_node* node) { delete node; });
}

TEST(intrusive_slist_test, erase_and_dispose_by_reference)
{
    mc::intrusive::slist<slist_node, mc::intrusive::constant_time_size<true>> nodes;

    auto* first  = new slist_node(1);
    auto* second = new slist_node(2);
    nodes.push_front(*second);
    nodes.push_front(*first);

    slist_node* disposed_node = nullptr;
    nodes.erase_and_dispose(*first, [&disposed_node](slist_node* node) {
        disposed_node = node;
    });

    EXPECT_EQ(nodes.size(), 1U);
    ASSERT_NE(disposed_node, nullptr);
    EXPECT_EQ(disposed_node->value, 1);
    delete disposed_node;

    nodes.clear_and_dispose([](slist_node* node) { delete node; });
}
