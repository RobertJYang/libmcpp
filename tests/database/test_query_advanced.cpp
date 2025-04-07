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
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <mc/database/table.h>
#include <mc/dict.h>
#include <mc/reflect.h>
#include <mc/variant.h>

namespace {

namespace mdb = mc::database;

// 定义标签类型
struct by_age : mdb::tag_base {};
struct by_city : mdb::tag_base {};

class test_user : public mdb::object_base<test_user> {
public:
    using object_id_type = uint32_t;

    test_user() = default;

    test_user(uint32_t id, std::string name, int age, std::string city, double score = 0.0)
        : m_id(id), m_name(std::move(name)), m_age(age), m_city(std::move(city)), m_score(score) {
    }

    ~test_user() override {
    }

    // 获取用户ID
    uint32_t id() const {
        return m_id;
    }

    // 获取用户名
    const std::string& name() const {
        return m_name;
    }

    // 获取年龄
    int age() const {
        return m_age;
    }

    // 获取城市
    const std::string& city() const {
        return m_city;
    }

    // 获取分数
    double score() const {
        return m_score;
    }

    uint32_t get_id_add_age() const {
        return m_id + m_age;
    }

    // 成员变量
    uint32_t    m_id;
    std::string m_name;
    int         m_age;
    std::string m_city;
    double      m_score;
};

} // namespace

MC_REFLECT(test_user,
           ((m_id, "id"))((m_name, "name"))((m_age, "age"))((m_city, "city"))((m_score, "score")))

namespace {
MC_FIELD_INDEX_TAG(by_id_add_age, "id_age");

// 使用限定命名空间访问
using user_table = mdb::table<
    test_user,
    mdb::indexed_by<
        mdb::ordered_unique<mdb::member<test_user, uint32_t, &test_user::m_id>>,
        mdb::ordered_unique<mdb::member<test_user, std::string, &test_user::m_name>>,
        mdb::ordered_non_unique<mdb::member<test_user, int, &test_user::m_age>, by_age>,
        mdb::ordered_non_unique<mdb::member<test_user, std::string, &test_user::m_city>, by_city>,
        mdb::ordered_unique<mdb::member<test_user, uint32_t, &test_user::get_id_add_age>,
                            by_id_add_age>>>;

// 测试表查询功能
class table_query_test : public ::testing::Test {
protected:
    table_query_test() {
    }

    void SetUp() override {
        users.add(test_user(1, "张三", 25, "北京", 88.5));
        users.add(test_user(2, "李四", 30, "上海", 92.0));
        users.add(test_user(3, "王五", 25, "广州", 76.5));
        users.add(test_user(4, "赵六", 35, "深圳", 95.0));
        users.add(test_user(5, "钱七", 40, "北京", 82.5));
    }

    void TearDown() override {
        // 清理测试数据
    }

    // 获取匹配条件的用户ID列表
    std::vector<uint32_t> query_users(const mdb::query_builder& builder) {
        std::vector<uint32_t> result;

        users.query(builder, [&result](auto& obj) {
            result.push_back(obj.id());
            return true;
        });

        return result;
    }

    user_table users;
};

// 使用 Proto DSL 命名空间
using namespace mdb::query::dsl;

// 测试复合条件查询
TEST_F(table_query_test, complex_condition_query) {
    // 测试 AND 条件：age = 25 AND city = "北京"
    {
        auto age_field  = field("age");
        auto city_field = field("city");
        auto expr       = age_field == 25 && city_field == "北京";

        auto user_ids = query_users(expr);
        EXPECT_EQ(user_ids.size(), 1);
        EXPECT_EQ(user_ids[0], 1); // 张三 (25岁, 北京)
    }

    // 测试 OR 条件：(age == 25) || (city == "北京")
    {
        auto age_field  = field("age");
        auto city_field = field("city");
        auto expr       = (age_field == 25) || (city_field == "北京");

        auto user_ids = query_users(expr);
        EXPECT_EQ(user_ids.size(), 3); // 应该有3个用户匹配

        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1); // 张三 (25岁, 北京)
        EXPECT_EQ(user_ids[1], 3); // 王五 (25岁)
        EXPECT_EQ(user_ids[2], 5); // 钱七 (北京)
    }

    // 测试比较条件：score > 90
    {
        auto score_field = field("score");
        auto expr        = score_field > 90.0;

        auto user_ids = query_users(expr);
        EXPECT_EQ(user_ids.size(), 2);

        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 2); // 李四 (92分 > 90)
        EXPECT_EQ(user_ids[1], 4); // 赵六 (95分 > 90)
    }
}

// 测试特殊条件查询
TEST_F(table_query_test, special_condition_query) {
    // 测试 IN 条件：city IN ["北京", "上海"]
    {
        auto city_field = field("city");
        auto expr       = in(city_field, {"北京", "上海"});

        auto user_ids = query_users(expr);
        EXPECT_EQ(user_ids.size(), 3);

        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1); // 张三 (北京)
        EXPECT_EQ(user_ids[1], 2); // 李四 (上海)
        EXPECT_EQ(user_ids[2], 5); // 钱七 (北京)
    }

    // 测试 BETWEEN 条件：age BETWEEN 25 AND 35
    {
        auto age_field = field("age");
        auto expr      = between(age_field, 25, 35);

        auto user_ids = query_users(expr);
        EXPECT_EQ(user_ids.size(), 4);

        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1); // 张三 (25岁)
        EXPECT_EQ(user_ids[1], 2); // 李四 (30岁)
        EXPECT_EQ(user_ids[2], 3); // 王五 (25岁)
        EXPECT_EQ(user_ids[3], 4); // 赵六 (35岁)
    }

    // 测试 LIKE 操作
    {
        auto name_field = field("name");
        auto expr       = like(name_field, "%三%");

        auto user_ids = query_users(expr);
        EXPECT_EQ(user_ids.size(), 1);
        EXPECT_EQ(user_ids[0], 1); // 张三
    }

    // 测试 CONTAINS 操作
    {
        auto name_field = field("name");
        auto expr       = contains(name_field, "六");

        auto user_ids = query_users(expr);
        EXPECT_EQ(user_ids.size(), 1);
        EXPECT_EQ(user_ids[0], 4); // 赵六
    }
}

// 测试索引优化查询
TEST_F(table_query_test, index_optimized_query) {
    // 测试按主键ID查询（应该使用主键索引）
    {
        auto id_field = field("id");
        auto expr     = id_field == 3;

        auto users_result = users.query(expr);
        ASSERT_EQ(users_result.size(), 1);
        EXPECT_EQ(users_result[0].id(), 3);
        EXPECT_EQ(users_result[0].name(), "王五");
    }

    // 测试按姓名查询（应该使用姓名索引）
    {
        auto name_field = field("name");
        auto expr       = name_field == "李四";

        auto user_opt = users.find(expr);
        ASSERT_TRUE(user_opt.has_value());
        EXPECT_EQ(user_opt->id(), 2);
        EXPECT_EQ(user_opt->name(), "李四");
    }
}

// 测试查询限制和自定义处理
TEST_F(table_query_test, query_limit_and_custom_handler) {
    // 测试查询并限制返回数量
    {
        auto age_field = field("age");
        auto expr      = age_field == 25;

        auto results = users.query(expr, 2UL);
        ASSERT_EQ(results.size(), 2);

        // 检查查询结果是否包含所有25岁的用户
        bool found_zhangsan = false;
        bool found_wangwu   = false;

        for (const auto& user : results) {
            if (user.id() == 1) {
                found_zhangsan = true;
            } else if (user.id() == 3) {
                found_wangwu = true;
            }
        }

        EXPECT_TRUE(found_zhangsan);
        EXPECT_TRUE(found_wangwu);
    }

    // 测试自定义处理器
    {
        auto city_field = field("city");
        auto expr       = city_field == "北京";

        std::vector<std::string> names;
        users.query(expr, [&names](const test_user& user) {
            names.push_back(user.name());
            return true;
        });

        ASSERT_EQ(names.size(), 2);
        std::sort(names.begin(), names.end());
        EXPECT_EQ(names[0], "张三");
        EXPECT_EQ(names[1], "钱七");
    }

    // 测试复合条件查询
    {
        auto age_field  = field("age");
        auto city_field = field("city");
        auto expr       = (age_field >= 30) && (city_field == "北京");

        auto results = users.query(expr);
        EXPECT_EQ(results.size(), 1);

        if (!results.empty()) {
            EXPECT_EQ(results[0].id(), 5); // 钱七 (40岁, 北京)
        }
    }
}

} // namespace