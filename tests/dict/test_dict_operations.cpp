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

/**
 * @file test_dict_operations.cpp
 * @brief 测试 dict 和 dict 类的各种操作方法
 */
#include <algorithm>
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <vector>

using namespace mc;

// 测试 dict 的基本访问操作
TEST(DictOperationsTest, DictBasicAccess) {
    dict d({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 operator[] - 非 const 版本可以自动创建键（类似 std::map）
    EXPECT_EQ(d["key1"], 123);
    EXPECT_EQ(d["key2"], "value");
    EXPECT_EQ(d["key3"], true);

    // 测试 const operator[] - 访问不存在的键会抛出异常
    const dict& const_d = d;
    EXPECT_THROW(const_d["key4"], std::out_of_range);

    // 或者使用 at() 方法，无论 const 还是非 const 都会抛出异常
    EXPECT_THROW(d.at("key4"), std::out_of_range);

    // 测试 get 方法
    EXPECT_EQ(d.get("key1", 0), 123);
    EXPECT_EQ(d.get("key4", 456), 456);

    // 测试 contains 方法
    EXPECT_TRUE(d.contains("key1"));
    EXPECT_TRUE(d.contains("key2"));
    EXPECT_TRUE(d.contains("key3"));
    EXPECT_FALSE(d.contains("key4"));

    // 测试 size 和 empty 方法
    EXPECT_EQ(d.size(), 3);
    EXPECT_FALSE(d.empty());

    // 测试 at 方法
    EXPECT_EQ(d.at_index(0).key, "key1");
    EXPECT_EQ(d.at_index(0).value, 123);
    EXPECT_EQ(d.at_index(1).key, "key2");
    EXPECT_EQ(d.at_index(1).value, "value");
    EXPECT_EQ(d.at_index(2).key, "key3");
    EXPECT_EQ(d.at_index(2).value, true);
    EXPECT_THROW(d.at_index(3), std::out_of_range);

    // 测试 find_index 方法
    EXPECT_EQ(d.find_index("key1"), 0);
    EXPECT_EQ(d.find_index("key2"), 1);
    EXPECT_EQ(d.find_index("key3"), 2);
    EXPECT_EQ(d.find_index("key4"), -1);
}

// 测试 dict 的迭代器操作
TEST(DictOperationsTest, DictIterators) {
    dict d({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试迭代器遍历
    std::vector<variant> keys;
    std::vector<variant> values;

    for (const auto& entry : d) {
        keys.push_back(entry.key);
        values.push_back(entry.value);
    }

    EXPECT_EQ(keys.size(), 3);
    EXPECT_EQ(values.size(), 3);

    // 验证键的顺序
    EXPECT_EQ(keys[0], "key1");
    EXPECT_EQ(keys[1], "key2");
    EXPECT_EQ(keys[2], "key3");

    // 验证值的内容
    EXPECT_EQ(values[0], 123);
    EXPECT_EQ(values[1], "value");
    EXPECT_EQ(values[2], true);

    // 测试 begin 和 end 方法
    auto it = d.begin();
    EXPECT_EQ(it->key, "key1");
    EXPECT_EQ(it->value, 123);

    ++it;
    EXPECT_EQ(it->key, "key2");
    EXPECT_EQ(it->value, "value");

    ++it;
    EXPECT_EQ(it->key, "key3");
    EXPECT_EQ(it->value, true);

    ++it;
    EXPECT_EQ(it, d.end());
}

// 测试 dict 的 find 方法
TEST(DictOperationsTest, DictFind) {
    dict d({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 find 方法 (std::string 版本)
    auto it1 = d.find(std::string("key1"));
    EXPECT_NE(it1, d.end());
    EXPECT_EQ(it1->key, "key1");
    EXPECT_EQ(it1->value, 123);

    // 测试 find 方法 (std::string_view 版本)
    auto it2 = d.find(std::string_view("key2"));
    EXPECT_NE(it2, d.end());
    EXPECT_EQ(it2->key, "key2");
    EXPECT_EQ(it2->value, "value");

    // 测试 find 方法 (const char* 版本)
    auto it3 = d.find("key3");
    EXPECT_NE(it3, d.end());
    EXPECT_EQ(it3->key, "key3");
    EXPECT_EQ(it3->value, true);

    // 测试查找不存在的键
    auto it4 = d.find("key4");
    EXPECT_EQ(it4, d.end());
}

// 测试 dict 的 keys 和 values 方法
TEST(DictOperationsTest, DictKeysAndValues) {
    dict d({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 keys 方法
    std::vector<variant> keys = d.keys();
    EXPECT_EQ(keys.size(), 3);
    EXPECT_EQ(keys[0], "key1");
    EXPECT_EQ(keys[1], "key2");
    EXPECT_EQ(keys[2], "key3");

    // 测试 values 方法
    std::vector<variant> values = d.values();
    EXPECT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 123);
    EXPECT_EQ(values[1], "value");
    EXPECT_EQ(values[2], true);
}

// 测试 dict 的比较操作
TEST(DictOperationsTest, DictComparison) {
    dict d1({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    dict d2({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    dict d3({{"key1", 456}, {"key2", "value"}, {"key3", true}});

    dict d4({{"key1", 123}, {"key2", "value"}});

    dict d5({{"key1", 123}, {"key2", "value"}, {"key4", false}});

    // 测试相等比较
    EXPECT_TRUE(d1 == d2);
    EXPECT_FALSE(d1 == d3);
    EXPECT_FALSE(d1 == d4);
    EXPECT_FALSE(d1 == d5);

    // 测试不等比较
    EXPECT_FALSE(d1 != d2);
    EXPECT_TRUE(d1 != d3);
    EXPECT_TRUE(d1 != d4);
    EXPECT_TRUE(d1 != d5);
}

// 测试 dict 的基本修改操作
TEST(DictOperationsTest, MutableDictBasicModification) {
    dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 operator[] 修改现有键值对
    md["key1"] = 456;
    EXPECT_EQ(md["key1"], 456);

    // 测试 operator[] 添加新键值对
    md["key4"] = "new value";
    EXPECT_EQ(md.size(), 4);
    EXPECT_EQ(md["key4"], "new value");

    // 测试 operator() 修改现有键值对
    md("key2", "modified value");
    EXPECT_EQ(md["key2"], "modified value");

    // 测试 operator() 添加新键值对
    md("key5", false);
    EXPECT_EQ(md.size(), 5);
    EXPECT_EQ(md["key5"], false);

    // 测试链式调用 operator()
    md("key6", 789)("key7", "chain")("key8", true);
    EXPECT_EQ(md.size(), 8);
    EXPECT_EQ(md["key6"], 789);
    EXPECT_EQ(md["key7"], "chain");
    EXPECT_EQ(md["key8"], true);
}

// 测试 dict 的 erase 和 clear 方法
TEST(DictOperationsTest, MutableDictEraseAndClear) {
    dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 erase 方法
    EXPECT_TRUE(md.erase("key1"));
    EXPECT_EQ(md.size(), 2);
    EXPECT_FALSE(md.contains("key1"));

    // 测试 erase 不存在的键
    EXPECT_FALSE(md.erase("key1"));
    EXPECT_EQ(md.size(), 2);

    // 测试 clear 方法
    md.clear();
    EXPECT_EQ(md.size(), 0);
    EXPECT_TRUE(md.empty());
}

// 测试 dict 的迭代器操作
TEST(DictOperationsTest, MutableDictIterators) {
    dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试迭代器遍历并修改值
    for (auto& entry : md) {
        if (entry.key == "key1") {
            entry.value = 456;
        } else if (entry.key == "key2") {
            entry.value = "modified";
        }
    }

    EXPECT_EQ(md["key1"], 456);
    EXPECT_EQ(md["key2"], "modified");
    EXPECT_EQ(md["key3"], true);

    // 测试 begin 和 end 方法
    auto it = md.begin();
    EXPECT_EQ(it->key, "key1");
    EXPECT_EQ(it->value, 456);

    ++it;
    EXPECT_EQ(it->key, "key2");
    EXPECT_EQ(it->value, "modified");

    ++it;
    EXPECT_EQ(it->key, "key3");
    EXPECT_EQ(it->value, true);

    ++it;
    EXPECT_EQ(it, md.end());
}

// 测试 dict 的 at 方法
TEST(DictOperationsTest, MutableDictAt) {
    dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 at 方法读取
    EXPECT_EQ(md.at_index(0).key, "key1");
    EXPECT_EQ(md.at_index(0).value, 123);
    EXPECT_EQ(md.at_index(1).key, "key2");
    EXPECT_EQ(md.at_index(1).value, "value");
    EXPECT_EQ(md.at_index(2).key, "key3");
    EXPECT_EQ(md.at_index(2).value, true);
    EXPECT_THROW(md.at_index(3), std::out_of_range);

    // 测试 at 方法修改
    md.at_index(0).value = 456;
    EXPECT_EQ(md["key1"], 456);

    md.at_index(1).value = "modified";
    EXPECT_EQ(md["key2"], "modified");
}

// 测试 dict 和 dict 之间的数据共享
TEST(DictOperationsTest, DataSharing) {
    dict d({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    dict md1({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 从 dict 创建 dict
    dict d2 = md1;

    // 从 dict 创建另一个 dict
    dict md2 = d2;

    // 修改 md1，验证 d 和 md2 也被修改
    md1["key1"] = 456;
    EXPECT_EQ(d2["key1"], 456);
    EXPECT_EQ(md2["key1"], 456);

    // 修改 md2，验证 d 和 md1 也被修改
    md2["key2"] = "modified";
    EXPECT_EQ(d2["key2"], "modified");
    EXPECT_EQ(md1["key2"], "modified");

    // 添加新键值对到 md1，验证 d 和 md2 也被修改
    md1["key4"] = 789;
    EXPECT_EQ(d2.size(), 4);
    EXPECT_EQ(d2["key4"], 789);
    EXPECT_EQ(md2.size(), 4);
    EXPECT_EQ(md2["key4"], 789);

    // 从 md2 删除键值对，验证 d 和 md1 也被修改
    md2.erase("key3");
    EXPECT_EQ(d2.size(), 3);
    EXPECT_FALSE(d2.contains("key3"));
    EXPECT_EQ(md1.size(), 3);
    EXPECT_FALSE(md1.contains("key3"));
}

// 测试 dict 的 find 方法
TEST(DictOperationsTest, MutableDictFind) {
    dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 find 方法 (std::string 版本)
    auto it1 = md.find(std::string("key1"));
    EXPECT_NE(it1, md.end());
    EXPECT_EQ(it1->key, "key1");
    EXPECT_EQ(it1->value, 123);

    // 修改找到的元素
    it1->value = 456;
    EXPECT_EQ(md["key1"], 456);

    // 测试 find 方法 (std::string_view 版本)
    auto it2 = md.find(std::string_view("key2"));
    EXPECT_NE(it2, md.end());
    EXPECT_EQ(it2->key, "key2");
    EXPECT_EQ(it2->value, "value");

    // 修改找到的元素
    it2->value = "modified";
    EXPECT_EQ(md["key2"], "modified");

    // 测试 find 方法 (const char* 版本)
    auto it3 = md.find("key3");
    EXPECT_NE(it3, md.end());
    EXPECT_EQ(it3->key, "key3");
    EXPECT_EQ(it3->value, true);

    // 修改找到的元素
    it3->value = false;
    EXPECT_EQ(md["key3"], false);

    // 测试查找不存在的键
    auto it4 = md.find("key4");
    EXPECT_EQ(it4, md.end());

    // 测试 const 版本的 find 方法
    const dict& cmd = md;

    auto it5 = cmd.find("key1");
    EXPECT_NE(it5, cmd.end());
    EXPECT_EQ(it5->key, "key1");
    EXPECT_EQ(it5->value, 456);

    auto it6 = cmd.find("key4");
    EXPECT_EQ(it6, cmd.end());
}

// 测试 dict 的 insert 方法
TEST(DictOperationsTest, MutableDictInsert) {
    // 测试基本的insert方法
    {
        dict md;

        // 测试insert返回值类型
        auto result = md.insert("key1", 123);
        EXPECT_TRUE(result.second); // 成功插入
        EXPECT_EQ(result.first->key, "key1");
        EXPECT_EQ(result.first->value, 123);

        // 测试插入已存在的键
        result = md.insert("key1", 456);
        EXPECT_FALSE(result.second); // 插入失败
        EXPECT_EQ(result.first->key, "key1");
        EXPECT_EQ(result.first->value, 123); // 值保持不变

        // 测试插入其他类型
        md.insert("key2", "value");
        md.insert("key3", true);

        EXPECT_EQ(md.size(), 3);
        EXPECT_EQ(md["key1"], 123);
        EXPECT_EQ(md["key2"], "value");
        EXPECT_EQ(md["key3"], true);
    }

    // 测试带hint的insert方法
    {
        dict md;
        md.insert("key1", 100);
        md.insert("key3", 300);

        // 使用hint在key1和key3之间插入key2
        auto it        = md.find("key3");
        auto result_it = md.insert(it, "key2", 200);

        EXPECT_EQ(result_it->key, "key2");
        EXPECT_EQ(result_it->value, 200);

        // 验证顺序
        auto keys = md.keys();
        EXPECT_EQ(keys.size(), 3);
        EXPECT_EQ(keys[0], "key1");
        EXPECT_EQ(keys[1], "key2"); // key2被插入到了key3之前
        EXPECT_EQ(keys[2], "key3");
    }

    // 测试插入entry
    {
        dict        md;
        dict::entry e("key1", 123);

        md.insert(std::move(e));
        EXPECT_EQ(md.size(), 1);
        EXPECT_EQ(md["key1"], 123);
    }

    // 测试批量插入（迭代器范围）
    {
        std::vector<dict::entry> entries;
        entries.push_back(dict::entry("key1", 100));
        entries.push_back(dict::entry("key2", 200));

        dict md;
        md.insert(entries.begin(), entries.end());

        EXPECT_EQ(md.size(), 2);
        EXPECT_EQ(md["key1"], 100);
        EXPECT_EQ(md["key2"], 200);
    }

    // 测试初始化列表插入
    {
        dict md;
        md.insert({{"key1", 100}, {"key2", 200}});

        EXPECT_EQ(md.size(), 2);
        EXPECT_EQ(md["key1"], 100);
        EXPECT_EQ(md["key2"], 200);
    }

    // 测试emplace方法
    {
        dict md;

        // 使用emplace插入元素
        auto result = md.emplace("key1", 123);
        EXPECT_TRUE(result.second);
        EXPECT_EQ(result.first->value, 123);

        // 尝试再次emplace同一个键
        result = md.emplace("key1", 456);
        EXPECT_FALSE(result.second);
        EXPECT_EQ(result.first->value, 123); // 值不变

        // 使用emplace插入字符串
        md.emplace("key2", "value");
        EXPECT_EQ(md["key2"], "value");
    }

    // 测试try_emplace方法
    {
        dict md;

        // 使用try_emplace插入新元素
        auto result = md.try_emplace("key1", 123);
        EXPECT_TRUE(result.second);
        EXPECT_EQ(result.first->value, 123);

        // 尝试对已存在的键使用try_emplace
        result = md.try_emplace("key1", 456);
        EXPECT_FALSE(result.second);
        EXPECT_EQ(result.first->value, 123); // 值不变

        // 使用try_emplace插入新元素
        md.try_emplace("key2", "value");
        EXPECT_EQ(md["key2"], "value");
    }

    // 测试模板方法
    {
        dict md;

        // 使用非variant类型调用insert
        std::string key   = "key1";
        int         value = 123;
        md.insert(key, value);

        EXPECT_EQ(md.size(), 1);
        EXPECT_EQ(md["key1"], 123);
    }
}

// 测试 dict 的 insert 方法与其他操作的交互
TEST(DictOperationsTest, MutableDictInsertInteraction) {
    // 测试insert与operator[]的交互
    {
        dict md;

        // 先使用insert插入元素
        md.insert("key1", 100);

        // 然后使用operator[]修改
        md["key1"] = 200;

        // 再次使用insert尝试插入同一个键
        auto result = md.insert("key1", 300);

        EXPECT_FALSE(result.second);         // 插入失败
        EXPECT_EQ(result.first->value, 200); // 值为operator[]修改后的值
    }

    // 测试insert与erase的交互
    {
        dict md;

        // 先插入元素
        md.insert("key1", 100);
        md.insert("key2", 200);

        // 删除一个元素
        EXPECT_TRUE(md.erase("key1"));

        // 再次插入被删除的元素
        auto result = md.insert("key1", 300);
        EXPECT_TRUE(result.second); // 插入成功
        EXPECT_EQ(md["key1"], 300); // 新值

        // 验证顺序 - 应该在末尾
        auto keys = md.keys();
        EXPECT_EQ(keys.size(), 2);
        EXPECT_EQ(keys[0], "key2");
        EXPECT_EQ(keys[1], "key1");
    }

    // 测试insert与clear的交互
    {
        dict md;

        // 先插入元素
        md.insert("key1", 100);
        md.insert("key2", 200);

        // 清空字典
        md.clear();
        EXPECT_EQ(md.size(), 0);

        // 再次插入元素，验证通过 std::pair 插入
        md.insert({std::make_pair("key1", 300), std::make_pair("key2", 400)});

        EXPECT_EQ(md.size(), 2);
        EXPECT_EQ(md["key1"], 300);
        EXPECT_EQ(md["key2"], 400);
    }

    // 测试数据共享
    {
        dict md1;
        md1.insert("key1", 100);

        // 拷贝构造共享数据
        dict md2(md1);

        // 在md2中插入新元素
        md2.insert("key2", 200);

        // 验证修改也影响了md1
        EXPECT_EQ(md1.size(), 2);
        EXPECT_EQ(md1["key1"], 100);
        EXPECT_EQ(md1["key2"], 200);

        // 在md1中插入新元素
        md1.insert("key3", 300);

        // 验证修改也影响了md2
        EXPECT_EQ(md2.size(), 3);
        EXPECT_EQ(md2["key3"], 300);
    }

    // 测试通过 std::map 的迭代器插入
    {
        std::map<std::string, int> map = {{"key1", 100}, {"key2", 200}};
        dict                       md;
        md.insert(map.begin(), map.end());

        EXPECT_EQ(md.size(), 2);
        EXPECT_EQ(md["key1"], 100);
        EXPECT_EQ(md["key2"], 200);
    }
}