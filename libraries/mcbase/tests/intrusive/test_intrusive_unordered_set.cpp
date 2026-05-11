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

namespace {

struct hash_node : mc::intrusive::unordered_set_base_hook<mc::intrusive::link_mode<mc::intrusive::safe_link>> {
    explicit hash_node(int v) : value(v)
    {}

    int value;
};

struct node_hash {
    std::size_t operator()(const hash_node& node) const
    {
        return static_cast<std::size_t>(node.value);
    }

    std::size_t operator()(int value) const
    {
        return static_cast<std::size_t>(value);
    }
};

struct node_equal {
    bool operator()(const hash_node& lhs, const hash_node& rhs) const
    {
        return lhs.value == rhs.value;
    }

    bool operator()(int value, const hash_node& node) const
    {
        return value == node.value;
    }

    bool operator()(const hash_node& node, int value) const
    {
        return node.value == value;
    }
};

} // namespace

TEST(intrusive_unordered_set_test, power_of_two_bucket_count_required)
{
    using node_set =
        mc::intrusive::unordered_set<hash_node, mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
                                     mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[7]; // not power of 2
    EXPECT_DEATH({ node_set nodes(node_set::bucket_traits(buckets, 7)); }, ".*");
}

TEST(intrusive_unordered_set_test, supports_insert_find_erase_and_clear)
{
    using node_set =
        mc::intrusive::unordered_set<hash_node, mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
                                     mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[8];
    node_set              nodes(node_set::bucket_traits(buckets, 8), node_hash(), node_equal());

    hash_node first(1);
    hash_node second(2);
    hash_node third(3);

    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);

    EXPECT_EQ(nodes.size(), 3U);

    auto found = nodes.find(2, nodes.hash_function(), nodes.key_eq());
    ASSERT_NE(found, nodes.end());
    EXPECT_EQ(found->value, 2);

    nodes.erase(found);
    EXPECT_EQ(nodes.size(), 2U);
    EXPECT_EQ(nodes.find(2, nodes.hash_function(), nodes.key_eq()), nodes.end());

    nodes.erase(nodes.iterator_to(first));
    EXPECT_EQ(nodes.size(), 1U);
    EXPECT_EQ(nodes.find(1, nodes.hash_function(), nodes.key_eq()), nodes.end());

    nodes.clear();
    EXPECT_EQ(nodes.size(), 0U);
    EXPECT_TRUE(nodes.empty());
}

TEST(intrusive_unordered_set_test, iterate_all_elements)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[8];
    node_set nodes(node_set::bucket_traits(buckets, 8), node_hash(), node_equal());

    hash_node first(1);
    hash_node second(2);
    hash_node third(3);
    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);

    std::vector<int> values;
    for (auto& node : nodes) {
        values.push_back(node.value);
    }
    std::sort(values.begin(), values.end());
    EXPECT_EQ(values, (std::vector<int>{1, 2, 3}));
}

TEST(intrusive_unordered_set_test, const_iteration_works)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[8];
    node_set nodes(node_set::bucket_traits(buckets, 8), node_hash(), node_equal());

    hash_node first(1);
    nodes.insert(first);

    const auto& const_nodes = nodes;
    auto it = const_nodes.begin();
    ASSERT_NE(it, const_nodes.end());
    EXPECT_EQ(it->value, 1);

    auto cit = const_nodes.cbegin();
    ASSERT_NE(cit, const_nodes.cend());
    EXPECT_EQ(cit->value, 1);
}

TEST(intrusive_unordered_set_test, count_and_contains)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[8];
    node_set nodes(node_set::bucket_traits(buckets, 8), node_hash(), node_equal());

    hash_node first(1);
    hash_node third(3);
    nodes.insert(first);
    nodes.insert(third);

    EXPECT_TRUE(nodes.contains(1));
    EXPECT_TRUE(nodes.contains(3));
    EXPECT_FALSE(nodes.contains(2));

    EXPECT_EQ(nodes.count(1), 1U);
    EXPECT_EQ(nodes.count(2), 0U);
    EXPECT_EQ(nodes.count(3), 1U);
}

TEST(intrusive_unordered_set_test, clear_and_dispose_calls_disposer_on_all_elements)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[8];
    node_set nodes(node_set::bucket_traits(buckets, 8), node_hash(), node_equal());

    auto* first  = new hash_node(1);
    auto* second = new hash_node(2);
    auto* third  = new hash_node(3);
    nodes.insert(*first);
    nodes.insert(*second);
    nodes.insert(*third);

    std::vector<int> disposed;
    nodes.clear_and_dispose([&disposed](hash_node* node) {
        disposed.push_back(node->value);
        delete node;
    });

    EXPECT_EQ(nodes.size(), 0U);
    EXPECT_TRUE(nodes.empty());
    std::sort(disposed.begin(), disposed.end());
    EXPECT_EQ(disposed, (std::vector<int>{1, 2, 3}));
}

TEST(intrusive_unordered_set_test, erase_and_dispose_removes_single_element)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[8];
    node_set nodes(node_set::bucket_traits(buckets, 8), node_hash(), node_equal());

    auto* first  = new hash_node(1);
    auto* second = new hash_node(2);
    nodes.insert(*first);
    nodes.insert(*second);

    hash_node* disposed_node = nullptr;
    nodes.erase_and_dispose(nodes.find(1), [&disposed_node](hash_node* node) {
        disposed_node = node;
    });

    EXPECT_EQ(nodes.size(), 1U);
    ASSERT_NE(disposed_node, nullptr);
    EXPECT_EQ(disposed_node->value, 1);
    EXPECT_EQ(nodes.find(1), nodes.end());
    EXPECT_NE(nodes.find(2), nodes.end());

    delete disposed_node;
    nodes.erase(*second);
    delete second;
}

TEST(intrusive_unordered_set_test, swap_exchanges_contents)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets_a[8];
    node_set::bucket_type buckets_b[8];
    node_set set_a(node_set::bucket_traits(buckets_a, 8), node_hash(), node_equal());
    node_set set_b(node_set::bucket_traits(buckets_b, 8), node_hash(), node_equal());

    hash_node first(1);
    hash_node second(2);
    set_a.insert(first);
    set_b.insert(second);

    set_a.swap(set_b);

    EXPECT_EQ(set_a.size(), 1U);
    EXPECT_EQ(set_b.size(), 1U);
    EXPECT_NE(set_a.find(2), set_a.end());
    EXPECT_NE(set_b.find(1), set_b.end());
}

TEST(intrusive_unordered_set_test, insert_rejects_duplicates)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[8];
    node_set nodes(node_set::bucket_traits(buckets, 8), node_hash(), node_equal());

    hash_node first(1);
    hash_node second(2);

    auto [it1, ok1] = nodes.insert(first);
    EXPECT_TRUE(ok1);
    EXPECT_EQ(it1->value, 1);

    auto [it2, ok2] = nodes.insert(second);
    EXPECT_TRUE(ok2);
    EXPECT_EQ(it2->value, 2);

    // Duplicate insert of first should be rejected
    auto [it3, ok3] = nodes.insert(first);
    EXPECT_FALSE(ok3);
    EXPECT_EQ(it3->value, 1);  // iterator points to existing element

    // Size unchanged
    EXPECT_EQ(nodes.size(), 2U);
}

TEST(intrusive_unordered_set_test, rehash_redistributes_nodes_to_new_buckets)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type small_buckets[4];
    node_set nodes(node_set::bucket_traits(small_buckets, 4), node_hash(), node_equal());

    hash_node first(1);
    hash_node second(2);
    hash_node third(3);
    hash_node fourth(4);
    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);
    nodes.insert(fourth);
    EXPECT_EQ(nodes.size(), 4U);

    // Rehash to larger bucket array
    node_set::bucket_type large_buckets[16];
    nodes.rehash(node_set::bucket_traits(large_buckets, 16));

    // All elements still findable
    EXPECT_EQ(nodes.size(), 4U);
    EXPECT_TRUE(nodes.contains(1));
    EXPECT_TRUE(nodes.contains(2));
    EXPECT_TRUE(nodes.contains(3));
    EXPECT_TRUE(nodes.contains(4));

    // Iteration still works
    std::vector<int> values;
    for (auto& node : nodes) {
        values.push_back(node.value);
    }
    std::sort(values.begin(), values.end());
    EXPECT_EQ(values, (std::vector<int>{1, 2, 3, 4}));
}

TEST(intrusive_unordered_set_test, erase_const_iterator)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[8];
    node_set nodes(node_set::bucket_traits(buckets, 8), node_hash(), node_equal());

    hash_node first(1);
    hash_node second(2);
    nodes.insert(first);
    nodes.insert(second);

    const auto& const_nodes = nodes;
    auto cit = const_nodes.find(1);
    ASSERT_NE(cit, const_nodes.end());
    EXPECT_EQ(cit->value, 1);

    nodes.erase(cit);
    EXPECT_EQ(nodes.size(), 1U);
    EXPECT_FALSE(nodes.contains(1));
    EXPECT_TRUE(nodes.contains(2));
}

TEST(intrusive_unordered_set_test, nonconst_to_const_iterator_conversion)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[8];
    node_set nodes(node_set::bucket_traits(buckets, 8), node_hash(), node_equal());

    hash_node first(1);
    nodes.insert(first);

    node_set::iterator it = nodes.begin();
    node_set::const_iterator cit(it);
    EXPECT_EQ(it == cit, true);
    EXPECT_EQ(cit->value, 1);
}

TEST(intrusive_unordered_set_test, const_iterator_to)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[8];
    node_set nodes(node_set::bucket_traits(buckets, 8), node_hash(), node_equal());

    hash_node first(1);
    hash_node second(2);
    nodes.insert(first);
    nodes.insert(second);

    const auto& const_nodes = nodes;
    auto cit = const_nodes.iterator_to(first);
    ASSERT_NE(cit, const_nodes.end());
    EXPECT_EQ(cit->value, 1);
}

TEST(intrusive_unordered_set_test, erase_and_dispose_by_reference)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[8];
    node_set nodes(node_set::bucket_traits(buckets, 8), node_hash(), node_equal());

    auto* first  = new hash_node(1);
    auto* second = new hash_node(2);
    nodes.insert(*first);
    nodes.insert(*second);

    hash_node* disposed_node = nullptr;
    nodes.erase_and_dispose(*first, [&disposed_node](hash_node* node) {
        disposed_node = node;
    });

    EXPECT_EQ(nodes.size(), 1U);
    ASSERT_NE(disposed_node, nullptr);
    EXPECT_EQ(disposed_node->value, 1);
    EXPECT_FALSE(nodes.contains(1));
    EXPECT_TRUE(nodes.contains(2));

    delete disposed_node;
    nodes.erase(*second);
    delete second;
}

TEST(intrusive_unordered_set_test, empty_table_find_and_contains)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    // Zero-bucket table
    node_set::bucket_type buckets[1];
    node_set nodes(node_set::bucket_traits(buckets, 1), node_hash(), node_equal());

    // No elements inserted — find should return end
    EXPECT_EQ(nodes.find(1), nodes.end());
    EXPECT_FALSE(nodes.contains(1));
    EXPECT_EQ(nodes.count(1), 0U);
    EXPECT_EQ(nodes.size(), 0U);
    EXPECT_TRUE(nodes.empty());

    // Iteration over empty table
    int count = 0;
    for (auto& node : nodes) {
        (void)node;
        ++count;
    }
    EXPECT_EQ(count, 0);
}

TEST(intrusive_unordered_set_test, erase_end_iterator_is_noop)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[8];
    node_set nodes(node_set::bucket_traits(buckets, 8), node_hash(), node_equal());

    hash_node first(1);
    nodes.insert(first);

    // Erasing end() should be a no-op
    nodes.erase(nodes.end());
    EXPECT_EQ(nodes.size(), 1U);
    EXPECT_TRUE(nodes.contains(1));
}

TEST(intrusive_unordered_set_test, constant_time_size_false_uses_count_all)
{
    using node_set = mc::intrusive::unordered_set<hash_node,
        mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
        mc::intrusive::constant_time_size<false>>;

    node_set::bucket_type buckets[8];
    node_set nodes(node_set::bucket_traits(buckets, 8), node_hash(), node_equal());

    hash_node first(1);
    hash_node second(2);
    hash_node third(3);
    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);

    EXPECT_EQ(nodes.size(), 3U);

    nodes.erase(nodes.iterator_to(first));
    EXPECT_EQ(nodes.size(), 2U);

    nodes.clear();
    EXPECT_EQ(nodes.size(), 0U);
    EXPECT_TRUE(nodes.empty());
}

TEST(intrusive_unordered_set_test, bucket_count)
{
    using node_set =
        mc::intrusive::unordered_set<hash_node, mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
                                     mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[16];
    node_set nodes(node_set::bucket_traits(buckets, 16));

    EXPECT_EQ(nodes.bucket_count(), 16U);

    hash_node first(1);
    nodes.insert(first);
    EXPECT_EQ(nodes.bucket_count(), 16U);
}

TEST(intrusive_unordered_set_test, load_factor)
{
    using node_set =
        mc::intrusive::unordered_set<hash_node, mc::intrusive::hash<node_hash>, mc::intrusive::equal<node_equal>,
                                     mc::intrusive::constant_time_size<true>>;

    node_set::bucket_type buckets[16];
    node_set nodes(node_set::bucket_traits(buckets, 16));

    EXPECT_FLOAT_EQ(nodes.load_factor(), 0.0f);

    hash_node first(1);
    hash_node second(2);
    hash_node third(3);
    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);

    EXPECT_FLOAT_EQ(nodes.load_factor(), 3.0f / 16.0f);
}
