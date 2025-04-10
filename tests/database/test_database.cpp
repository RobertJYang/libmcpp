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
#include <mc/database/database.h>
#include <mc/dict.h>
#include <mc/variant.h>

using namespace mc;
using namespace mc::database;

namespace {

// 定义标签类型
struct by_id : tag_base {};

// 测试用的对象类
class test_object : public object<test_object> {
public:
    using object_id_type = uint32_t;

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

// 使用限定命名空间访问
using test_table =
    table<test_object,
          indexed_by<ordered_unique<member<test_object, uint32_t, &test_object::m_id>, by_id>,
                     ordered_non_unique<member<test_object, std::string, &test_object::m_name>>>>;

// 拿到表的字段，可用于后续构造查询语句
auto field_id    = test_object::field(&test_object::m_id);
auto field_name  = test_object::field(&test_object::m_name);
auto field_value = test_object::field(&test_object::m_value);

} // namespace

MC_REFLECT(test_object, ((m_id, "id"))((m_name, "name"))((m_value, "value")))

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
    }

    mc::database::database      db;
    std::shared_ptr<test_table> table;
};

// 测试工厂类的基本功能
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

// 测试工厂类的错误处理
TEST_F(database_test, error_handling) {
    // 尝试添加对象到未注册的表
    dict test_obj{{"id", 1}, {"name", "test"}, {"value", 100}};

    EXPECT_FALSE(db.add("non_existent_table", test_obj) != nullptr);

    // 尝试从未注册的表移除对象
    EXPECT_FALSE(db.remove("non_existent_table", field_id == 1));
}

// 测试工厂类的多表操作
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
    auto obj1 = db.query<test_object>("test_table", field_id == 2)[0];
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
    auto obj3 = db.query<test_object>("test_table", field_id == 3)[0];
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