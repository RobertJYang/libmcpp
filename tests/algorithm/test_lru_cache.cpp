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
#include <mc/algorithm/lru_cache.h>

#include <string>
#include <vector>

using namespace mc::algorithm;

// 测试基本的 put 和 get 操作
TEST(lru_cache_test, basic_put_and_get) {
    lru_cache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    EXPECT_EQ(cache.size(), 3);

    auto val1 = cache.get(1);
    ASSERT_TRUE(val1.has_value());
    EXPECT_EQ(val1->get(), "one");

    auto val2 = cache.get(2);
    ASSERT_TRUE(val2.has_value());
    EXPECT_EQ(val2->get(), "two");

    auto val_not_exist = cache.get(999);
    EXPECT_FALSE(val_not_exist.has_value());
}

// 测试 LRU 淘汰机制
TEST(lru_cache_test, lru_eviction) {
    lru_cache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    EXPECT_EQ(cache.size(), 3);

    // 添加第 4 个元素，应该淘汰最久未使用的元素 (1)
    cache.put(4, "four");

    EXPECT_EQ(cache.size(), 3);
    EXPECT_FALSE(cache.get(1).has_value()); // 1 被淘汰了
    EXPECT_TRUE(cache.get(2).has_value());
    EXPECT_TRUE(cache.get(3).has_value());
    EXPECT_TRUE(cache.get(4).has_value());
}

// 测试访问更新顺序
TEST(lru_cache_test, access_updates_order) {
    lru_cache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    // 访问 1，使其成为最近使用
    cache.get(1);

    // 添加新元素，应该淘汰 2（最久未使用）
    cache.put(4, "four");

    EXPECT_TRUE(cache.get(1).has_value());  // 1 仍然存在
    EXPECT_FALSE(cache.get(2).has_value()); // 2 被淘汰了
    EXPECT_TRUE(cache.get(3).has_value());
    EXPECT_TRUE(cache.get(4).has_value());
}

// 测试更新已存在的键
TEST(lru_cache_test, update_existing_key) {
    lru_cache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    // 更新键 1 的值
    cache.put(1, "ONE");

    auto val = cache.get(1);
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val->get(), "ONE");

    // 更新后，1 成为最近使用，添加新元素应该淘汰 2
    cache.put(4, "four");

    EXPECT_TRUE(cache.get(1).has_value());
    EXPECT_FALSE(cache.get(2).has_value());
    EXPECT_TRUE(cache.get(3).has_value());
    EXPECT_TRUE(cache.get(4).has_value());
}

// 测试清空缓存
TEST(lru_cache_test, clear_cache) {
    lru_cache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    EXPECT_EQ(cache.size(), 3);

    cache.clear();

    EXPECT_EQ(cache.size(), 0);
    EXPECT_TRUE(cache.empty());
    EXPECT_FALSE(cache.get(1).has_value());
}

// 测试 erase 操作
TEST(lru_cache_test, erase_key) {
    lru_cache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    EXPECT_TRUE(cache.erase(2));
    EXPECT_EQ(cache.size(), 2);
    EXPECT_FALSE(cache.get(2).has_value());

    // 删除不存在的键
    EXPECT_FALSE(cache.erase(999));
}

// 测试 contains 操作
TEST(lru_cache_test, contains_key) {
    lru_cache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");

    EXPECT_TRUE(cache.contains(1));
    EXPECT_TRUE(cache.contains(2));
    EXPECT_FALSE(cache.contains(3));
}

// 测试设置最大容量
TEST(lru_cache_test, set_max_size) {
    lru_cache<int, std::string> cache(5);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");
    cache.put(4, "four");
    cache.put(5, "five");

    EXPECT_EQ(cache.size(), 5);

    // 减小容量到 3，应该淘汰 2 个最久未使用的条目
    cache.set_max_size(3);

    EXPECT_EQ(cache.size(), 3);
    EXPECT_FALSE(cache.get(1).has_value()); // 最久未使用
    EXPECT_FALSE(cache.get(2).has_value()); // 次久未使用
    EXPECT_TRUE(cache.get(3).has_value());
    EXPECT_TRUE(cache.get(4).has_value());
    EXPECT_TRUE(cache.get(5).has_value());
}

// 测试不限制容量（max_size = 0）
TEST(lru_cache_test, unlimited_size) {
    lru_cache<int, std::string> cache(0); // 不限制

    for (int i = 0; i < 1000; i++) {
        cache.put(i, std::to_string(i));
    }

    EXPECT_EQ(cache.size(), 1000);

    // 所有元素都应该存在
    for (int i = 0; i < 1000; i++) {
        EXPECT_TRUE(cache.get(i).has_value());
    }
}

// 测试淘汰回调函数
TEST(lru_cache_test, eviction_callback) {
    std::vector<int> evicted_keys;

    auto callback = [&evicted_keys](const int& key, std::string&& value) {
        evicted_keys.push_back(key);
    };

    lru_cache<int, std::string> cache(3, callback);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    EXPECT_EQ(evicted_keys.size(), 0);

    // 添加第 4 个元素，触发淘汰
    cache.put(4, "four");

    EXPECT_EQ(evicted_keys.size(), 1);
    EXPECT_EQ(evicted_keys[0], 1); // 1 被淘汰了

    // 再添加一个元素
    cache.put(5, "five");

    EXPECT_EQ(evicted_keys.size(), 2);
    EXPECT_EQ(evicted_keys[1], 2); // 2 被淘汰了
}

// 测试 for_each 遍历
TEST(lru_cache_test, for_each_iteration) {
    lru_cache<int, std::string> cache(5);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    std::vector<int> keys;
    cache.for_each([&keys](const int& key, const std::string& value) {
        keys.push_back(key);
    });

    EXPECT_EQ(keys.size(), 3);
    // 顺序应该是从最近使用到最久未使用
    EXPECT_EQ(keys[0], 3); // 最近添加
    EXPECT_EQ(keys[1], 2);
    EXPECT_EQ(keys[2], 1); // 最早添加
}

// 测试使用字符串作为键
TEST(lru_cache_test, string_key) {
    lru_cache<std::string, int> cache(3);

    cache.put("one", 1);
    cache.put("two", 2);
    cache.put("three", 3);

    auto val = cache.get("two");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val->get(), 2);

    cache.put("four", 4);

    EXPECT_FALSE(cache.get("one").has_value()); // "one" 被淘汰了
    EXPECT_TRUE(cache.get("two").has_value());
}

// 测试复杂对象作为值
TEST(lru_cache_test, complex_value_type) {
    struct data {
        std::string      name;
        int              value;
        std::vector<int> numbers;
    };

    lru_cache<int, data> cache(2);

    cache.put(1, {"first", 100, {1, 2, 3}});
    cache.put(2, {"second", 200, {4, 5, 6}});

    auto val = cache.get(1);
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val->get().name, "first");
    EXPECT_EQ(val->get().value, 100);
    EXPECT_EQ(val->get().numbers.size(), 3);
}

// 测试空缓存行为
TEST(lru_cache_test, empty_cache) {
    lru_cache<int, std::string> cache(3);

    EXPECT_TRUE(cache.empty());
    EXPECT_EQ(cache.size(), 0);
    EXPECT_FALSE(cache.get(1).has_value());
    EXPECT_FALSE(cache.erase(1));
    EXPECT_FALSE(cache.contains(1));
}

// 测试修改 get 返回的引用
TEST(lru_cache_test, modify_via_get_reference) {
    lru_cache<int, std::string> cache(3);

    cache.put(1, "one");

    auto val = cache.get(1);
    ASSERT_TRUE(val.has_value());

    // 通过引用修改值
    val->get() = "ONE";

    auto val2 = cache.get(1);
    ASSERT_TRUE(val2.has_value());
    EXPECT_EQ(val2->get(), "ONE");
}
