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

#include <mc/db/table.h>

namespace {

namespace mdb = mc::db;

// 定义标签类型
struct by_age : mdb::tag_base<by_age> {};
MC_FIELD_INDEX_TAG(by_name_age, "name_age");

class user : public mdb::object_base {
public:
    MC_REFLECTABLE("mc.test.db_table.TestUser")

    user() = default;

    user(std::string name, int age, double score = 0.0)
        : m_name(std::move(name)), m_age(age), m_score(score) {
    }

    ~user() override {
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
} // namespace

MC_REFLECT(user, ((m_name, "name"))((m_age, "age"))((m_score, "score")))

namespace {
using user_table = mdb::table<
    user, mdb::indexed_by<mdb::ordered_unique<&user::m_name>,
                          mdb::ordered_non_unique<&user::get_age, by_age::tag>,
                          mdb::ordered_non_unique<&user::m_name, &user::m_age, by_name_age::tag>>>;

// 拿到表的字段，可用于后续构造查询语句
auto field_name  = mc::db::field(&user::m_name);
auto field_age   = mc::db::field(&user::m_age);
auto field_score = mc::db::field(&user::m_score);

class table_test : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};
} // namespace

// 测试表格创建和基本操作
TEST_F(table_test, create_table) {
    // 创建表
    user_table users;

    // 添加用户
    user u1("张三", 25, 88.5);
    user u2("李四", 30, 92.0);
    user u3("王五", 25, 76.5);

    EXPECT_TRUE(users.add(u1));
    EXPECT_TRUE(users.add(u2));
    EXPECT_TRUE(users.add(u3));

    // 通过名称索引查找（成员变量）
    auto it = users.template get<1>().find("张三");
    ASSERT_FALSE(it.is_end());
    EXPECT_EQ((*it).name(), "张三");
    EXPECT_EQ((*it).get_age(), 25);

    // 通过年龄索引查找（成员函数）
    auto                     age_range = users.template get<2>().equal_range(25);
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
    auto composite_it = users.template get<3>().find("李四", 30);
    ASSERT_FALSE(composite_it.is_end());
    EXPECT_EQ((*composite_it).name(), "李四");
    EXPECT_EQ((*composite_it).get_age(), 30);
}

// 测试通过标签访问索引
TEST_F(table_test, tag_index_access) {
    // 创建表
    user_table users;

    // 添加用户
    user u1("张三", 25, 88.5);
    user u2("李四", 30, 92.0);
    user u3("王五", 25, 76.5);
    user u4("赵六", 35, 95.0);

    EXPECT_TRUE(users.add(u1));
    EXPECT_TRUE(users.add(u2));
    EXPECT_TRUE(users.add(u3));
    EXPECT_TRUE(users.add(u4));

    // 通过名称标签查找
    {
        auto& name_idx = users.template get<1>();
        auto  it       = name_idx.find("张三");
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->name(), "张三");
        EXPECT_EQ(it->get_age(), 25);
    }

    // 通过年龄标签查找
    {
        auto& age_idx     = users.template get<by_age>();
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
        auto& name_age_idx = users.template get<by_name_age>();
        auto  it           = name_age_idx.find("李四", 30);
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->name(), "李四");
        EXPECT_EQ(it->get_age(), 30);
    }
}

// 测试统一的get接口
TEST_F(table_test, unified_get_interface) {
    user_table users;

    // 添加用户
    {
        user u1("张三", 25, 88.5);
        user u2("李四", 30, 92.0);
        EXPECT_TRUE(users.add(u1));
        EXPECT_TRUE(users.add(u2));
    }

    // 通过数字索引访问
    {
        auto& name_idx = users.template get<1>();
        auto  it       = name_idx.find("张三");
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->name(), "张三");
    }

    // 通过标签访问
    {
        auto& age_idx = users.template get<by_age>();
        auto  it      = age_idx.find(25);
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->get_age(), 25);
    }

    // 验证数字索引和标签索引指向相同的索引
    {
        auto& idx1 = users.template get<2>();      // 通过数字
        auto& idx2 = users.template get<by_age>(); // 通过标签

        // 应该能找到相同的对象
        auto it1 = idx1.find(25);
        auto it2 = idx2.find(25);

        ASSERT_FALSE(it1.is_end());
        ASSERT_FALSE(it2.is_end());
        EXPECT_EQ(&(*it1), &(*it2));
    }
}

// 高级查询测试，使用DSL构建查询语句
TEST_F(table_test, advanced_query) {
    user_table users;

    // 添加一些测试数据
    user u1("张三", 25, 85.5);
    user u2("李四", 30, 90.0);
    user u3("王五", 25, 76.5);
    user u4("赵六", 35, 95.0);
    user u5("钱七", 40, 88.0);
    users.add(u1);
    users.add(u2);
    users.add(u3);
    users.add(u4);
    users.add(u5);

    // 简单条件查询
    {
        auto results = users.query(field_name == "张三");
        EXPECT_EQ(results.size(), 1);
        if (!results.empty()) {
            EXPECT_EQ(results[0]->name(), "张三");
            EXPECT_EQ(results[0]->get_age(), 25);
            EXPECT_DOUBLE_EQ(results[0]->score(), 85.5);
        }
    }

    // 复合条件查询 (AND)
    {
        auto results = users.query(field_age == 25 && field_score > 80.0);
        EXPECT_EQ(results.size(), 1);
        if (!results.empty()) {
            EXPECT_EQ(results[0]->name(), "张三");
            EXPECT_DOUBLE_EQ(results[0]->score(), 85.5);
        }
    }

    // 复合条件查询 (OR)
    {
        auto results = users.query(field_name == "张三" || field_name == "李四");
        EXPECT_EQ(results.size(), 2);
        // 排序结果以便于测试
        std::sort(results.begin(), results.end(), [](auto& a, auto& b) {
            return a->name() < b->name();
        });
        if (results.size() == 2) {
            EXPECT_EQ(results[0]->name(), "张三");
            EXPECT_EQ(results[1]->name(), "李四");
        }
    }

    // 范围查询
    {
        auto results = users.query(field_age >= 30 && field_age <= 35);
        EXPECT_EQ(results.size(), 2);
        // 排序结果以便于测试
        std::sort(results.begin(), results.end(), [](auto& a, auto& b) {
            return a->name() < b->name();
        });
        if (results.size() == 2) {
            EXPECT_EQ(results[0]->name(), "李四");
            EXPECT_EQ(results[1]->name(), "赵六");
        }
    }

    // 限制返回数量的查询
    {
        auto results = users.query(field_age >= 25, 2);
        EXPECT_EQ(results.size(), 2); // 只返回2条记录，尽管有5条匹配
    }

    // 使用自定义处理函数的查询
    {
        std::vector<std::string> names;
        users.query(field_score >= 85.0, [&names](const user& u) {
            names.push_back(u.name());
            return true; // 继续处理下一条记录
        });

        EXPECT_EQ(names.size(), 4); // 张三、李四、赵六、钱七的分数都 >= 85.0

        // 排序名称以便于测试
        std::sort(names.begin(), names.end());
        if (names.size() == 4) {
            EXPECT_EQ(names[0], "张三");
            EXPECT_EQ(names[1], "李四");
            EXPECT_EQ(names[2], "赵六");
            EXPECT_EQ(names[3], "钱七");
        }
    }

    // 查询单个记录
    {
        auto result = users.find(field_name == "王五");
        EXPECT_TRUE(result != nullptr);
        if (result != nullptr) {
            EXPECT_EQ(result->name(), "王五");
            EXPECT_EQ(result->get_age(), 25);
            EXPECT_DOUBLE_EQ(result->score(), 76.5);
        }
    }

    // 复合条件查询（AND和OR组合）
    {
        auto results = users.query((field_age < 30 && field_score > 80.0) ||
                                   (field_age > 35 && field_score < 90.0));
        EXPECT_EQ(results.size(), 2); // 应该匹配张三和钱七

        // 排序结果以便于测试
        std::sort(results.begin(), results.end(), [](auto& a, auto& b) {
            return a->name() < b->name();
        });

        if (results.size() == 2) {
            EXPECT_EQ(results[0]->name(), "张三"); // 满足第一个条件
            EXPECT_EQ(results[1]->name(), "钱七"); // 满足第二个条件
        }
    }
}

// 高级更新测试，使用DSL构建更新语句
TEST_F(table_test, advanced_update) {
    user_table users;

    // 添加一些测试数据
    user u1("Alice", 25, 85.5);
    user u2("Bob", 30, 90.0);
    user u3("Charlie", 35, 95.5);
    users.add(u1);
    users.add(u2);
    users.add(u3);

    // 使用 dict 更新
    {
        mc::dict update_values = {{"age", 26}, {"score", 88.0}};
        size_t   updated       = users.update(field_name == "Alice", update_values);
        EXPECT_EQ(updated, 1);

        auto result = users.find(field_name == "Alice");
        EXPECT_TRUE(result != nullptr);
        EXPECT_EQ(result->get_age(), 26); // Alice 的年龄被更新为 26
        EXPECT_DOUBLE_EQ(result->score(), 88.0);
    }

    // 使用 map 更新
    {
        std::map<std::string, mc::variant> update_values = {{"score", 88.0}};
        size_t                             updated       = users.update(field_name == "Bob", update_values);
        EXPECT_EQ(updated, 1);

        auto result = users.find(field_name == "Bob");
        EXPECT_TRUE(result != nullptr);
        EXPECT_DOUBLE_EQ(result->score(), 88.0);
    }

    // 批量更新测试
    {
        mc::dict update_values = {{"age", 26}};
        auto     expr          = field_age >= 30;

        auto   uu      = users.query(expr);
        size_t updated = users.update(expr, update_values);
        EXPECT_EQ(updated, 2); // Bob 和 Charlie 的年龄都 >= 30

        auto results = users.query(field_age >= 26);
        EXPECT_EQ(results.size(), 3);
    }
}

// 高级删除测试，使用DSL构建删除语句
TEST_F(table_test, advanced_remove) {
    user_table users;

    // 添加一些测试数据
    user u1("Alice", 25, 85.5);
    user u2("Bob", 30, 90.0);
    user u3("Charlie", 35, 95.5);
    user u4("David", 40, 100.0);
    users.add(u1);
    users.add(u2);
    users.add(u3);
    users.add(u4);

    // 测试删除单个记录
    {
        size_t removed = users.remove(field_name == "Alice");
        EXPECT_EQ(removed, 1);

        auto result = users.find(field_name == "Alice");
        EXPECT_FALSE(result != nullptr);
    }

    // 测试删除多个记录
    {
        size_t removed = users.remove(field_age >= 30);
        EXPECT_EQ(removed, 3); // Bob、Charlie 和 David 的年龄都 >= 30

        auto results = users.query(field_age >= 30);
        EXPECT_EQ(results.size(), 0);
    }

    // 前两个测试删除后，表应该为空
    EXPECT_TRUE(users.empty());

    // 测试复合条件删除
    {
        // 重新添加一些数据
        user u5("Eve", 25, 80.0);
        user u6("Frank", 30, 85.0);
        user u7("Grace", 35, 90.0);
        users.add(u5);
        users.add(u6);
        users.add(u7);

        size_t removed = users.remove((field_age >= 30) && (field_score < 90.0));
        EXPECT_EQ(removed, 1); // 只有 Frank 满足条件

        auto results = users.query((field_age >= 30) && (field_score < 90.0));
        EXPECT_EQ(results.size(), 0);

        EXPECT_EQ(users.size(), 2);
        auto all = users.all();
        EXPECT_EQ(all.size(), 2); // Eve 和 Grace 还在

        users.clear();
        EXPECT_TRUE(users.empty());
    }

    // 测试事务中的删除
    {
        // 重新添加一些数据
        user u8("Henry", 25, 75.0);
        user u9("Ivy", 30, 80.0);
        users.add(u8);
        users.add(u9);

        auto& txn = mc::db::transaction::get_instance();

        size_t removed = users.remove(field_score < 85.0, &txn);
        EXPECT_EQ(removed, 2); // Henry 和 Ivy 的分数都 < 85.0
        EXPECT_EQ(users.size(), 0);

        // 提交事务
        txn.commit();

        // 事务提交后，记录应该被删除
        auto results_after = users.query(field_score < 85.0);
        EXPECT_EQ(results_after.size(), 0);
        EXPECT_TRUE(users.empty());
    }

    // 测试事务中的删除后回滚
    {
        // 重新添加一些数据
        user u8("Henry", 25, 75.0);
        user u9("Ivy", 30, 80.0);
        users.add(u8);
        users.add(u9);

        auto& txn = mc::db::transaction::get_instance();

        size_t removed = users.remove(field_score < 85.0, &txn);
        EXPECT_EQ(removed, 2); // Henry 和 Ivy 的分数都 < 85.0
        EXPECT_EQ(users.size(), 0);

        // 回滚事务
        txn.rollback();

        // 事务回滚后，记录应该还在
        auto results_after = users.query(field_score < 85.0);
        EXPECT_EQ(results_after.size(), 2);

        users.clear();
        EXPECT_TRUE(users.empty());
    }
}

TEST_F(table_test, index_name) {
    user_table users;

    // 使用反射名命名索引
    EXPECT_EQ(users.get<1>().index_name(), "name");

    // 非反射信息的索引，使用索引编号命名
    EXPECT_EQ(users.get<2>().index_name(), "index_2");

    // MC_FIELD_INDEX_TAG 自定义的命名索引，使用自定义的名字
    EXPECT_EQ(users.get<3>().index_name(), "name_age");
}

TEST_F(table_test, index_name_composite) {
    using user_table_1 = mdb::table<
        user, mdb::indexed_by<
                  mdb::ordered_unique<&user::m_name, &user::get_age>>>;
    user_table_1 users;

    // 0: user::m_name 有反射信息
    // 1: user::get_age 没有反射信息，使用 key_type 加 key 编号命名
    EXPECT_EQ(users.get<1>().index_name(), "name,int_1");

    user u1("张三", 25, 88.5);
    auto key_values = users.get<1>().get_key_values(u1);
    EXPECT_EQ(key_values.size(), 2);
    EXPECT_EQ(key_values.to_string(), "[\"张三\",25]");

    users.add(u1);
    EXPECT_THROW(users.add(u1), mc::invalid_arg_exception);
}
