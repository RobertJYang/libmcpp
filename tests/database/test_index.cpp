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

/**
 * @file test_index.cpp
 * @brief 数据库索引单元测试
 */

#include <algorithm>
#include <functional>
#include <gtest/gtest.h>
#include <mc/database/index.h>
#include <mc/database/key.h>
#include <mc/database/key_extractor.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct user {
    user() = default;
    user(int id, std::string name, int age) : m_id(id), m_name(name), m_age(age) {
    }

    int         m_id;
    std::string m_name;
    int         m_age;

    const std::string& name() const {
        return m_name;
    }

    int age() const {
        return m_age;
    }

    uint32_t get_id() const {
        return static_cast<uint32_t>(m_id);
    }
};

// 测试 mc::database::index 的基本功能
TEST(database_index_test, mc_database_index_basic) {
    // 创建测试用户数据
    user u1(1, "张三", 20);

    user u2(2, "李四", 25);
    user u3(3, "王五", 22);

    auto name_extractor = mc::database::make_key<user, const std::string&, &user::name>();
    auto index = mc::database::make_index<user>("name", 1, name_extractor);

    // 添加用户到索引
    EXPECT_TRUE(index->add(u1));
    EXPECT_TRUE(index->add(u2));
    EXPECT_TRUE(index->add(u3));

    // 按名称查找用户
    auto found_u1 = index->find("张三");
    ASSERT_NE(found_u1, index->end());
    EXPECT_EQ(found_u1->m_id, 1);

    auto found_u2 = index->find("李四");
    ASSERT_NE(found_u2, index->end());
    EXPECT_EQ(found_u2->m_id, 2);

    // 查找不存在的用户
    auto not_found = index->find("赵六");
    EXPECT_EQ(not_found, index->end());

    // 删除用户
    EXPECT_TRUE(index->remove("张三").has_value());

    // 验证删除后无法找到
    found_u1 = index->find("张三");
    EXPECT_EQ(found_u1, index->end());
}

// 测试 mc::database::index 降序索引功能
TEST(database_index_test, mc_database_index_descending) {
    // 创建测试用户数据
    user u1(1, "张三", 20);
    user u2(2, "李四", 25);
    user u3(3, "王五", 22);

    // 使用mc::database的成员函数键提取器
    auto age_extractor = mc::database::make_key<user, int, &user::age>();

    // 创建降序索引 - 按年龄索引，降序排序
    auto index = mc::database::make_index<user, decltype(age_extractor), false>("age", 1, age_extractor);

    // 添加用户到索引
    EXPECT_TRUE(index->add(u1));
    EXPECT_TRUE(index->add(u2));
    EXPECT_TRUE(index->add(u3));

    std::vector<int> all_ids;
    for (auto &u: *index) {
        all_ids.push_back(u.m_age);
    }
    EXPECT_EQ(all_ids, (std::vector<int>{25, 22, 20}));

    // 按年龄查找用户
    auto found_u2 = index->find(25);
    ASSERT_NE(found_u2, index->end());
    EXPECT_EQ(found_u2->m_id, 2); // 应该找到李四（25岁）

    auto found_u3 = index->find(22);
    ASSERT_NE(found_u3, index->end());
    EXPECT_EQ(found_u3->m_id, 3); // 应该找到王五（22岁）

    auto found_u1 = index->find(20);
    ASSERT_NE(found_u1, index->end());
    EXPECT_EQ(found_u1->m_id, 1); // 应该找到张三（20岁）
}

// 测试 mc::database 函数对象键提取器
TEST(database_index_test, mc_database_index_functor_key) {
    // 创建测试用户数据
    user u1(1, "张三", 20);
    user u2(2, "李四", 25);
    user u3(3, "王五", 22);

    // 使用函数对象创建键提取器
    auto extractor = mc::database::make_key<user>([](const user& u) -> int {
        return u.age();
    });

    // 创建索引
    auto index = mc::database::make_index<user>("age", 1, extractor);

    // 添加用户并验证
    EXPECT_TRUE(index->add(u1));
    EXPECT_TRUE(index->add(u2));
    EXPECT_TRUE(index->add(u3));

    // 按年龄查找
    auto found = index->find(25);
    ASSERT_NE(found, index->end());
    EXPECT_EQ(found->m_id, 2);
}

// 测试 mc::database 复合操作
TEST(database_index_test, mc_database_index_operations) {
    // 创建测试用户数据
    user u1(1, "张三", 20);
    user u2(2, "李四", 25);
    user u3(3, "王五", 22);

    // 使用mc::database的成员函数键提取器
    auto name_extractor = mc::database::make_key<user, const std::string&, &user::name>();

    // 创建索引
    auto index = mc::database::make_index<user>("name", 1, name_extractor, true);

    // 添加用户到索引
    EXPECT_TRUE(index->add(u1));
    EXPECT_TRUE(index->add(u2));
    EXPECT_TRUE(index->add(u3));

    // 查找用户
    auto found_u1 = index->find("张三");
    ASSERT_NE(found_u1, index->end());
    EXPECT_EQ(found_u1->m_id, 1);

    // 删除用户并验证
    EXPECT_TRUE(index->remove("张三").has_value());

    found_u1 = index->find("张三");
    EXPECT_EQ(found_u1, index->end());

    // 重新插入，验证可以再次添加
    user u1_new(4, "赵六", 21); // 新用户，名字不同
    EXPECT_TRUE(index->add(u1_new));

    // 查找新插入的用户
    auto found_u4 = index->find("赵六");
    ASSERT_NE(found_u4, index->end());
    EXPECT_EQ(found_u4->m_id, 4);   // 应该是新用户
    EXPECT_EQ(found_u4->age(), 21); // 验证是新用户
}

// 测试组合键索引及前缀查询迭代
TEST(database_index_test, compound_key_test) {
    // 创建测试用户数据
    user u1(1, "张三", 20);
    user u2(2, "张三", 25);
    user u3(3, "李四", 20);
    user u4(4, "李四", 25);
    user u5(5, "王五", 30);

    // 创建组合索引：名字 + 年龄
    std::string index_name     = "name|age";
    auto        field_names    = std::vector<std::string>{"name", "age"};
    auto        name_extractor = mc::database::make_key<user, const std::string&, &user::name>();
    auto        age_extractor  = mc::database::make_key<user, int, &user::age>();
    auto        name_age_extractor = mc::database::make_key(name_extractor, age_extractor);
    auto        index = mc::database::make_index<user>(index_name, 1, name_age_extractor, true);

    // 添加用户到索引
    EXPECT_TRUE(index->add(u1));
    EXPECT_TRUE(index->add(u2));
    EXPECT_TRUE(index->add(u3));
    EXPECT_TRUE(index->add(u4));
    EXPECT_TRUE(index->add(u5));

    // 1. 直接查找特定用户
    {
        auto it = index->find(u1);
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it.get().m_id, 1);
    }

    // 2. 只使用组合索引的前半截查询: 张三
    {
        auto it = index->find("张三");
        ASSERT_FALSE(it.is_end());

        // 应该找到多个"张三"的用户
        std::vector<int> found_ids;
        for (; it->m_name == "张三"; ++it) {
            found_ids.push_back(it->m_id);
        }

        // 应该找到两个"张三"的用户 (ID: 1, 2)
        ASSERT_EQ(found_ids.size(), 2);
        std::sort(found_ids.begin(), found_ids.end());
        EXPECT_EQ(found_ids[0], 1);
        EXPECT_EQ(found_ids[1], 2);
    }

    // 3. 只使用组合索引的后半截查询: 名字，使用 equal_range
    {
        auto it = index->equal_range("张三");
        ASSERT_FALSE(it.first.is_end());
        ASSERT_TRUE(it.first != it.second);

        // 应该找到多个"张三"的用户
        std::vector<int> found_ids;
        for (; it.first != it.second; ++it.first) {
            found_ids.push_back(it.first->m_id);
        }

        // 应该找到两个"张三"的用户 (ID: 1, 2)
        ASSERT_EQ(found_ids.size(), 2);
        std::sort(found_ids.begin(), found_ids.end());
        EXPECT_EQ(found_ids[0], 1);
        EXPECT_EQ(found_ids[1], 2);
    }

    // 4. 使用find方法查询
    {
        auto it = index->find("李四");

        // 应该找到"李四"的用户
        std::vector<int> found_ids;
        for (; it->m_name == "李四"; ++it) {
            found_ids.push_back(it->m_id);
        }

        // 应该找到两个"李四"的用户 (ID: 3, 4)
        ASSERT_EQ(found_ids.size(), 2);
        std::sort(found_ids.begin(), found_ids.end());
        EXPECT_EQ(found_ids[0], 3);
        EXPECT_EQ(found_ids[1], 4);

        size_t i = 0;
        auto ret = index->equal_range("李四");
        for (auto it = ret.first; it != ret.second; ++it) {
            EXPECT_EQ(found_ids[i++], it->m_id);
        }
    }

    // 5. 使用全量组合key查询
    {
        auto it = index->find("李四", 25);

        // 应该找到"李四"的用户
        std::vector<int> found_ids;
        for (; it->m_name == "李四"; ++it) {
            found_ids.push_back(it->m_id);
        }

        // 应该找到两个"李四"的用户 (ID: 4)
        ASSERT_EQ(found_ids.size(), 1);
        std::sort(found_ids.begin(), found_ids.end());
        EXPECT_EQ(found_ids[0], 4);

        size_t i = 0;
        auto ret = index->equal_range("李四", 25);
        for (auto it = ret.first; it != ret.second; ++it) {
            EXPECT_EQ(found_ids[i++], it->m_id);
        }

        auto ret1 = index->equal_range("李四", 20);
        EXPECT_EQ(ret1.first->m_age, 20);
        EXPECT_EQ(ret1.second->m_age, 25); // 结束位置就是(李四, 25)
    }

    // 6. 使用begin/end迭代所有元素
    {
        std::vector<int> all_ids;
        for (auto &u: *index) {
            all_ids.push_back(u.m_id);
        }

        // 应该找到所有5个用户
        ASSERT_EQ(all_ids.size(), 5);
        std::sort(all_ids.begin(), all_ids.end());
        EXPECT_EQ(all_ids[0], 1);
        EXPECT_EQ(all_ids[1], 2);
        EXPECT_EQ(all_ids[2], 3);
        EXPECT_EQ(all_ids[3], 4);
        EXPECT_EQ(all_ids[4], 5);
    }
}
