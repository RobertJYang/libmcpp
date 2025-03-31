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

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <mc/database/table.h>

namespace {

namespace mdb = mc::database;

// 定义标签类型
struct by_age : mdb::tag_base {};
struct by_name_age : mdb::tag_base {};

class user : public mdb::object_base<user> {
public:
    user() = default;

    user(uint32_t id, std::string name, int age, double score = 0.0)
        : object_base(id), m_name(std::move(name)), m_age(age), m_score(score) {
    }

    const std::string& name() const {
        return m_name;
    }

    int get_age() const {
        return m_age;
    }

    double score() const {
        return m_score;
    }

    std::string m_name;
    int         m_age;
    double      m_score;
};

// 使用限定命名空间访问
using user_table = mdb::table<
    user, mdb::indexed_by<mdb::ordered_unique<mdb::member<user, std::string, &user::m_name>>,
                          mdb::ordered_non_unique<mdb::member<user, int, &user::get_age>, by_age>,
                          mdb::ordered_non_unique<
                              mdb::composite_key<mdb::member<user, std::string, &user::m_name>,
                                                 mdb::member<user, int, &user::m_age>>,
                              by_name_age>>>;

} // namespace

// 表类测试
class table_test : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

// 测试表格创建和基本操作
TEST_F(table_test, create_table) {
    // 创建表
    user_table users;

    // 添加用户
    user u1(1, "张三", 25, 88.5);
    user u2(2, "李四", 30, 92.0);
    user u3(3, "王五", 25, 76.5);

    EXPECT_TRUE(users.add(u1));
    EXPECT_TRUE(users.add(u2));
    EXPECT_TRUE(users.add(u3));

    // 通过名称索引查找（成员变量）
    auto it = users.get<0>().find("张三");
    ASSERT_FALSE(it.is_end());
    EXPECT_EQ((*it).name(), "张三");
    EXPECT_EQ((*it).get_age(), 25);

    // 通过年龄索引查找（成员函数）
    auto                     age_range = users.get<1>().equal_range(25);
    std::vector<std::string> names;
    for (auto it = age_range.first; it != age_range.second; ++it) {
        names.push_back((*it).name());
    }
    EXPECT_EQ(names.size(), 2);
    if (names.size() == 2) {
        std::sort(names.begin(), names.end());
        EXPECT_EQ(names[0], "张三");
        EXPECT_EQ(names[1], "王五");
    }

    // 通过复合索引查找（同时使用成员变量）
    auto composite_it = users.get<2>().find("李四", 30);
    ASSERT_FALSE(composite_it.is_end());
    EXPECT_EQ((*composite_it).name(), "李四");
    EXPECT_EQ((*composite_it).get_age(), 30);
}

// 测试通过标签访问索引
TEST_F(table_test, tag_index_access) {
    // 创建表
    user_table users;

    // 添加用户
    user u1(1, "张三", 25, 88.5);
    user u2(2, "李四", 30, 92.0);
    user u3(3, "王五", 25, 76.5);
    user u4(4, "赵六", 35, 95.0);

    EXPECT_TRUE(users.add(u1));
    EXPECT_TRUE(users.add(u2));
    EXPECT_TRUE(users.add(u3));
    EXPECT_TRUE(users.add(u4));

    // 通过名称标签查找
    {
        auto& name_idx = users.get<0>();
        auto  it       = name_idx.find("张三");
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->name(), "张三");
        EXPECT_EQ(it->get_age(), 25);
    }

    // 通过年龄标签查找
    {
        auto& age_idx     = users.get<by_age>();
        auto [begin, end] = age_idx.equal_range(25);

        // 应该找到两个年龄为25的用户
        int count = 0;
        for (auto it = begin; it != end; ++it) {
            EXPECT_EQ(it->get_age(), 25);
            count++;
        }
        EXPECT_EQ(count, 2);
    }

    // 通过复合键标签查找
    {
        auto& name_age_idx = users.get<by_name_age>();
        auto  it           = name_age_idx.find("李四", 30);
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->name(), "李四");
        EXPECT_EQ(it->get_age(), 30);
    }
}

// 测试统一的get接口
TEST_F(table_test, unified_get_interface) {
    // 创建表
    user_table users;

    // 添加用户
    user u1(1, "张三", 25, 88.5);
    user u2(2, "李四", 30, 92.0);
    EXPECT_TRUE(users.add(u1));
    EXPECT_TRUE(users.add(u2));

    // 通过数字索引访问
    {
        auto& name_idx = users.get<0>();
        auto  it       = name_idx.find("张三");
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->name(), "张三");
    }

    // 通过标签访问
    {
        auto& age_idx = users.get<by_age>();
        auto  it      = age_idx.find(25);
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->get_age(), 25);
    }

    // 验证数字索引和标签索引指向相同的索引
    {
        auto& idx1 = users.get<1>();      // 通过数字
        auto& idx2 = users.get<by_age>(); // 通过标签

        // 应该能找到相同的对象
        auto it1 = idx1.find(25);
        auto it2 = idx2.find(25);

        ASSERT_FALSE(it1.is_end());
        ASSERT_FALSE(it2.is_end());
        EXPECT_EQ(&(*it1), &(*it2));
    }
}
