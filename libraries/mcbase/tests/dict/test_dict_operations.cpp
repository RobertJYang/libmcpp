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
TEST(DictOperationsTest, DictBasicAccess)
{
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
TEST(DictOperationsTest, DictIterators)
{
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
TEST(DictOperationsTest, DictFind)
{
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
TEST(DictOperationsTest, DictKeysAndValues)
{
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
TEST(DictOperationsTest, DictComparison)
{
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
TEST(DictOperationsTest, MutableDictBasicModification)
{
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

    // 测试特殊值：0 应该被识别为整数而不是 null
    md["zero_int"] = 0;
    EXPECT_FALSE(md["zero_int"].is_null()) << "0 应该被识别为整数，而不是 null";
    EXPECT_TRUE(md["zero_int"].is_integer()) << "0 应该是整数类型";
    EXPECT_EQ(md["zero_int"].as<int>(), 0) << "0 的值应该是 0";

    // 测试其他整数类型的 0
    md["zero_uint"] = static_cast<uint32_t>(0);
    EXPECT_FALSE(md["zero_uint"].is_null());
    EXPECT_TRUE(md["zero_uint"].is_integer());
    EXPECT_EQ(md["zero_uint"].as<uint32_t>(), 0U);

    // 测试浮点数 0.0
    md["zero_double"] = 0.0;
    EXPECT_FALSE(md["zero_double"].is_null());
    EXPECT_TRUE(md["zero_double"].is_double());
    EXPECT_EQ(md["zero_double"].as<double>(), 0.0);

    // 测试 bool 类型
    md["bool_true"] = true;
    EXPECT_FALSE(md["bool_true"].is_null());
    EXPECT_TRUE(md["bool_true"].is_bool());
    EXPECT_EQ(md["bool_true"].as<bool>(), true);

    // 测试 nullptr 应该被识别为 null
    md["null_key"] = nullptr;
    EXPECT_TRUE(md["null_key"].is_null());
}

// 测试 dict 的 erase 和 clear 方法
TEST(DictOperationsTest, MutableDictEraseAndClear)
{
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
TEST(DictOperationsTest, MutableDictIterators)
{
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
TEST(DictOperationsTest, MutableDictAt)
{
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
TEST(DictOperationsTest, DataSharing)
{
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

// 测试 dict 的浅拷贝 copy() 方法
TEST(DictOperationsTest, ShallowCopy)
{
    // 测试基本的浅拷贝 - 字典数据结构本身不共享
    {
        dict d1({{"key1", 123}, {"key2", "value"}, {"key3", true}});

        // 使用 copy() 创建浅拷贝
        dict d2 = d1.copy();

        // 验证拷贝后的对象包含相同的数据
        EXPECT_EQ(d2.size(), 3);
        EXPECT_EQ(d2["key1"], 123);
        EXPECT_EQ(d2["key2"], "value");
        EXPECT_EQ(d2["key3"], true);

        // 验证 d1 和 d2 不共享字典数据结构（浅拷贝）
        d2["key1"] = 456;
        EXPECT_EQ(d1["key1"], 123); // d1 不应该被修改
        EXPECT_EQ(d2["key1"], 456);

        // 添加新键值对到 d2，不影响 d1
        d2["key4"] = 789;
        EXPECT_EQ(d1.size(), 3); // d1 大小不变
        EXPECT_EQ(d2.size(), 4);
        EXPECT_FALSE(d1.contains("key4"));
        EXPECT_TRUE(d2.contains("key4"));

        // 删除 d2 的键值对，不影响 d1
        d2.erase("key3");
        EXPECT_EQ(d1.size(), 3);
        EXPECT_EQ(d2.size(), 3);
        EXPECT_TRUE(d1.contains("key3"));  // d1 仍然包含 key3
        EXPECT_FALSE(d2.contains("key3")); // d2 不包含 key3
    }

    // 测试 const dict 的浅拷贝
    {
        const dict d1({{"key1", 123}, {"key2", "value"}, {"key3", true}});

        // const 版本的 copy() 也应该工作
        dict d2 = d1.copy();

        EXPECT_EQ(d2.size(), 3);
        EXPECT_EQ(d2["key1"], 123);
        EXPECT_EQ(d2["key2"], "value");
        EXPECT_EQ(d2["key3"], true);

        // 修改 d2，验证 d1 不受影响
        d2["key1"] = 456;
        EXPECT_EQ(d1["key1"], 123); // d1 保持不变
        EXPECT_EQ(d2["key1"], 456);
    }

    // 测试链式拷贝 - 每次都是独立的拷贝
    {
        dict d1({{"key1", 123}, {"key2", "value"}});
        dict d2 = d1.copy();
        dict d3 = d2.copy();

        // 三个对象不共享字典数据结构
        d3["key1"] = 999;
        EXPECT_EQ(d1["key1"], 123); // d1 不变
        EXPECT_EQ(d2["key1"], 123); // d2 不变
        EXPECT_EQ(d3["key1"], 999); // 仅 d3 改变

        d1["key3"] = true;
        EXPECT_EQ(d1.size(), 3);
        EXPECT_EQ(d2.size(), 2); // d2 大小不变
        EXPECT_EQ(d3.size(), 2); // d3 大小不变
        EXPECT_TRUE(d1.contains("key3"));
        EXPECT_FALSE(d2.contains("key3"));
        EXPECT_FALSE(d3.contains("key3"));
    }

    // 测试空 dict 的浅拷贝
    {
        dict d1;
        dict d2 = d1.copy();

        EXPECT_TRUE(d2.empty());
        EXPECT_EQ(d2.size(), 0);

        // 向拷贝添加元素，不影响原始 dict
        d2["key1"] = 123;
        EXPECT_EQ(d1.size(), 0); // d1 仍然为空
        EXPECT_EQ(d2.size(), 1);
        EXPECT_FALSE(d1.contains("key1"));
        EXPECT_TRUE(d2.contains("key1"));
    }

    // 测试嵌套 dict 的浅拷贝 - 外层字典独立，但内层 variant 值可能共享
    {
        dict inner({{"inner_key", 123}});
        dict outer({{"outer_key", inner}});

        dict outer_copy = outer.copy();

        // 验证外层字典数据结构独立
        outer_copy["new_key"] = "new_value";
        EXPECT_EQ(outer.size(), 1);      // outer 大小不变
        EXPECT_EQ(outer_copy.size(), 2); // outer_copy 增加了元素
        EXPECT_FALSE(outer.contains("new_key"));
        EXPECT_TRUE(outer_copy.contains("new_key"));

        // 浅拷贝：内层的 dict 对象（作为 variant 值）仍然共享
        dict inner_dict1         = outer["outer_key"].as<dict>();
        dict inner_dict2         = outer_copy["outer_key"].as<dict>();
        inner_dict1["inner_key"] = 456;

        // 因为 inner_dict1 和 inner_dict2 引用同一个内层 dict，修改会相互影响
        EXPECT_EQ(inner_dict2["inner_key"], 456);
        EXPECT_EQ(outer["outer_key"].as<dict>()["inner_key"], 456);
        EXPECT_EQ(outer_copy["outer_key"].as<dict>()["inner_key"], 456);
    }

    // 对比测试：引用行为（默认拷贝构造）vs 浅拷贝
    {
        dict d1({{"key1", 123}, {"key2", "value"}});

        // 默认拷贝构造 - 引用行为，共享数据
        dict d2_ref = d1;

        // 浅拷贝 - 不共享字典数据结构
        dict d2_copy = d1.copy();

        // 修改 d1
        d1["key1"] = 456;
        d1["key3"] = true;

        // 引用行为：d2_ref 受影响
        EXPECT_EQ(d2_ref["key1"], 456);
        EXPECT_EQ(d2_ref.size(), 3);
        EXPECT_TRUE(d2_ref.contains("key3"));

        // 浅拷贝：d2_copy 不受影响
        EXPECT_EQ(d2_copy["key1"], 123);
        EXPECT_EQ(d2_copy.size(), 2);
        EXPECT_FALSE(d2_copy.contains("key3"));
    }
}

// 测试 dict 的深拷贝 deep_copy() 方法
TEST(DictOperationsTest, DeepCopy)
{
    // 测试基本的深拷贝 - 字典数据结构和值都不共享
    {
        dict d1({{"key1", 123}, {"key2", "value"}, {"key3", true}});

        // 使用 deep_copy() 创建深拷贝
        dict d2 = d1.deep_copy();

        // 验证拷贝后的对象包含相同的数据
        EXPECT_EQ(d2.size(), 3);
        EXPECT_EQ(d2["key1"], 123);
        EXPECT_EQ(d2["key2"], "value");
        EXPECT_EQ(d2["key3"], true);

        // 验证 d1 和 d2 完全独立（深拷贝）
        d2["key1"] = 456;
        EXPECT_EQ(d1["key1"], 123); // d1 不应该被修改
        EXPECT_EQ(d2["key1"], 456);

        // 添加新键值对到 d2，不影响 d1
        d2["key4"] = 789;
        EXPECT_EQ(d1.size(), 3); // d1 大小不变
        EXPECT_EQ(d2.size(), 4);
        EXPECT_FALSE(d1.contains("key4"));
        EXPECT_TRUE(d2.contains("key4"));

        // 删除 d2 的键值对，不影响 d1
        d2.erase("key3");
        EXPECT_EQ(d1.size(), 3);
        EXPECT_EQ(d2.size(), 3);
        EXPECT_TRUE(d1.contains("key3"));  // d1 仍然包含 key3
        EXPECT_FALSE(d2.contains("key3")); // d2 不包含 key3
    }

    // 测试嵌套 dict 的深拷贝 - 外层和内层都完全独立
    {
        dict inner({{"inner_key", 123}});
        dict outer({{"outer_key", inner}});

        dict outer_deep = outer.deep_copy();

        // 验证外层字典数据结构独立
        outer_deep["new_key"] = "new_value";
        EXPECT_EQ(outer.size(), 1);      // outer 大小不变
        EXPECT_EQ(outer_deep.size(), 2); // outer_deep 增加了元素
        EXPECT_FALSE(outer.contains("new_key"));
        EXPECT_TRUE(outer_deep.contains("new_key"));

        // 深拷贝：内层的 dict 对象也应该独立
        dict inner_dict1         = outer["outer_key"].as<dict>();
        dict inner_dict2         = outer_deep["outer_key"].as<dict>();
        inner_dict1["inner_key"] = 456;

        // 深拷贝后，修改原始字典的内层不影响拷贝
        EXPECT_EQ(inner_dict1["inner_key"], 456);
        EXPECT_EQ(outer["outer_key"].as<dict>()["inner_key"], 456);
        EXPECT_EQ(inner_dict2["inner_key"], 123);                        // inner_dict2 保持原值
        EXPECT_EQ(outer_deep["outer_key"].as<dict>()["inner_key"], 123); // outer_deep 保持原值

        // 修改拷贝的内层 dict
        inner_dict2["inner_key"] = 999;
        EXPECT_EQ(outer["outer_key"].as<dict>()["inner_key"], 456); // outer 不受影响
        EXPECT_EQ(outer_deep["outer_key"].as<dict>()["inner_key"], 999);
    }

    // 测试多层嵌套的深拷贝
    {
        dict level3({{"level3_key", "deep"}});
        dict level2({{"level2_key", level3}});
        dict level1({{"level1_key", level2}});

        dict level1_deep = level1.deep_copy();

        // 修改最深层的值
        dict l3_orig          = level1["level1_key"].as<dict>()["level2_key"].as<dict>();
        l3_orig["level3_key"] = "modified";

        // 验证深拷贝的对象不受影响
        EXPECT_EQ(level1["level1_key"].as<dict>()["level2_key"].as<dict>()["level3_key"], "modified");
        EXPECT_EQ(level1_deep["level1_key"].as<dict>()["level2_key"].as<dict>()["level3_key"], "deep");
    }

    // 测试包含数组的深拷贝
    {
        variants arr = {1, 2, 3};
        dict     d1({{"array", arr}, {"value", 100}});

        dict d2 = d1.deep_copy();

        // 修改原始字典中的数组
        variants arr1 = d1["array"].as<variants>();
        arr1[0]       = 999;
        d1["array"]   = arr1;

        // 验证深拷贝的数组不受影响
        EXPECT_EQ(d1["array"].as<variants>()[0], 999);
        EXPECT_EQ(d2["array"].as<variants>()[0], 1); // 深拷贝的数组保持原值
    }

    // 测试空 dict 的深拷贝
    {
        dict d1;
        dict d2 = d1.deep_copy();

        EXPECT_TRUE(d2.empty());
        EXPECT_EQ(d2.size(), 0);

        // 向拷贝添加元素，不影响原始 dict
        d2["key1"] = 123;
        EXPECT_EQ(d1.size(), 0); // d1 仍然为空
        EXPECT_EQ(d2.size(), 1);
        EXPECT_FALSE(d1.contains("key1"));
        EXPECT_TRUE(d2.contains("key1"));
    }

    // 对比测试：引用、浅拷贝、深拷贝三种行为
    {
        dict inner({{"inner", 100}});
        dict d1({{"nested", inner}, {"value", 200}});

        // 引用（默认拷贝）
        dict d2_ref = d1;

        // 浅拷贝
        dict d2_shallow = d1.copy();

        // 深拷贝
        dict d2_deep = d1.deep_copy();

        // 修改原始字典的外层
        d1["value"] = 999;
        d1["new"]   = "added";

        // 修改原始字典的内层
        dict inner_dict     = d1["nested"].as<dict>();
        inner_dict["inner"] = 888;

        // 引用行为：完全共享
        EXPECT_EQ(d2_ref["value"], 999);
        EXPECT_TRUE(d2_ref.contains("new"));
        EXPECT_EQ(d2_ref["nested"].as<dict>()["inner"], 888);

        // 浅拷贝：外层独立，内层共享
        EXPECT_EQ(d2_shallow["value"], 200); // 外层独立
        EXPECT_FALSE(d2_shallow.contains("new"));
        EXPECT_EQ(d2_shallow["nested"].as<dict>()["inner"], 888); // 内层共享

        // 深拷贝：完全独立
        EXPECT_EQ(d2_deep["value"], 200); // 外层独立
        EXPECT_FALSE(d2_deep.contains("new"));
        EXPECT_EQ(d2_deep["nested"].as<dict>()["inner"], 100); // 内层也独立
    }

    // 测试多个 dict 相互引用的深拷贝（非循环引用）
    // 注意：这里是共享引用，不是真正的循环引用
    {
        dict d1({{"key1", 123}});
        dict d2({{"ref_to_d1", d1}, {"key2", 456}});
        dict d3({{"ref_to_d2", d2}, {"ref_to_d1", d1}});

        dict d3_deep = d3.deep_copy();

        // 修改 d1
        d1["key1"] = 999;

        // 验证深拷贝完全独立
        EXPECT_EQ(d1["key1"], 999);
        EXPECT_EQ(d3["ref_to_d2"].as<dict>()["ref_to_d1"].as<dict>()["key1"], 999);
        EXPECT_EQ(d3_deep["ref_to_d2"].as<dict>()["ref_to_d1"].as<dict>()["key1"], 123);
        EXPECT_EQ(d3_deep["ref_to_d1"].as<dict>()["key1"], 123);
    }
}

// 测试 dict 深拷贝的循环引用处理
TEST(DictOperationsTest, DeepCopyCycleHandling)
{
    // 测试自引用 - 深拷贝应该保持引用关系
    {
        dict d1({{"key1", 123}});
        d1["self"] = d1; // 创建自引用

        // 深拷贝应该保持自引用关系
        dict d2 = d1.deep_copy();

        // 验证拷贝成功
        EXPECT_EQ(d2["key1"], 123);
        EXPECT_TRUE(d2.contains("self"));

        // 验证 d2["self"] 指向 d2 自己（保持自引用）
        dict self_ref = d2["self"].as<dict>();
        EXPECT_EQ(self_ref["key1"], 123);

        // 关键验证：使用 data() 指针直接验证 d2["self"] 就是 d2 本身
        dict self_dict = d2["self"].as<dict>();
        EXPECT_EQ(self_dict.data(), d2.data()); // ✅ 指针相同，确认是同一对象

        // 通过修改验证引用关系
        d2["key1"] = 456;
        EXPECT_EQ(d2["self"].as<dict>()["key1"], 456); // self 引用应该看到修改

        // 验证 d2 和 d1 是独立的
        EXPECT_NE(d2.data(), d1.data()); // ✅ 指针不同，确认是不同对象
        EXPECT_EQ(d1["key1"], 123);      // d1 不受影响

        // 再次验证自引用的完整性
        d2["self"].as<dict>()["key2"] = "new_value";
        EXPECT_EQ(d2["key2"], "new_value"); // 通过 self 引用修改应该反映到 d2
    }

    // 测试相互引用 - 应该保持引用关系
    {
        dict d1({{"key1", 123}});
        dict d2({{"key2", 456}});

        // 创建相互引用
        d1["ref"] = d2;
        d2["ref"] = d1;

        // 深拷贝 d1
        dict d1_copy = d1.deep_copy();

        // 验证拷贝成功
        EXPECT_EQ(d1_copy["key1"], 123);
        EXPECT_TRUE(d1_copy.contains("ref"));

        // 获取 d1_copy 引用的对象
        dict d2_copy = d1_copy["ref"].as<dict>();
        EXPECT_EQ(d2_copy["key2"], 456);

        // 验证 d2_copy 也引用回 d1_copy（保持循环引用）
        dict d1_copy_back = d2_copy["ref"].as<dict>();
        EXPECT_EQ(d1_copy_back["key1"], 123);

        // 关键验证：使用 data() 指针直接验证循环引用的身份
        EXPECT_EQ(d1_copy_back.data(), d1_copy.data());              // ✅ d1_copy_back 就是 d1_copy
        EXPECT_EQ(d2_copy.data(), d1_copy["ref"].as<dict>().data()); // ✅ d2_copy 就是 d1_copy["ref"]

        // 验证拷贝与原始对象独立
        EXPECT_NE(d1_copy.data(), d1.data()); // ✅ d1_copy 和 d1 是不同对象
        EXPECT_NE(d2_copy.data(), d2.data()); // ✅ d2_copy 和 d2 是不同对象

        // 通过修改验证引用关系
        d1_copy["key1"] = 999;
        EXPECT_EQ(d2_copy["ref"].as<dict>()["key1"], 999); // 通过 d2_copy 的引用应该看到修改
        EXPECT_EQ(d1_copy_back["key1"], 999);              // d1_copy_back 也应该看到修改

        // 验证原始对象不受影响
        EXPECT_EQ(d1["key1"], 123); // 原始不变
        EXPECT_EQ(d2["key2"], 456); // 原始不变

        // 通过循环引用修改 d2_copy，应该能从 d1_copy 访问到
        d2_copy["key2"] = 789;
        EXPECT_EQ(d1_copy["ref"].as<dict>()["key2"], 789); // 循环引用保持
    }

    // 测试间接循环引用（环形）
    {
        dict d1({{"key1", 123}});
        dict d2({{"key2", 456}});
        dict d3({{"key3", 789}});

        // 创建环形引用: d1 -> d2 -> d3 -> d1
        d1["next"] = d2;
        d2["next"] = d3;
        d3["next"] = d1;

        // 深拷贝应该保持环形引用结构
        dict d1_copy = d1.deep_copy();

        // 验证环形结构
        dict d2_copy = d1_copy["next"].as<dict>();
        dict d3_copy = d2_copy["next"].as<dict>();
        dict d1_back = d3_copy["next"].as<dict>();

        EXPECT_EQ(d1_copy["key1"], 123);
        EXPECT_EQ(d2_copy["key2"], 456);
        EXPECT_EQ(d3_copy["key3"], 789);
        EXPECT_EQ(d1_back["key1"], 123); // 应该回到 d1_copy

        // 关键验证：使用 data() 指针直接验证环形引用的身份
        EXPECT_EQ(d1_back.data(), d1_copy.data());                    // ✅ d1_back 就是 d1_copy
        EXPECT_EQ(d2_copy.data(), d1_copy["next"].as<dict>().data()); // ✅ d2_copy 就是 d1_copy["next"]
        EXPECT_EQ(d3_copy.data(), d2_copy["next"].as<dict>().data()); // ✅ d3_copy 就是 d2_copy["next"]
        EXPECT_EQ(d1_copy.data(), d3_copy["next"].as<dict>().data()); // ✅ 环回到 d1_copy

        // 验证完整的环形结构（使用临时变量避免链式调用问题）
        dict temp1 = d2_copy["next"].as<dict>(); // d3_copy
        dict temp2 = temp1["next"].as<dict>();   // 应该回到 d1_copy
        dict temp3 = temp2["next"].as<dict>();   // 应该是 d2_copy
        EXPECT_EQ(temp1.data(), d3_copy.data()); // ✅ temp1 是 d3_copy
        EXPECT_EQ(temp2.data(), d1_copy.data()); // ✅ temp2 是 d1_copy
        EXPECT_EQ(temp3.data(), d2_copy.data()); // ✅ temp3 是 d2_copy

        // 验证拷贝与原始对象独立
        EXPECT_NE(d1_copy.data(), d1.data()); // ✅ 不同对象
        EXPECT_NE(d2_copy.data(), d2.data()); // ✅ 不同对象
        EXPECT_NE(d3_copy.data(), d3.data()); // ✅ 不同对象

        // 通过修改验证引用关系
        d1_copy["key1"] = 999;
        EXPECT_EQ(d1_back["key1"], 999);                    // d1_back 应该看到修改
        EXPECT_EQ(d3_copy["next"].as<dict>()["key1"], 999); // 通过环形引用应该看到修改
        EXPECT_EQ(temp2["key1"], 999);                      // 验证确实是 d1_copy

        // 验证原始对象不受影响
        EXPECT_EQ(d1["key1"], 123); // 原始不变

        // 通过环中任意节点修改，都应该能从其他节点访问到
        d2_copy["key2"] = 111;
        EXPECT_EQ(d1_copy["next"].as<dict>()["key2"], 111); // 从 d1_copy 访问 d2_copy
        d3_copy["key3"] = 222;
        EXPECT_EQ(d2_copy["next"].as<dict>()["key3"], 222); // 从 d2_copy 访问 d3_copy
    }

    // 测试复杂嵌套结构中的循环引用
    {
        dict inner({{"inner_key", 100}});
        dict outer({{"outer_key", inner}, {"value", 200}});

        // 在内层dict中创建对外层dict的引用，形成循环
        inner["back_ref"] = outer;

        // 深拷贝应该保持循环引用结构
        dict outer_copy = outer.deep_copy();

        // 验证拷贝成功
        EXPECT_EQ(outer_copy["value"], 200);
        dict inner_copy = outer_copy["outer_key"].as<dict>();
        EXPECT_EQ(inner_copy["inner_key"], 100);

        // 验证循环引用保持
        dict outer_back = inner_copy["back_ref"].as<dict>();
        EXPECT_EQ(outer_back["value"], 200);

        // 关键验证：使用 data() 指针直接验证循环引用的身份
        EXPECT_EQ(outer_back.data(), outer_copy.data()); // ✅ outer_back 就是 outer_copy
        EXPECT_EQ(inner_copy.data(),
                  outer_copy["outer_key"].as<dict>().data()); // ✅ inner_copy 就是 outer_copy["outer_key"]

        // 验证拷贝与原始对象独立
        EXPECT_NE(outer_copy.data(), outer.data()); // ✅ 不同对象
        EXPECT_NE(inner_copy.data(), inner.data()); // ✅ 不同对象

        // 通过修改验证引用关系
        outer_copy["value"] = 999;
        EXPECT_EQ(inner_copy["back_ref"].as<dict>()["value"], 999); // 通过循环引用应该看到修改
        EXPECT_EQ(outer_back["value"], 999);                        // outer_back 也应该看到修改

        // 验证原始对象不受影响
        EXPECT_EQ(outer["value"], 200);     // 原始不变
        EXPECT_EQ(inner["inner_key"], 100); // 原始不变

        // 通过内层修改，外层应该能访问到
        inner_copy["inner_key"] = 555;
        EXPECT_EQ(outer_copy["outer_key"].as<dict>()["inner_key"], 555); // 循环引用保持
    }
}

// 测试 dict 的 find 方法
TEST(DictOperationsTest, MutableDictFind)
{
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
TEST(DictOperationsTest, MutableDictInsert)
{
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
TEST(DictOperationsTest, MutableDictInsertInteraction)
{
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

// 测试 find_entry(nullptr) 异常 - 通过 find() 间接测试
// 注意：find_entry 是 protected 方法，无法直接调用
// 我们通过公共接口 find() 来间接测试 find_entry 的 nullptr 处理
TEST(DictOperationsTest, DictFindEntryWithNullPointerKey)
{
    dict d({{"key1", 123}});
    // find() 方法内部会调用 find_entry，如果 find_entry 对 nullptr 抛出异常，这里也会抛出
    EXPECT_THROW(d.find(nullptr), std::invalid_argument);
}

// 测试 operator[](nullptr) 异常
TEST(DictOperationsTest, DictOperatorBracketWithNullPointerKey)
{
    const dict d({{"key1", 123}});
    EXPECT_THROW(d[nullptr], std::invalid_argument);
}

// 测试 operator[](const variant&) 键不存在异常
TEST(DictOperationsTest, DictOperatorBracketWithVariantKeyNotFound)
{
    const dict d({{"key1", 123}});
    variant    non_existent_key("key2");
    EXPECT_THROW(d[non_existent_key], std::out_of_range);
}

// 测试 get(const std::string&, const variant&)
TEST(DictOperationsTest, DictGetWithStringKey)
{
    dict    d({{"key1", 123}, {"key2", "value"}});
    variant default_val(456);
    EXPECT_EQ(d.get(std::string("key1"), default_val).as<int>(), 123);
    EXPECT_EQ(d.get(std::string("key2"), default_val).as<std::string>(), "value");
    EXPECT_EQ(d.get(std::string("key3"), default_val).as<int>(), 456);
}

// 测试 get(nullptr, default_value) 异常
TEST(DictOperationsTest, DictGetWithNullPointerKey)
{
    dict    d({{"key1", 123}});
    variant default_val(456);
    EXPECT_THROW(d.get(nullptr, default_val), std::invalid_argument);
}

// 测试 get(const variant&, const variant&)
TEST(DictOperationsTest, DictGetWithVariantKey)
{
    dict    d({{"key1", 123}, {"key2", "value"}});
    variant key1("key1");
    variant key2("key2");
    variant key3("key3");
    variant default_val(456);

    EXPECT_EQ(d.get(key1, default_val).as<int>(), 123);
    EXPECT_EQ(d.get(key2, default_val).as<std::string>(), "value");
    EXPECT_EQ(d.get(key3, default_val).as<int>(), 456);
}

// 测试空字典的 at_index 异常
TEST(DictOperationsTest, DictAtIndexOutOfRangeWithEmptyDict)
{
    dict d;
    EXPECT_THROW(d.at_index(0), std::out_of_range);
    EXPECT_THROW(d.at_index(1), std::out_of_range);
}

// 测试 at(const std::string&)
TEST(DictOperationsTest, DictAtWithStringKey)
{
    const dict d({{"key1", 123}, {"key2", "value"}});
    EXPECT_EQ(d.at(std::string("key1")).as<int>(), 123);
    EXPECT_EQ(d.at(std::string("key2")).as<std::string>(), "value");
}

// 测试 at 键不存在异常
TEST(DictOperationsTest, DictAtWithNonExistentKey)
{
    const dict d({{"key1", 123}});
    EXPECT_THROW(d.at("key2"), std::out_of_range);
}

// 测试 at(nullptr) 异常
TEST(DictOperationsTest, DictAtWithNullPointerKey)
{
    const dict d({{"key1", 123}});
    EXPECT_THROW(d.at(nullptr), std::invalid_argument);
}

// 测试 at(const variant&)
TEST(DictOperationsTest, DictAtWithVariantKey)
{
    const dict d({{"key1", 123}, {"key2", "value"}});
    variant    key1("key1");
    variant    key2("key2");
    variant    key3("key3");

    EXPECT_EQ(d.at(key1).as<int>(), 123);
    EXPECT_EQ(d.at(key2).as<std::string>(), "value");
    EXPECT_THROW(d.at(key3), std::out_of_range);
}

// 测试 find_index(nullptr) 异常
TEST(DictOperationsTest, DictFindIndexWithNullPointerKey)
{
    dict d({{"key1", 123}});
    EXPECT_THROW(d.find_index(nullptr), std::invalid_argument);
}

// 测试 find_index(const variant&)
TEST(DictOperationsTest, DictFindIndexWithVariantKey)
{
    dict    d({{"key1", 123}, {"key2", "value"}});
    variant key1("key1");
    variant key2("key2");
    variant key3("key3");

    int idx1 = d.find_index(key1);
    int idx2 = d.find_index(key2);
    int idx3 = d.find_index(key3);

    EXPECT_GE(idx1, 0);
    EXPECT_GE(idx2, 0);
    EXPECT_EQ(idx3, -1);
}

// 测试 contains(nullptr) 异常
TEST(DictOperationsTest, DictContainsWithNullPointerKey)
{
    dict d({{"key1", 123}});
    EXPECT_THROW(d.contains(nullptr), std::invalid_argument);
}

// 测试 hash() 方法
TEST(DictOperationsTest, DictHash)
{
    dict d1({{"key1", 123}, {"key2", "value"}});
    dict d2({{"key1", 123}, {"key2", "value"}});
    dict d3({{"key1", 456}, {"key2", "value"}});
    dict empty_dict;

    size_t hash1      = d1.hash();
    size_t hash2      = d2.hash();
    size_t hash3      = d3.hash();
    size_t hash_empty = empty_dict.hash();

    EXPECT_NE(hash1, 0); // 非空字典的hash应该非零
    EXPECT_NE(hash2, 0);
    EXPECT_NE(hash3, 0);
    EXPECT_EQ(hash_empty, 0); // 空字典的hash应该为0
    EXPECT_EQ(hash1, hash2);  // 相同内容的字典应该有相同的hash
    EXPECT_NE(hash1, hash3);  // 不同内容的字典应该有不同的hash
}

// 测试非 const find_entry(nullptr) 异常 - 通过 find() 间接测试
// 注意：find_entry 是 protected 方法，无法直接调用
// 我们通过公共接口 find() 来间接测试 find_entry 的 nullptr 处理
TEST(DictOperationsTest, MutableDictFindEntryWithNullPointerKey)
{
    dict md;
    md["key1"] = 123;
    // find() 方法内部会调用 find_entry，如果 find_entry 对 nullptr 抛出异常，这里也会抛出
    EXPECT_THROW(md.find(nullptr), std::invalid_argument);
}

// 测试非 const operator[](nullptr) 异常
TEST(DictOperationsTest, MutableDictOperatorBracketWithNullPointerKey)
{
    dict md;
    md["key1"] = 123;
    EXPECT_THROW(md[nullptr] = 456, std::invalid_argument);
}

// 测试非 const operator[](null variant) 异常
TEST(DictOperationsTest, MutableDictOperatorBracketWithNullVariantKey)
{
    dict    md;
    variant null_key; // null variant
    EXPECT_THROW(md[null_key] = 123, std::invalid_argument);
}

// 测试 erase(const variant&)
TEST(DictOperationsTest, MutableDictEraseWithVariantKey)
{
    dict md;
    md["key1"] = 123;
    md["key2"] = "value";
    md["key3"] = true;

    variant key1("key1");
    variant key2("key2");
    variant key4("key4");

    EXPECT_TRUE(md.erase(key1));
    EXPECT_EQ(md.size(), 2);
    EXPECT_FALSE(md.contains("key1"));

    EXPECT_TRUE(md.erase(key2));
    EXPECT_EQ(md.size(), 1);
    EXPECT_FALSE(md.contains("key2"));

    EXPECT_FALSE(md.erase(key4)); // 不存在的键
    EXPECT_EQ(md.size(), 1);
}

// 测试非 const at(const std::string&)
TEST(DictOperationsTest, MutableDictAtWithStringKey)
{
    dict md;
    md["key1"] = 123;
    md["key2"] = "value";

    md.at(std::string("key1")) = 456;
    EXPECT_EQ(md["key1"].as<int>(), 456);

    md.at(std::string("key2")) = "new_value";
    EXPECT_EQ(md["key2"].as<std::string>(), "new_value");
}

// 测试非 const at(nullptr) 异常
TEST(DictOperationsTest, MutableDictAtWithNullPointerKey)
{
    dict md;
    md["key1"] = 123;
    EXPECT_THROW(md.at(nullptr) = 456, std::invalid_argument);
}

// 测试非 const at(const variant&)
TEST(DictOperationsTest, MutableDictAtWithVariantKey)
{
    dict md;
    md["key1"] = 123;
    md["key2"] = "value";

    variant key1("key1");
    variant key2("key2");
    variant key3("key3");

    md.at(key1) = 456;
    EXPECT_EQ(md["key1"].as<int>(), 456);

    md.at(key2) = "new_value";
    EXPECT_EQ(md["key2"].as<std::string>(), "new_value");

    EXPECT_THROW(md.at(key3) = 789, std::out_of_range);
}

// 测试 insert 带 hint 参数
TEST(DictOperationsTest, MutableDictInsertWithHint)
{
    dict md;
    md["key1"] = 100;
    md["key2"] = 200;
    md["key3"] = 300;

    // 获取一个有效的 hint 迭代器
    auto hint = md.find("key2");
    ASSERT_NE(hint, md.end());

    // 使用 hint 插入新元素
    auto result = md.insert(hint, variant("key4"), variant(400));
    EXPECT_NE(result, md.end());
    EXPECT_EQ(md["key4"].as<int>(), 400);
    EXPECT_EQ(md.size(), 4);

    // 验证新元素确实被插入
    EXPECT_TRUE(md.contains("key4"));
}
