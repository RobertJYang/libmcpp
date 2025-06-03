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
#include <mc/db/database.h>
#include <mc/dict.h>
#include <mc/variant.h>

using namespace mc;
using namespace mc::db;

namespace {

// 定义标签类型
struct by_id : tag_base<by_id> {};

// 测试用的对象类
class test_object : public object_base {
public:
    test_object() = default;

    test_object(uint32_t id, std::string name, int value)
        : m_id(id), m_name(std::move(name)), m_value(value) {
    }

    ~test_object() override {
    }

    uint32_t id() const {
        return m_id;
    }

    const std::string& name() const {
        return m_name;
    }

    int value() const {
        return m_value;
    }

    uint32_t    m_id;
    std::string m_name;
    int         m_value;
};
} // namespace

MC_REFLECT(test_object, ((m_id, "id"))((m_name, "name"))((m_value, "value")))

namespace {

// 使用限定命名空间访问
using test_table = table<test_object, indexed_by<ordered_unique<&test_object::m_id, by_id::tag>,
                                                 ordered_non_unique<&test_object::m_name>>>;

// 拿到表的字段，可用于后续构造查询语句
auto field_id    = mc::db::field(&test_object::m_id);
auto field_name  = mc::db::field(&test_object::m_name);
auto field_value = mc::db::field(&test_object::m_value);

// 数据库测试类
class database_test : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试表
        table = std::make_shared<test_table>("test_table");

        // 注册表
        db.register_table(table);

        // 添加测试数据
        db.add("test_table", dict{{"id", 1}, {"name", "test1"}, {"value", 100}});
        db.add("test_table", dict{{"id", 2}, {"name", "test2"}, {"value", 200}});
        db.add("test_table", dict{{"id", 3}, {"name", "test3"}, {"value", 300}});
        db.add("test_table", dict{{"id", 4}, {"name", "test1"}, {"value", 400}}); // 重复的name
    }

    void TearDown() override {
        db.clear("test_table");
        mc::db::transaction::reset_for_test();
    }

    mc::db::database            db;
    std::shared_ptr<test_table> table;
};

} // namespace

// 测试表的基本功能
TEST_F(database_test, basic_operations) {
    // 验证对象是否被添加到表中
    auto it1 = table->get<by_id>().find(1);
    ASSERT_FALSE(it1.is_end());
    EXPECT_EQ(it1->name(), "test1");
    EXPECT_EQ(it1->value(), 100);

    auto it2 = table->get<by_id>().find(2);
    ASSERT_FALSE(it2.is_end());
    EXPECT_EQ(it2->name(), "test2");
    EXPECT_EQ(it2->value(), 200);

    // 移除对象
    EXPECT_TRUE(db.remove("test_table", field_id == 1));

    // 验证对象是否被移除
    it1 = table->get<by_id>().find(1);
    EXPECT_TRUE(it1.is_end());
}

// 测试数据库的错误处理
TEST_F(database_test, error_handling) {
    // 尝试添加对象到未注册的表
    dict test_obj{{"id", 1}, {"name", "test"}, {"value", 100}};

    EXPECT_FALSE(db.add("non_existent_table", test_obj) != nullptr);

    // 尝试从未注册的表移除对象
    EXPECT_FALSE(db.remove("non_existent_table", field_id == 1));
}

// 测试数据库的多表操作
TEST_F(database_test, multiple_tables) {
    // 创建多个测试表
    auto table1 = std::make_shared<test_table>("test_table1");
    auto table2 = std::make_shared<test_table>("test_table2");

    // 注册表
    db.register_table(table1);
    db.register_table(table2);

    auto user1 = db.add("test_table1", dict{{"id", 1}, {"name", "table1"}, {"value", 100}});
    auto user2 = db.add("test_table2", dict{{"id", 2}, {"name", "table2"}, {"value", 200}});

    // 向不同表添加对象
    EXPECT_TRUE(user1 != nullptr);
    EXPECT_TRUE(user2 != nullptr);

    // 验证对象被正确添加到各自的表中
    auto ret_obj1 = table1->find(field_id == 1);
    ASSERT_TRUE(ret_obj1 != nullptr);
    EXPECT_EQ(ret_obj1->name(), "table1");

    auto ret_obj2 = table2->find(field_id == 2);
    ASSERT_TRUE(ret_obj2 != nullptr);
    EXPECT_EQ(ret_obj2->name(), "table2");
}

// 测试查询功能
TEST_F(database_test, query_operations) {
    // 测试等值查询
    auto results1 = db.query<test_object>("test_table", field_id == 2);
    ASSERT_EQ(results1.size(), 1);
    EXPECT_EQ(results1[0]->id(), 2);
    EXPECT_EQ(results1[0]->name(), "test2");

    // 测试大于查询
    auto results2 = db.query<test_object>("test_table", field_id > 2);
    ASSERT_EQ(results2.size(), 2);

    // 测试小于查询
    auto results3 = db.query<test_object>("test_table", field_id < 3);
    ASSERT_EQ(results3.size(), 2);

    // 测试范围查询
    auto results4 = db.query<test_object>("test_table", field_id >= 2 && field_id <= 3);
    ASSERT_EQ(results4.size(), 2);

    // 测试字符串查询
    auto results5 = db.query<test_object>("test_table", field_name == "test1");
    ASSERT_EQ(results5.size(), 2);

    // 测试复合条件查询
    auto results6 = db.query<test_object>("test_table", field_name == "test1" && field_value > 200);
    ASSERT_EQ(results6.size(), 1);
    EXPECT_EQ(results6[0]->id(), 4);

    // 测试查询结果限制
    auto results7 = db.query<test_object>("test_table", field_id > 0, 2);
    ASSERT_EQ(results7.size(), 2);

    // 测试获取所有对象
    auto all_results = db.all<test_object>("test_table");
    ASSERT_EQ(all_results.size(), 4);
}

// 测试更新功能
TEST_F(database_test, update_operations) {
    // 更新单个对象
    dict updates1{{"name", "updated_name"}, {"value", 999}};
    EXPECT_TRUE(db.update("test_table", field_id == 2, updates1));

    // 验证更新结果
    auto obj1 = db.find<test_object>("test_table", field_id == 2);
    ASSERT_TRUE(obj1 != nullptr);
    EXPECT_EQ(obj1->name(), "updated_name");
    EXPECT_EQ(obj1->value(), 999);

    // 更新多个对象
    dict updates2{{"value", 1000}};
    EXPECT_TRUE(db.update("test_table", field_name == "test1", updates2));

    // 验证更新结果
    auto objs2 = db.query<test_object>("test_table", field_name == "test1");
    ASSERT_EQ(objs2.size(), 2);
    for (const auto& obj : objs2) {
        EXPECT_EQ(obj->value(), 1000);
    }

    // 使用map更新
    std::map<std::string, variant> updates3{{"name", "map_updated"}, {"value", 888}};
    EXPECT_TRUE(db.update("test_table", field_id == 3, updates3));

    // 验证更新结果
    auto obj3 = db.find<test_object>("test_table", field_id == 3);
    ASSERT_TRUE(obj3 != nullptr);
    EXPECT_EQ(obj3->name(), "map_updated");
    EXPECT_EQ(obj3->value(), 888);
}

// 测试表操作功能
TEST_F(database_test, table_operations) {
    // 测试表注册状态
    EXPECT_TRUE(db.is_table_registered("test_table"));
    EXPECT_FALSE(db.is_table_registered("non_existent_table"));

    // 测试获取表
    auto retrieved_table = db.get_table("test_table");
    EXPECT_EQ(retrieved_table->get_table_name(), "test_table");

    // 测试获取表大小
    EXPECT_EQ(db.size("test_table"), 4);

    // 测试清空表
    db.clear("test_table");
    EXPECT_EQ(db.size("test_table"), 0);
    EXPECT_TRUE(db.empty("test_table"));

    // 重新添加数据
    db.add("test_table", dict{{"id", 1}, {"name", "test1"}, {"value", 100}});
    EXPECT_EQ(db.size("test_table"), 1);
    EXPECT_FALSE(db.empty("test_table"));

    // 测试注销表
    db.unregister_table("test_table");
    EXPECT_FALSE(db.is_table_registered("test_table"));

    // 尝试操作已注销的表
    EXPECT_FALSE(db.add("test_table", dict{{"id", 2}, {"name", "test2"}, {"value", 200}}) !=
                 nullptr);
}

// 测试数据库事务功能
TEST_F(database_test, transaction_operations) {
    // 开始事务
    auto& txn = mc::db::transaction::get_instance();

    // 添加新数据
    db.add("test_table", dict{{"id", 5}, {"name", "transaction_test"}, {"value", 500}}, &txn);

    // 修改数据
    db.update("test_table", field_id == 1, dict{{"name", "modified_in_transaction"}}, &txn);

    // 验证数据在事务中可见
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "modified_in_transaction");

        auto obj5 = db.find<test_object>("test_table", field_id == 5);
        ASSERT_TRUE(obj5 != nullptr);
        EXPECT_EQ(obj5->name(), "transaction_test");
    }

    // 提交事务
    txn.commit();

    // 验证修改被保存
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "modified_in_transaction");

        auto obj5 = db.find<test_object>("test_table", field_id == 5);
        ASSERT_TRUE(obj5 != nullptr);
        EXPECT_EQ(obj5->name(), "transaction_test");
    }
}

// 测试事务回滚功能
TEST_F(database_test, transaction_rollback) {
    // 开始事务
    auto& txn = mc::db::transaction::get_instance();

    // 修改数据
    db.update("test_table", field_id == 1, dict{{"name", "will_be_rolled_back"}}, &txn);
    db.add("test_table", dict{{"id", 10}, {"name", "temp_object"}, {"value", 1000}}, &txn);

    // 验证数据在事务中可见
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "will_be_rolled_back");

        auto obj10 = db.find<test_object>("test_table", field_id == 10);
        ASSERT_TRUE(obj10 != nullptr);
    }

    // 回滚事务
    txn.rollback();

    // 验证修改被撤销
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "test1"); // 恢复为原始值

        auto obj10 = db.find<test_object>("test_table", field_id == 10);
        ASSERT_TRUE(obj10 == nullptr);
    }
}

// 测试事务保存点功能
TEST_F(database_test, transaction_savepoint) {
    // 开始事务
    auto& txn = mc::db::transaction::get_instance();

    // 第一批修改
    db.update("test_table", field_id == 1, dict{{"name", "first_change"}}, &txn);

    // 创建保存点
    auto& sp1 = txn.alloc_savepoint();

    // 第二批修改
    db.update("test_table", field_id == 2, dict{{"name", "second_change"}}, &txn);
    db.add("test_table", dict{{"id", 20}, {"name", "after_savepoint"}, {"value", 2000}}, &txn);

    // 验证所有修改都可见
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "first_change");

        auto obj2 = db.find<test_object>("test_table", field_id == 2);
        ASSERT_TRUE(obj2 != nullptr);
        EXPECT_EQ(obj2->name(), "second_change");

        auto obj20 = db.find<test_object>("test_table", field_id == 20);
        ASSERT_TRUE(obj20 != nullptr);
    }

    // 回滚到保存点
    txn.rollback_to(sp1);

    // 验证第一批修改仍然存在，第二批修改被撤销
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "first_change"); // 第一批修改保留

        auto obj2 = db.find<test_object>("test_table", field_id == 2);
        ASSERT_TRUE(obj2 != nullptr);
        EXPECT_EQ(obj2->name(), "test2"); // 第二批修改撤销

        auto obj20 = db.find<test_object>("test_table", field_id == 20);
        ASSERT_TRUE(obj20 == nullptr); // 添加的对象应该消失
    }

    // 提交事务，确认第一批修改被保存
    txn.commit();

    // 验证第一批修改仍然存在
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "first_change");
    }
}

// 测试多个事务回滚点功能
TEST_F(database_test, multiple_savepoints) {
    // 检查初始状态
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "test1");

        auto obj2 = db.find<test_object>("test_table", field_id == 2);
        ASSERT_TRUE(obj2 != nullptr);
        EXPECT_EQ(obj2->name(), "test2");
    }

    // 开始事务
    auto& txn = mc::db::transaction::get_instance();

    // 创建第一个保存点
    auto& sp1 = txn.alloc_savepoint();

    // 第一批修改
    db.update("test_table", field_id == 1, dict{{"name", "first_change"}}, &txn);
    db.add("test_table", dict{{"id", 5}, {"name", "temp_object"}, {"value", 500}}, &txn);

    // 验证第一批修改生效
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "first_change");

        auto obj5 = db.find<test_object>("test_table", field_id == 5);
        ASSERT_TRUE(obj5 != nullptr);
        EXPECT_EQ(obj5->name(), "temp_object");
    }

    // 创建第二个保存点
    auto& sp2 = txn.alloc_savepoint();

    // 第二批修改
    db.update("test_table", field_id == 2, dict{{"name", "second_change"}}, &txn);
    db.add("test_table", dict{{"id", 6}, {"name", "another_temp"}, {"value", 600}}, &txn);

    // 验证第二批修改生效
    {
        auto obj2 = db.find<test_object>("test_table", field_id == 2);
        ASSERT_TRUE(obj2 != nullptr);
        EXPECT_EQ(obj2->name(), "second_change");

        auto obj6 = db.find<test_object>("test_table", field_id == 6);
        ASSERT_TRUE(obj6 != nullptr);
        EXPECT_EQ(obj6->name(), "another_temp");
    }

    // 回滚到第二个保存点
    txn.rollback_to(sp2);

    // 验证第二批修改被撤销，第一批修改仍然存在
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "first_change"); // 第一批修改保留

        auto obj2 = db.find<test_object>("test_table", field_id == 2);
        ASSERT_TRUE(obj2 != nullptr);
        EXPECT_EQ(obj2->name(), "test2"); // 第二批修改撤销，恢复原值

        auto obj5 = db.find<test_object>("test_table", field_id == 5);
        ASSERT_TRUE(obj5 != nullptr);
        EXPECT_EQ(obj5->name(), "temp_object"); // 第一批添加的对象保留

        auto obj6 = db.find<test_object>("test_table", field_id == 6);
        ASSERT_TRUE(obj6 == nullptr);
    }

    // 回滚到第一个保存点
    txn.rollback_to(sp1);

    // 验证第一批修改也被撤销
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "test1"); // 第一批修改撤销，恢复原值

        auto obj5 = db.find<test_object>("test_table", field_id == 5);
        ASSERT_TRUE(obj5 == nullptr);
    }

    // 再次添加对象以验证事务仍然有效
    db.add("test_table", dict{{"id", 7}, {"name", "after_rollback"}, {"value", 700}}, &txn);

    // 验证新添加的对象可见
    {
        auto obj7 = db.find<test_object>("test_table", field_id == 7);
        ASSERT_TRUE(obj7 != nullptr);
        EXPECT_EQ(obj7->name(), "after_rollback");
    }

    // 提交事务
    txn.commit();

    // 验证提交后的状态
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "test1"); // 原始值保持不变

        auto obj7 = db.find<test_object>("test_table", field_id == 7);
        ASSERT_TRUE(obj7 != nullptr);
        EXPECT_EQ(obj7->name(), "after_rollback"); // 回滚后添加的对象保留
    }
}

// 测试多表事务功能
TEST_F(database_test, multi_table_transaction) {
    // 创建第二张测试表
    auto second_table = std::make_shared<test_table>("second_table");
    db.register_table(second_table);

    // 向第二张表添加初始数据
    db.add("second_table", dict{{"id", 1}, {"name", "second1"}, {"value", 100}});
    db.add("second_table", dict{{"id", 2}, {"name", "second2"}, {"value", 200}});

    // 检查两张表的初始状态
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "test1");

        auto second_obj1 = db.find<test_object>("second_table", field_id == 1);
        ASSERT_TRUE(second_obj1 != nullptr);
        EXPECT_EQ(second_obj1->name(), "second1");
    }

    // 开始事务
    auto& txn = mc::db::transaction::get_instance();

    // 创建第一个保存点
    auto& sp1 = txn.alloc_savepoint();

    // 第一批修改 - 同时修改两张表
    db.update("test_table", field_id == 1, dict{{"name", "first_table_change1"}}, &txn);
    db.update("second_table", field_id == 1, dict{{"name", "second_table_change1"}}, &txn);
    db.add("test_table", dict{{"id", 10}, {"name", "first_table_new1"}, {"value", 1000}}, &txn);
    db.add("second_table", dict{{"id", 10}, {"name", "second_table_new1"}, {"value", 1000}}, &txn);

    // 验证第一批修改在两张表上都生效
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "first_table_change1");

        auto obj10 = db.find<test_object>("test_table", field_id == 10);
        ASSERT_TRUE(obj10 != nullptr);
        EXPECT_EQ(obj10->name(), "first_table_new1");

        auto second_obj1 = db.find<test_object>("second_table", field_id == 1);
        ASSERT_TRUE(second_obj1 != nullptr);
        EXPECT_EQ(second_obj1->name(), "second_table_change1");

        auto second_obj10 = db.find<test_object>("second_table", field_id == 10);
        ASSERT_TRUE(second_obj10 != nullptr);
        EXPECT_EQ(second_obj10->name(), "second_table_new1");
    }

    // 创建第二个保存点
    auto& sp2 = txn.alloc_savepoint();

    // 第二批修改 - 同时修改两张表
    db.update("test_table", field_id == 2, dict{{"name", "first_table_change2"}}, &txn);
    db.update("second_table", field_id == 2, dict{{"name", "second_table_change2"}}, &txn);
    db.add("test_table", dict{{"id", 20}, {"name", "first_table_new2"}, {"value", 2000}}, &txn);
    db.add("second_table", dict{{"id", 20}, {"name", "second_table_new2"}, {"value", 2000}}, &txn);

    // 验证第二批修改在两张表上都生效
    {
        auto obj2 = db.find<test_object>("test_table", field_id == 2);
        ASSERT_TRUE(obj2 != nullptr);
        EXPECT_EQ(obj2->name(), "first_table_change2");

        auto obj20 = db.find<test_object>("test_table", field_id == 20);
        ASSERT_TRUE(obj20 != nullptr);
        EXPECT_EQ(obj20->name(), "first_table_new2");

        auto second_obj2 = db.find<test_object>("second_table", field_id == 2);
        ASSERT_TRUE(second_obj2 != nullptr);
        EXPECT_EQ(second_obj2->name(), "second_table_change2");

        auto second_obj20 = db.find<test_object>("second_table", field_id == 20);
        ASSERT_TRUE(second_obj20 != nullptr);
        EXPECT_EQ(second_obj20->name(), "second_table_new2");
    }

    // 回滚到第二个保存点
    txn.rollback_to(sp2);

    // 验证两张表上的第二批修改都被撤销，第一批修改仍然存在
    {
        // 第一张表检查
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "first_table_change1"); // 第一批修改保留

        auto obj2 = db.find<test_object>("test_table", field_id == 2);
        ASSERT_TRUE(obj2 != nullptr);
        EXPECT_EQ(obj2->name(), "test2"); // 第二批修改撤销

        auto obj10 = db.find<test_object>("test_table", field_id == 10);
        ASSERT_TRUE(obj10 != nullptr);
        EXPECT_EQ(obj10->name(), "first_table_new1"); // 第一批添加的对象保留

        auto obj20 = db.find<test_object>("test_table", field_id == 20);
        ASSERT_TRUE(obj20 == nullptr); // 第二批添加的对象被撤销

        // 第二张表检查
        auto second_obj1 = db.find<test_object>("second_table", field_id == 1);
        ASSERT_TRUE(second_obj1 != nullptr);
        EXPECT_EQ(second_obj1->name(), "second_table_change1"); // 第一批修改保留

        auto second_obj2 = db.find<test_object>("second_table", field_id == 2);
        ASSERT_TRUE(second_obj2 != nullptr);
        EXPECT_EQ(second_obj2->name(), "second2"); // 第二批修改撤销

        auto second_obj10 = db.find<test_object>("second_table", field_id == 10);
        ASSERT_TRUE(second_obj10 != nullptr);
        EXPECT_EQ(second_obj10->name(), "second_table_new1"); // 第一批添加的对象保留

        auto second_obj20 = db.find<test_object>("second_table", field_id == 20);
        ASSERT_TRUE(second_obj20 == nullptr); // 第二批添加的对象被撤销
    }

    // 回滚到第一个保存点
    txn.rollback_to(sp1);

    // 验证两张表上的第一批修改也被撤销
    {
        // 第一张表检查
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "test1"); // 第一批修改撤销

        auto obj10 = db.find<test_object>("test_table", field_id == 10);
        ASSERT_TRUE(obj10 == nullptr); // 第一批添加的对象被撤销

        // 第二张表检查
        auto second_obj1 = db.find<test_object>("second_table", field_id == 1);
        ASSERT_TRUE(second_obj1 != nullptr);
        EXPECT_EQ(second_obj1->name(), "second1"); // 第一批修改撤销

        auto second_obj10 = db.find<test_object>("second_table", field_id == 10);
        ASSERT_TRUE(second_obj10 == nullptr); // 第一批添加的对象被撤销
    }

    // 在完全回滚后再次修改两张表
    db.update("test_table", field_id == 3, dict{{"name", "after_all_rollbacks_1"}}, &txn);
    db.update("second_table", field_id == 1, dict{{"name", "after_all_rollbacks_2"}}, &txn);

    // 验证新修改生效
    {
        auto obj3 = db.find<test_object>("test_table", field_id == 3);
        ASSERT_TRUE(obj3 != nullptr);
        EXPECT_EQ(obj3->name(), "after_all_rollbacks_1");

        auto second_obj1 = db.find<test_object>("second_table", field_id == 1);
        ASSERT_TRUE(second_obj1 != nullptr);
        EXPECT_EQ(second_obj1->name(), "after_all_rollbacks_2");
    }

    // 提交事务
    txn.commit();

    // 验证最终状态
    {
        auto obj1 = db.find<test_object>("test_table", field_id == 1);
        ASSERT_TRUE(obj1 != nullptr);
        EXPECT_EQ(obj1->name(), "test1"); // 原始值保持不变

        auto obj3 = db.find<test_object>("test_table", field_id == 3);
        ASSERT_TRUE(obj3 != nullptr);
        EXPECT_EQ(obj3->name(), "after_all_rollbacks_1"); // 回滚后的修改保留

        auto second_obj1 = db.find<test_object>("second_table", field_id == 1);
        ASSERT_TRUE(second_obj1 != nullptr);
        EXPECT_EQ(second_obj1->name(), "after_all_rollbacks_2"); // 回滚后的修改保留
    }

    // 清理第二张表
    db.unregister_table("second_table");
}

TEST_F(database_test, index_name) {
    auto name_id = table->get<by_id>().index_name();
    EXPECT_EQ(name_id, "id");

    auto name2 = table->get<2>().index_name();
    EXPECT_EQ(name2, "name");
}