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
#include <array>

#include <mc/db/query/query.h>
#include <mc/db/table.h>
#include <mc/db/transaction.h>
#include <mc/dict.h>
#include <mc/reflect.h>
#include <mc/variant.h>

namespace {

namespace mdb = mc::db;

struct int_range {
    std::vector<int> values;

    auto begin() const
    {
        return values.begin();
    }

    auto end() const
    {
        return values.end();
    }
};

// 定义标签类型
struct by_age : mdb::tag_base<by_age> {};
struct by_city : mdb::tag_base<by_city> {};

class test_user : public mdb::object_base {
public:
    MC_REFLECTABLE("mc.test.db_query.TestUser")

    test_user() = default;

    test_user(uint32_t id, mc::string name, int age, mc::string city, double score = 0.0)
        : m_id(id), m_name(std::move(name)), m_age(age), m_city(std::move(city)), m_score(score)
    {}

    ~test_user() override
    {}

    // 获取用户ID
    uint32_t id() const
    {
        return m_id;
    }

    // 获取用户名
    const mc::string& name() const
    {
        return m_name;
    }

    // 获取年龄
    int age() const
    {
        return m_age;
    }

    // 获取城市
    const mc::string& city() const
    {
        return m_city;
    }

    // 获取分数
    double score() const
    {
        return m_score;
    }

    uint32_t get_id_add_age() const
    {
        return m_id + m_age;
    }

    // 成员变量
    uint32_t   m_id;
    mc::string m_name;
    int        m_age;
    mc::string m_city;
    double     m_score;
};

} // namespace

MC_REFLECT(test_user,
           ((m_id, "id"))((m_name, "name"))((m_age, "age"))((m_city, "city"))((m_score, "score"))((get_id_add_age,
                                                                                                   "id_age")))

namespace {
MC_FIELD_INDEX_TAG(by_id_add_age, "id_age");

// 使用限定命名空间访问
using user_table =
    mdb::table<test_user,
               mdb::indexed_by<mdb::ordered_unique<&test_user::m_id>, mdb::ordered_unique<&test_user::m_name>,
                               mdb::ordered_non_unique<&test_user::m_age, by_age::tag>,
                               mdb::ordered_non_unique<&test_user::m_city, by_city::tag>,
                               mdb::ordered_unique<&test_user::get_id_add_age, by_id_add_age::tag>>>;

// 测试表查询功能
class table_query_test : public ::testing::Test {
protected:
    table_query_test()
    {}

    void SetUp() override
    {
        users.add(test_user(1, "张三", 25, "北京", 88.5));
        users.add(test_user(2, "李四", 30, "上海", 92.0));
        users.add(test_user(3, "王五", 25, "广州", 76.5));
        users.add(test_user(4, "赵六", 35, "深圳", 95.0));
        users.add(test_user(5, "钱七", 40, "北京", 82.5));
    }

    void TearDown() override
    {
        users.clear();
        mdb::transaction::reset_for_test();
    }

    std::vector<uint32_t> query_users(const mdb::query_builder& builder)
    {
        std::vector<uint32_t>        result;
        mdb::table_query<user_table> executor(users);

        executor.query(builder, [&result](auto& obj) {
            result.push_back(obj.id());
            return true;
        });

        return result;
    }

    user_table::object_ptr_type query_one_user(const mc::dict& spec)
    {
        mdb::table_query<user_table> executor(users);
        return executor.query_one(mdb::query_builder(spec));
    }

    user_table users;
};

TEST_F(table_query_test, condition_conversion)
{
    mc::variant spec = mc::dict{{"age", 25}, {"city", "北京"}};

    auto cond = mdb::to_condition(spec);
    EXPECT_TRUE(cond.is_logical());
    EXPECT_EQ(cond.get_logical_op(), mdb::logical_op::AND);
    ASSERT_EQ(cond.get_conditions().size(), 2);
    EXPECT_EQ(cond.get_conditions()[0].get_field(), "age");
    EXPECT_EQ(cond.get_conditions()[0].get_op(), mdb::compare_op::eq);
    EXPECT_EQ(cond.get_conditions()[1].get_field(), "city");
}

TEST_F(table_query_test, complex_condition_query)
{
    // 测试 AND 条件：age = 25 AND city = "北京"
    {
        auto user_ids = query_users(mdb::query_builder(mc::dict{
            {"age", 25},
            {"city", "北京"},
        }));
        ASSERT_EQ(user_ids.size(), 1);
        EXPECT_EQ(user_ids[0], 1);
    }

    // 测试 OR 条件：(age == 25) || (city == "北京")
    {
        auto user_ids = query_users(mdb::query_builder(mc::dict{
            {"$or",
             mc::variants{
                 mc::dict{{"age", 25}},
                 mc::dict{{"city", "北京"}},
             }},
        }));
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 3);
        EXPECT_EQ(user_ids[2], 5);
    }

    // 测试比较条件：score > 90
    {
        auto user_ids = query_users(mdb::query_builder(mc::dict{
            {"score", mc::dict{{"$gt", 90.0}}},
        }));
        ASSERT_EQ(user_ids.size(), 2);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 2);
        EXPECT_EQ(user_ids[1], 4);
    }
}

TEST_F(table_query_test, special_condition_query)
{
    // 测试 IN 条件：city IN ["北京", "上海"]
    {
        mdb::query_builder builder(mc::dict{{"city", mc::dict{{"$in", mc::variants{"北京", "上海"}}}}});
        auto               user_ids = query_users(builder);
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 2);
        EXPECT_EQ(user_ids[2], 5);
    }

    // 测试 BETWEEN 条件：age BETWEEN 25 AND 35
    {
        mdb::query_builder builder(mc::dict{{"age", mc::dict{{"$between", mc::variants{25, 35}}}}});
        auto               user_ids = query_users(builder);
        ASSERT_EQ(user_ids.size(), 4);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 2);
        EXPECT_EQ(user_ids[2], 3);
        EXPECT_EQ(user_ids[3], 4);
    }

    // 测试 LIKE 操作
    {
        mdb::query_builder builder(mc::dict{{"name", mc::dict{{"$like", "%三%"}}}});
        auto               user_ids = query_users(builder);
        ASSERT_EQ(user_ids.size(), 1);
        EXPECT_EQ(user_ids[0], 1);
    }

    // 测试 CONTAINS 操作
    {
        mdb::query_builder builder(mc::dict{{"name", mc::dict{{"$contains", "六"}}}});
        auto               user_ids = query_users(builder);
        ASSERT_EQ(user_ids.size(), 1);
        EXPECT_EQ(user_ids[0], 4);
    }
}

TEST_F(table_query_test, in_supports_common_containers_and_generic_ranges)
{
    {
        auto user_ids = query_users(mdb::field("city").in({"北京", "上海"}));
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 2);
        EXPECT_EQ(user_ids[2], 5);
    }

    {
        auto user_ids = query_users(mdb::field("age").in(std::array<int, 2>{25, 35}));
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 3);
        EXPECT_EQ(user_ids[2], 4);
    }

    {
        auto user_ids = query_users(mdb::field("age").in(mc::array<int>{25, 40}));
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 3);
        EXPECT_EQ(user_ids[2], 5);
    }

    {
        auto user_ids = query_users(mdb::field("age").in(int_range{{30, 40}}));
        ASSERT_EQ(user_ids.size(), 2);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 2);
        EXPECT_EQ(user_ids[1], 5);
    }

    {
        auto user_ids = query_users(mdb::conditions::in("city", {"北京", "深圳"}));
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 4);
        EXPECT_EQ(user_ids[2], 5);
    }
}

TEST_F(table_query_test, index_optimized_query)
{
    // 测试按主键ID查询（应该使用主键索引）
    {
        mdb::query_builder builder(mc::dict{{"id", 3}});

        mdb::table_query<user_table> executor(users);
        auto                         user_opt = executor.query_one(builder);
        ASSERT_TRUE(user_opt != nullptr);
        EXPECT_EQ(user_opt->id(), 3);
        EXPECT_EQ(user_opt->name(), "王五");
    }

    // 测试按姓名查询（应该使用姓名索引）
    {
        auto user_opt = query_one_user(mc::dict{{"name", "李四"}});
        ASSERT_TRUE(user_opt != nullptr);
        EXPECT_EQ(user_opt->id(), 2);
        EXPECT_EQ(user_opt->name(), "李四");
    }
}

TEST_F(table_query_test, query_limit_and_custom_handler)
{
    // 测试查询并限制返回数量
    {
        mdb::query_builder builder(mc::dict{{"age", 25}});
        builder.limit(1);

        mdb::table_query<user_table> executor(users);
        auto                         results = executor.query(builder, builder.get_limit());
        ASSERT_EQ(results.size(), 1);
        EXPECT_TRUE(results[0]->id() == 1 || results[0]->id() == 3);
    }

    // 测试自定义处理器
    {
        mdb::query_builder builder(mc::dict{
            {"city", "北京"},
        });

        std::vector<mc::string>      names;
        mdb::table_query<user_table> executor(users);
        auto                         completed = executor.query(builder, [&names](const test_user& user) {
            names.push_back(user.name());
            return false;
        });

        EXPECT_FALSE(completed);
        ASSERT_EQ(names.size(), 1);
    }

    // 测试复合条件查询
    {
        auto result = query_one_user(mc::dict{
            {"age", mc::dict{{"$gte", 30}}},
            {"city", "北京"},
        });
        ASSERT_TRUE(result != nullptr);
        EXPECT_EQ(result->id(), 5);
    }
}

TEST_F(table_query_test, direct_query_builder)
{
    // 使用 query_builder 的 where 方法构建查询
    {
        mdb::query_builder builder;
        builder.where("age", mdb::compare_op::eq, 25);

        auto user_ids = query_users(builder);
        ASSERT_EQ(user_ids.size(), 2);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 3);
    }

    // 使用 query_builder 的 where 和 or_where 方法组合条件
    {
        mdb::query_builder builder;
        builder.where("city", mdb::compare_op::eq, "北京");
        builder.or_where("city", mdb::compare_op::eq, "上海");

        auto user_ids = query_users(builder);
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 2);
        EXPECT_EQ(user_ids[2], 5);
    }

    // 使用 query_builder 的 where 构建范围条件
    {
        mdb::query_builder builder;
        builder.where("age", mdb::compare_op::le, 30);

        auto user_ids = query_users(builder);
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 2);
        EXPECT_EQ(user_ids[2], 3);
    }
}

TEST_F(table_query_test, query_builder_where_in_supports_common_containers_and_generic_ranges)
{
    {
        mdb::query_builder builder;
        builder.where_in("city", {"北京", "上海"});

        auto user_ids = query_users(builder);
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 2);
        EXPECT_EQ(user_ids[2], 5);
    }

    {
        mdb::query_builder builder;
        builder.where_in("age", std::array<int, 2>{25, 35});

        auto user_ids = query_users(builder);
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 3);
        EXPECT_EQ(user_ids[2], 4);
    }

    {
        mdb::query_builder builder;
        builder.where_in("age", mc::array<int>{25, 40});

        auto user_ids = query_users(builder);
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 3);
        EXPECT_EQ(user_ids[2], 5);
    }

    {
        mdb::query_builder builder;
        builder.where_in("age", int_range{{30, 40}});

        auto user_ids = query_users(builder);
        ASSERT_EQ(user_ids.size(), 2);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 2);
        EXPECT_EQ(user_ids[1], 5);
    }
}

TEST_F(table_query_test, expression_dsl_query)
{
    {
        auto user_ids = query_users((mdb::field("age") == 25) && (mdb::field("city") == "北京"));
        ASSERT_EQ(user_ids.size(), 1);
        EXPECT_EQ(user_ids[0], 1);
    }

    {
        auto user_ids = query_users((mdb::field("age") == 25) || (mdb::field("city") == "北京"));
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 3);
        EXPECT_EQ(user_ids[2], 5);
    }

    {
        auto user_ids = query_users(!(mdb::field("city") == "北京"));
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 2);
        EXPECT_EQ(user_ids[1], 3);
        EXPECT_EQ(user_ids[2], 4);
    }
}

TEST_F(table_query_test, find_accepts_expression_condition)
{
    auto user_opt = users.find((mdb::field("age") == 25) && (mdb::field("city") == "北京"));
    ASSERT_TRUE(user_opt != nullptr);
    EXPECT_EQ(user_opt->id(), 1);
    EXPECT_EQ(user_opt->name(), "张三");
}

TEST_F(table_query_test, find_accepts_reflected_method_condition)
{
    auto user_opt = users.find(mdb::field("id_age") == 32);
    ASSERT_TRUE(user_opt != nullptr);
    EXPECT_EQ(user_opt->id(), 2);
    EXPECT_EQ(user_opt->name(), "李四");
}

TEST_F(table_query_test, find_object_accepts_expression_condition)
{
    auto user_opt = users.find_object((mdb::field("age") == 25) && (mdb::field("city") == "北京"));
    ASSERT_TRUE(user_opt != nullptr);
    EXPECT_EQ(user_opt->id(), 1);
    EXPECT_EQ(user_opt->name(), "张三");
}

TEST_F(table_query_test, field_tag_exposes_legacy_field_expression)
{
    auto user_opt = users.find_object(by_id_add_age::field == 32);
    ASSERT_TRUE(user_opt != nullptr);
    EXPECT_EQ(user_opt->id(), 2);
    EXPECT_EQ(user_opt->name(), "李四");
}

TEST_F(table_query_test, string_field_method)
{
    // 使用字符串版本的字段名
    {
        auto user_ids = query_users(mdb::query_builder(mc::dict{
            {"age", 25},
        }));
        ASSERT_EQ(user_ids.size(), 2);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 3);
    }

    // 组合使用字符串字段构造 OR 查询
    {
        auto user_ids = query_users(mdb::query_builder(mc::dict{
            {"$or",
             mc::variants{
                 mc::dict{{"name", "张三"}},
                 mc::dict{{"city", "上海"}},
             }},
        }));
        ASSERT_EQ(user_ids.size(), 2);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 2);
    }

    // 使用字符串字段构建复杂查询
    {
        auto user_ids = query_users(mdb::query_builder(mc::dict{
            {"score", mc::dict{{"$gt", 80.0}}},
            {"city", "北京"},
        }));
        ASSERT_EQ(user_ids.size(), 2);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 1);
        EXPECT_EQ(user_ids[1], 5);
    }
}

TEST_F(table_query_test, logical_not_query)
{
    {
        mdb::query_builder builder(mc::dict{
            {"$not", mc::dict{{"city", "北京"}}},
        });
        auto               user_ids = query_users(builder);
        ASSERT_EQ(user_ids.size(), 3);
        std::sort(user_ids.begin(), user_ids.end());
        EXPECT_EQ(user_ids[0], 2);
        EXPECT_EQ(user_ids[1], 3);
        EXPECT_EQ(user_ids[2], 4);
    }
}

TEST_F(table_query_test, compound_field_predicates)
{
    mdb::query_builder builder(mc::dict{
        {"age", mc::dict{{"$gte", 30}, {"$lte", 35}}},
        {"city", mc::dict{{"$in", mc::variants{"上海", "深圳"}}}},
    });

    auto user_ids = query_users(builder);
    ASSERT_EQ(user_ids.size(), 2);
    std::sort(user_ids.begin(), user_ids.end());
    EXPECT_EQ(user_ids[0], 2);
    EXPECT_EQ(user_ids[1], 4);
}

TEST_F(table_query_test, query_helper_methods)
{
    mdb::table_query<user_table> executor(users);

    {
        auto results = executor.query_limit(mdb::query_builder(), 2);
        ASSERT_EQ(results.size(), 2);
    }

    {
        auto results = executor.query_all(mdb::query_builder(mc::dict{{"age", 25}}));
        ASSERT_EQ(results.size(), 2);
        std::vector<uint32_t> ids;
        for (const auto& user : results) {
            ids.push_back(user->id());
        }
        std::sort(ids.begin(), ids.end());
        EXPECT_EQ(ids[0], 1);
        EXPECT_EQ(ids[1], 3);
    }

    {
        auto results = executor.query_limit(mdb::query_builder(mc::dict{{"age", mc::dict{{"$gte", 25}}}}), 2);
        ASSERT_EQ(results.size(), 2);
    }

    {
        auto results = executor.query_limit(mdb::query_builder(mc::dict{{"name", mc::dict{{"$contains", ""}}}}), 2);
        ASSERT_EQ(results.size(), 2);
    }
}

TEST_F(table_query_test, planner_generates_index_exact_match_for_indexed_eq)
{
    const auto& metadata = mdb::table_query<user_table>::get_metadata();
    auto        plan =
        mc::db::query::query_planner<test_user>(metadata).plan_for_query(mdb::query_builder(mc::dict{{"age", 25}}));

    EXPECT_TRUE(plan.use_index);
    EXPECT_EQ(plan.plan_type, mdb::query_plan_type::index_exact_match);
    ASSERT_EQ(plan.fields.size(), 1);
    EXPECT_EQ(plan.fields[0], "age");
    EXPECT_EQ(plan.key_value, 25);
    EXPECT_TRUE(plan.values.empty());
}

TEST_F(table_query_test, planner_merges_same_field_or_into_in_plan)
{
    const auto& metadata = mdb::table_query<user_table>::get_metadata();
    auto        plan     = mc::db::query::query_planner<test_user>(metadata).plan_for_query(mdb::query_builder(mc::dict{
                   {"$or", mc::variants{mc::dict{{"city", "北京"}}, mc::dict{{"city", "上海"}}}},
    }));

    EXPECT_TRUE(plan.use_index);
    EXPECT_EQ(plan.plan_type, mdb::query_plan_type::index_exact_match);
    ASSERT_EQ(plan.fields.size(), 1);
    EXPECT_EQ(plan.fields[0], "city");
    ASSERT_EQ(plan.values.size(), 2);
    EXPECT_EQ(plan.values[0], "北京");
    EXPECT_EQ(plan.values[1], "上海");
}

} // namespace