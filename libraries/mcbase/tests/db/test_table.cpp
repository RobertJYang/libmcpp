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

#include <mc/db/query/query.h>
#include <mc/db/table.h>
#include <mc/db/transaction.h>

namespace {

namespace mdb = mc::db;

// 定义标签类型
struct by_age : mdb::tag_base<by_age> {};
MC_FIELD_INDEX_TAG(by_name_age, "name_age");

class user : public mdb::object_base {
public:
    MC_REFLECTABLE("mc.test.db_table.TestUser")

    user() = default;

    user(mc::string name, int age, double score = 0.0) : m_name(std::move(name)), m_age(age), m_score(score)
    {}

    ~user() override
    {}

    const mc::string& name() const
    {
        return m_name;
    }

    int get_age() const
    {
        return m_age;
    }

    double score() const
    {
        return m_score;
    }

    mc::string m_name;
    int        m_age;
    double     m_score;
};
} // namespace

MC_REFLECT(user, ((m_name, "name"))((m_age, "age"))((m_score, "score")))

namespace {
using user_table =
    mdb::table<user,
               mdb::indexed_by<mdb::ordered_unique<&user::m_name>, mdb::ordered_non_unique<&user::get_age, by_age::tag>,
                               mdb::ordered_non_unique<&user::m_name, &user::m_age, by_name_age::tag>>>;

class table_test : public ::testing::Test {
protected:
    void SetUp() override
    {}

    void TearDown() override
    {
        mdb::transaction::reset_for_test();
    }
};

std::vector<user_table::object_ptr_type> query_user_objects(user_table& users, const mc::dict& spec, size_t limit = 0)
{
    mdb::table_query<user_table> executor(users);
    return executor.query(mdb::query_builder(spec), limit);
}

user_table::object_ptr_type query_one_user(user_table& users, const mc::dict& spec)
{
    mdb::table_query<user_table> executor(users);
    return executor.query_one(mdb::query_builder(spec));
}
} // namespace

// 测试表格创建和基本操作
TEST_F(table_test, create_table)
{
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
    auto                    age_range = users.template get<2>().equal_range(25);
    std::vector<mc::string> names;
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
TEST_F(table_test, tag_index_access)
{
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
TEST_F(table_test, unified_get_interface)
{
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

// 高级查询测试，使用结构化查询语句
TEST_F(table_test, advanced_query)
{
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
        auto results = query_user_objects(users, mc::dict{{"name", "张三"}});
        EXPECT_EQ(results.size(), 1);
        if (!results.empty()) {
            EXPECT_EQ(results[0]->name(), "张三");
            EXPECT_EQ(results[0]->get_age(), 25);
            EXPECT_DOUBLE_EQ(results[0]->score(), 85.5);
        }
    }

    // 复合条件查询 (AND)
    {
        auto results = query_user_objects(users, mc::dict{{"age", 25}, {"score", mc::dict{{"$gt", 80.0}}}});
        EXPECT_EQ(results.size(), 1);
        if (!results.empty()) {
            EXPECT_EQ(results[0]->name(), "张三");
            EXPECT_DOUBLE_EQ(results[0]->score(), 85.5);
        }
    }

    // 复合条件查询 (OR)
    {
        auto results = query_user_objects(users, mc::dict{
                                                     {"$or",
                                                      mc::variants{
                                                          mc::dict{{"name", "张三"}},
                                                          mc::dict{{"name", "李四"}},
                                                      }},
                                                 });
        EXPECT_EQ(results.size(), 2);
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
        auto results = query_user_objects(users, mc::dict{{"age", mc::dict{{"$gte", 30}, {"$lte", 35}}}});
        EXPECT_EQ(results.size(), 2);
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
        auto results = query_user_objects(users, mc::dict{{"age", mc::dict{{"$gte", 25}}}}, 2);
        EXPECT_EQ(results.size(), 2);
    }

    // 使用自定义处理函数的查询
    {
        std::vector<mc::string>      names;
        mdb::table_query<user_table> executor(users);
        executor.query(mdb::query_builder(mc::dict{{"score", mc::dict{{"$gte", 85.0}}}}), [&names](const user& u) {
            names.push_back(u.name());
            return true;
        });

        EXPECT_EQ(names.size(), 4);
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
        auto result = query_one_user(users, mc::dict{{"name", "王五"}});
        EXPECT_TRUE(result != nullptr);
        if (result != nullptr) {
            EXPECT_EQ(result->name(), "王五");
            EXPECT_EQ(result->get_age(), 25);
            EXPECT_DOUBLE_EQ(result->score(), 76.5);
        }
    }

    // 复合条件查询（AND和OR组合）
    {
        auto results = query_user_objects(
            users, mc::dict{
                       {"$or",
                        mc::variants{
                            mc::dict{{"age", mc::dict{{"$lt", 30}}}, {"score", mc::dict{{"$gt", 80.0}}}},
                            mc::dict{{"age", mc::dict{{"$gt", 35}}}, {"score", mc::dict{{"$lt", 90.0}}}},
                        }},
                   });
        EXPECT_EQ(results.size(), 2);
        std::sort(results.begin(), results.end(), [](auto& a, auto& b) {
            return a->name() < b->name();
        });

        if (results.size() == 2) {
            EXPECT_EQ(results[0]->name(), "张三");
            EXPECT_EQ(results[1]->name(), "钱七");
        }
    }
}

// 高级更新测试，使用结构化查询选中对象后更新
TEST_F(table_test, advanced_update)
{
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
        auto target = query_one_user(users, mc::dict{{"name", "Alice"}});
        ASSERT_TRUE(target != nullptr);

        auto updated = users.update(target, user("Alice", 26, 88.0));
        EXPECT_TRUE(updated != nullptr);

        auto result = query_one_user(users, mc::dict{{"name", "Alice"}});
        EXPECT_TRUE(result != nullptr);
        EXPECT_EQ(result->get_age(), 26);
        EXPECT_DOUBLE_EQ(result->score(), 88.0);
    }

    // 使用 map 更新
    {
        auto target = query_one_user(users, mc::dict{{"name", "Bob"}});
        ASSERT_TRUE(target != nullptr);

        auto updated = users.update(target, user("Bob", 30, 88.0));
        EXPECT_TRUE(updated != nullptr);

        auto result = query_one_user(users, mc::dict{{"name", "Bob"}});
        EXPECT_TRUE(result != nullptr);
        EXPECT_DOUBLE_EQ(result->score(), 88.0);
    }

    // 批量更新测试
    {
        auto   targets = query_user_objects(users, mc::dict{{"age", mc::dict{{"$gte", 30}}}});
        size_t updated = 0;
        for (const auto& target : targets) {
            if (users.update(target, user(target->name(), 26, target->score())) != nullptr) {
                updated++;
            }
        }
        EXPECT_EQ(updated, 2);

        auto results = query_user_objects(users, mc::dict{{"age", mc::dict{{"$gte", 26}}}});
        EXPECT_EQ(results.size(), 3);
    }
}

// 高级删除测试，使用结构化查询选中对象后删除
TEST_F(table_test, advanced_remove)
{
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
        auto target = query_one_user(users, mc::dict{{"name", "Alice"}});
        ASSERT_TRUE(target != nullptr);
        EXPECT_TRUE(users.remove(target));

        auto result = query_one_user(users, mc::dict{{"name", "Alice"}});
        EXPECT_FALSE(result != nullptr);
    }

    // 测试删除多个记录
    {
        auto   targets = query_user_objects(users, mc::dict{{"age", mc::dict{{"$gte", 30}}}});
        size_t removed = 0;
        for (const auto& target : targets) {
            if (users.remove(target)) {
                removed++;
            }
        }
        EXPECT_EQ(removed, 3);

        auto results = query_user_objects(users, mc::dict{{"age", mc::dict{{"$gte", 30}}}});
        EXPECT_EQ(results.size(), 0);
    }

    EXPECT_TRUE(users.empty());

    // 测试复合条件删除
    {
        user u5("Eve", 25, 80.0);
        user u6("Frank", 30, 85.0);
        user u7("Grace", 35, 90.0);
        users.add(u5);
        users.add(u6);
        users.add(u7);

        auto targets =
            query_user_objects(users, mc::dict{{"age", mc::dict{{"$gte", 30}}}, {"score", mc::dict{{"$lt", 90.0}}}});
        size_t removed = 0;
        for (const auto& target : targets) {
            if (users.remove(target)) {
                removed++;
            }
        }
        EXPECT_EQ(removed, 1);

        auto results =
            query_user_objects(users, mc::dict{{"age", mc::dict{{"$gte", 30}}}, {"score", mc::dict{{"$lt", 90.0}}}});
        EXPECT_EQ(results.size(), 0);

        EXPECT_EQ(users.size(), 2);
        auto all = users.all();
        EXPECT_EQ(all.size(), 2);

        users.clear();
        EXPECT_TRUE(users.empty());
    }

    // 测试事务中的删除
    {
        user u8("Henry", 25, 75.0);
        user u9("Ivy", 30, 80.0);
        users.add(u8);
        users.add(u9);

        auto& txn = mc::db::transaction::get_instance();

        auto   targets = query_user_objects(users, mc::dict{{"score", mc::dict{{"$lt", 85.0}}}});
        size_t removed = 0;
        for (const auto& target : targets) {
            if (users.remove(target, &txn)) {
                removed++;
            }
        }
        EXPECT_EQ(removed, 2);
        EXPECT_EQ(users.size(), 0);

        txn.commit();

        auto results_after = query_user_objects(users, mc::dict{{"score", mc::dict{{"$lt", 85.0}}}});
        EXPECT_EQ(results_after.size(), 0);
        EXPECT_TRUE(users.empty());
    }

    // 测试事务中的删除后回滚
    {
        user u8("Henry", 25, 75.0);
        user u9("Ivy", 30, 80.0);
        users.add(u8);
        users.add(u9);

        auto& txn = mc::db::transaction::get_instance();

        auto   targets = query_user_objects(users, mc::dict{{"score", mc::dict{{"$lt", 85.0}}}});
        size_t removed = 0;
        for (const auto& target : targets) {
            if (users.remove(target, &txn)) {
                removed++;
            }
        }
        EXPECT_EQ(removed, 2);
        EXPECT_EQ(users.size(), 0);

        txn.rollback();

        auto results_after = query_user_objects(users, mc::dict{{"score", mc::dict{{"$lt", 85.0}}}});
        EXPECT_EQ(results_after.size(), 2);

        users.clear();
        EXPECT_TRUE(users.empty());
    }
}

TEST_F(table_test, index_name)
{
    user_table users;

    // 使用反射名命名索引
    EXPECT_EQ(users.get<1>().index_name(), "name");

    // 非反射信息的索引，使用索引编号命名
    EXPECT_EQ(users.get<2>().index_name(), "index_2");

    // MC_FIELD_INDEX_TAG 自定义的命名索引，使用自定义的名字
    EXPECT_EQ(users.get<3>().index_name(), "name_age");
}

TEST_F(table_test, index_name_composite)
{
    using user_table_1 = mdb::table<user, mdb::indexed_by<mdb::ordered_unique<&user::m_name, &user::get_age>>>;
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

// 测试 table::update 触发 on_object_updated 和 update_index 分支
TEST_F(table_test, update_existing_record)
{
    user_table users;

    // 添加一些测试数据
    user u1("Alice", 25, 85.5);
    user u2("Bob", 30, 90.0);
    users.add(u1);
    users.add(u2);

    // 验证初始状态
    auto it1 = users.get<1>().find("Alice");
    ASSERT_FALSE(it1.is_end());
    EXPECT_EQ(it1->get_age(), 25);
    EXPECT_DOUBLE_EQ(it1->score(), 85.5);

    // 记录 on_object_updated 是否被调用
    bool  update_called = false;
    user* old_obj_ptr   = nullptr;
    user* new_obj_ptr   = nullptr;

    // 连接 on_object_updated 信号
    users.on_object_updated.connect([&](mc::db::object_base& old_obj, mc::db::object_base& new_obj) {
        update_called = true;
        old_obj_ptr   = static_cast<user*>(&old_obj);
        new_obj_ptr   = static_cast<user*>(&new_obj);
    });

    // 获取原始对象指针
    auto old_obj_it = users.get<1>().find("Alice");
    ASSERT_FALSE(old_obj_it.is_end());

    auto old_obj_shared_ptr = mc::shared_ptr<user>(const_cast<user*>(&old_obj_it.get()));
    ASSERT_TRUE(old_obj_shared_ptr != nullptr);

    // 更新记录（修改非主键字段，触发索引更新）
    user u1_updated("Alice", 26, 88.0); // 修改年龄和分数
    auto new_obj = users.update(old_obj_shared_ptr, u1_updated);
    ASSERT_TRUE(new_obj != nullptr);

    // 验证 on_object_updated 被调用
    EXPECT_TRUE(update_called);
    // 注意：old_obj_ptr 和 new_obj_ptr 在信号回调中获取，验证它们的值
    EXPECT_EQ(old_obj_ptr->get_age(), 25); // 旧对象的年龄（在回调中保存）
    EXPECT_EQ(new_obj_ptr->get_age(), 26); // 新对象的年龄（在回调中保存）

    // 验证索引已更新（通过年龄索引查找）
    auto it2 = users.get<2>().find(26); // 通过年龄索引查找
    ASSERT_FALSE(it2.is_end());
    EXPECT_EQ(it2->name(), "Alice");
    EXPECT_EQ(it2->get_age(), 26);
    EXPECT_DOUBLE_EQ(it2->score(), 88.0);

    // 验证旧年龄索引中不再有该记录
    auto it3 = users.get<2>().find(25);
    if (!it3.is_end()) {
        // 如果找到，应该不是 Alice
        EXPECT_NE(it3->name(), "Alice");
    }

    // 验证主键索引仍然可以找到
    auto it4 = users.get<1>().find("Alice");
    ASSERT_FALSE(it4.is_end());
    EXPECT_EQ(it4->get_age(), 26);
    EXPECT_DOUBLE_EQ(it4->score(), 88.0);
}
