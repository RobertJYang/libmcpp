/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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
#include <memory>
#include <vector>

#include <mc/db/database.h>
#include <mc/db/query/query.h>
#include <mc/db/transaction.h>
#include <mc/dict.h>
#include <mc/reflect.h>

namespace {

namespace mdb = mc::db;

struct by_id : mdb::tag_base<by_id> {};

class test_object : public mdb::object_base {
public:
    MC_REFLECTABLE("mc.test.db.QueryDatabaseObject")

    test_object() = default;

    test_object(uint32_t id, mc::string name, int value) : m_id(id), m_name(std::move(name)), m_value(value)
    {}

    ~test_object() override
    {}

    uint32_t id() const
    {
        return m_id;
    }

    const mc::string& name() const
    {
        return m_name;
    }

    int value() const
    {
        return m_value;
    }

    uint32_t   m_id;
    mc::string m_name;
    int        m_value;
};

} // namespace

MC_REFLECT(test_object, ((m_id, "id"))((m_name, "name"))((m_value, "value")))

namespace {

using test_table = mdb::table<test_object, mdb::indexed_by<mdb::ordered_unique<&test_object::m_id, by_id::tag>,
                                                           mdb::ordered_non_unique<&test_object::m_name>>>;

class database_test : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_table = std::make_shared<test_table>("test_table");
        m_db.register_table(m_table);

        m_db.add("test_table", mc::dict{{"id", 1}, {"name", "test1"}, {"value", 100}});
        m_db.add("test_table", mc::dict{{"id", 2}, {"name", "test2"}, {"value", 200}});
        m_db.add("test_table", mc::dict{{"id", 3}, {"name", "test3"}, {"value", 300}});
        m_db.add("test_table", mc::dict{{"id", 4}, {"name", "test1"}, {"value", 400}});
    }

    void TearDown() override
    {
        m_db.clear("test_table");
        mdb::transaction::reset_for_test();
    }

    std::vector<uint32_t> query_ids(const mc::dict& spec, size_t limit = 0)
    {
        mdb::table_query<test_table> executor(*m_table);
        auto                         results = executor.query(mdb::query_builder(spec), limit);

        std::vector<uint32_t> ids;
        ids.reserve(results.size());
        for (const auto& obj : results) {
            ids.push_back(obj->id());
        }
        return ids;
    }

    std::vector<uint32_t> query_ids(test_table& table, const mc::dict& spec, size_t limit = 0)
    {
        mdb::table_query<test_table> executor(table);
        auto                         results = executor.query(mdb::query_builder(spec), limit);

        std::vector<uint32_t> ids;
        ids.reserve(results.size());
        for (const auto& obj : results) {
            ids.push_back(obj->id());
        }
        return ids;
    }

    test_table::object_ptr_type query_one(test_table& table, const mc::dict& spec)
    {
        mdb::table_query<test_table> executor(table);
        return executor.query_one(mdb::query_builder(spec));
    }

    mdb::database               m_db;
    std::shared_ptr<test_table> m_table;
};

TEST_F(database_test, query_operations)
{
    {
        auto ids = query_ids(mc::dict{{"id", 2}});
        ASSERT_EQ(ids.size(), 1);
        EXPECT_EQ(ids[0], 2);
    }

    {
        auto ids = query_ids(mc::dict{{"id", mc::dict{{"$gt", 2}}}});
        ASSERT_EQ(ids.size(), 2);
        std::sort(ids.begin(), ids.end());
        EXPECT_EQ(ids[0], 3);
        EXPECT_EQ(ids[1], 4);
    }

    {
        auto ids = query_ids(mc::dict{{"id", mc::dict{{"$lt", 3}}}});
        ASSERT_EQ(ids.size(), 2);
        std::sort(ids.begin(), ids.end());
        EXPECT_EQ(ids[0], 1);
        EXPECT_EQ(ids[1], 2);
    }

    {
        auto ids = query_ids(mc::dict{{"id", mc::dict{{"$gte", 2}, {"$lte", 3}}}});
        ASSERT_EQ(ids.size(), 2);
        std::sort(ids.begin(), ids.end());
        EXPECT_EQ(ids[0], 2);
        EXPECT_EQ(ids[1], 3);
    }

    {
        auto ids = query_ids(mc::dict{{"name", "test1"}});
        ASSERT_EQ(ids.size(), 2);
        std::sort(ids.begin(), ids.end());
        EXPECT_EQ(ids[0], 1);
        EXPECT_EQ(ids[1], 4);
    }

    {
        auto ids = query_ids(mc::dict{{"name", "test1"}, {"value", mc::dict{{"$gt", 200}}}});
        ASSERT_EQ(ids.size(), 1);
        EXPECT_EQ(ids[0], 4);
    }

    {
        auto ids = query_ids(mc::dict{{"id", mc::dict{{"$gt", 0}}}}, 2);
        ASSERT_EQ(ids.size(), 2);
    }

    {
        auto all = m_table->all();
        ASSERT_EQ(all.size(), 4);
    }
}

TEST_F(database_test, multiple_tables)
{
    auto second_table = std::make_shared<test_table>("second_table");
    m_db.register_table(second_table);

    m_db.add("second_table", mc::dict{{"id", 1}, {"name", "table2_a"}, {"value", 1000}});
    m_db.add("second_table", mc::dict{{"id", 2}, {"name", "table2_b"}, {"value", 2000}});

    mdb::table_query<test_table> first_executor(*m_table);
    mdb::table_query<test_table> second_executor(*second_table);

    auto first_ids = first_executor.query(mdb::query_builder(mc::dict{{"name", "test1"}}));
    ASSERT_EQ(first_ids.size(), 2);

    auto second_ids = second_executor.query(mdb::query_builder(mc::dict{{"value", mc::dict{{"$gte", 1500}}}}));
    ASSERT_EQ(second_ids.size(), 1);
    EXPECT_EQ(second_ids[0]->name(), "table2_b");

    m_db.unregister_table("second_table");
}

TEST_F(database_test, transaction_query_visibility)
{
    auto& txn = mdb::transaction::get_instance();

    m_db.add("test_table", mc::dict{{"id", 5}, {"name", "txn"}, {"value", 500}}, &txn);
    auto ids = query_ids(mc::dict{{"name", "txn"}});
    ASSERT_EQ(ids.size(), 1);
    EXPECT_EQ(ids[0], 5);

    txn.rollback();

    EXPECT_TRUE(query_ids(mc::dict{{"name", "txn"}}).empty());
}

TEST_F(database_test, table_operations_and_missing_table_branches)
{
    EXPECT_TRUE(m_db.is_table_registered("test_table"));
    EXPECT_FALSE(m_db.is_table_registered("non_existent_table"));

    auto table = m_db.get_table("test_table");
    ASSERT_TRUE(table != nullptr);
    EXPECT_EQ(table->get_table_name(), "test_table");

    EXPECT_EQ(m_db.size("test_table"), 4);
    EXPECT_FALSE(m_db.empty("test_table"));

    m_db.clear("test_table");
    EXPECT_EQ(m_db.size("test_table"), 0);
    EXPECT_TRUE(m_db.empty("test_table"));

    auto readded = m_db.add("test_table", mc::dict{{"id", 1}, {"name", "test1"}, {"value", 100}});
    EXPECT_TRUE(readded != nullptr);

    EXPECT_FALSE(m_db.add("non_existent_table", mc::dict{{"id", 1}, {"name", "missing"}, {"value", 100}}) != nullptr);

    mdb::database empty_db;
    EXPECT_NO_THROW(empty_db.unregister_table("unknown_table"));
    EXPECT_EQ(empty_db.get_table("non_existent_table"), nullptr);
    EXPECT_FALSE(empty_db.is_table_registered("non_existent_table"));
    EXPECT_FALSE(empty_db.empty("non_existent_table"));
    EXPECT_EQ(empty_db.size("non_existent_table"), 0);

    m_db.unregister_table("test_table");
    EXPECT_FALSE(m_db.is_table_registered("test_table"));
    EXPECT_FALSE(m_db.add("test_table", mc::dict{{"id", 2}, {"name", "after_unregister"}, {"value", 200}}) != nullptr);
}

TEST_F(database_test, transaction_operations)
{
    auto& txn = mdb::transaction::get_instance();

    m_db.add("test_table", mc::dict{{"id", 5}, {"name", "transaction_test"}, {"value", 500}}, &txn);

    auto obj1 = query_one(*m_table, mc::dict{{"id", 1}});
    ASSERT_TRUE(obj1 != nullptr);
    auto updated_obj1 = m_table->update(obj1, test_object(1, "modified_in_transaction", 100), &txn);
    ASSERT_TRUE(updated_obj1 != nullptr);

    auto changed = query_one(*m_table, mc::dict{{"id", 1}});
    ASSERT_TRUE(changed != nullptr);
    EXPECT_EQ(changed->name(), "modified_in_transaction");

    auto added = query_one(*m_table, mc::dict{{"id", 5}});
    ASSERT_TRUE(added != nullptr);
    EXPECT_EQ(added->name(), "transaction_test");

    txn.commit();

    changed = query_one(*m_table, mc::dict{{"id", 1}});
    ASSERT_TRUE(changed != nullptr);
    EXPECT_EQ(changed->name(), "modified_in_transaction");

    added = query_one(*m_table, mc::dict{{"id", 5}});
    ASSERT_TRUE(added != nullptr);
    EXPECT_EQ(added->name(), "transaction_test");
}

TEST_F(database_test, transaction_rollback)
{
    auto& txn = mdb::transaction::get_instance();

    auto obj1 = query_one(*m_table, mc::dict{{"id", 1}});
    ASSERT_TRUE(obj1 != nullptr);
    auto rolled_obj1 = m_table->update(obj1, test_object(1, "will_be_rolled_back", 100), &txn);
    ASSERT_TRUE(rolled_obj1 != nullptr);

    m_db.add("test_table", mc::dict{{"id", 10}, {"name", "temp_object"}, {"value", 1000}}, &txn);

    auto changed = query_one(*m_table, mc::dict{{"id", 1}});
    ASSERT_TRUE(changed != nullptr);
    EXPECT_EQ(changed->name(), "will_be_rolled_back");

    auto temp = query_one(*m_table, mc::dict{{"id", 10}});
    ASSERT_TRUE(temp != nullptr);
    EXPECT_EQ(temp->name(), "temp_object");

    txn.rollback();

    changed = query_one(*m_table, mc::dict{{"id", 1}});
    ASSERT_TRUE(changed != nullptr);
    EXPECT_EQ(changed->name(), "test1");

    temp = query_one(*m_table, mc::dict{{"id", 10}});
    EXPECT_TRUE(temp == nullptr);
}

TEST_F(database_test, multiple_savepoints)
{
    auto& txn = mdb::transaction::get_instance();

    auto& sp1 = txn.alloc_savepoint();

    auto obj1 = query_one(*m_table, mc::dict{{"id", 1}});
    ASSERT_TRUE(obj1 != nullptr);
    ASSERT_TRUE(m_table->update(obj1, test_object(1, "first_change", 100), &txn) != nullptr);
    m_db.add("test_table", mc::dict{{"id", 5}, {"name", "temp_object"}, {"value", 500}}, &txn);

    auto& sp2 = txn.alloc_savepoint();

    auto obj2 = query_one(*m_table, mc::dict{{"id", 2}});
    ASSERT_TRUE(obj2 != nullptr);
    ASSERT_TRUE(m_table->update(obj2, test_object(2, "second_change", 200), &txn) != nullptr);
    m_db.add("test_table", mc::dict{{"id", 6}, {"name", "another_temp"}, {"value", 600}}, &txn);

    txn.rollback_to(sp2);

    auto current1 = query_one(*m_table, mc::dict{{"id", 1}});
    auto current2 = query_one(*m_table, mc::dict{{"id", 2}});
    auto current5 = query_one(*m_table, mc::dict{{"id", 5}});
    auto current6 = query_one(*m_table, mc::dict{{"id", 6}});
    ASSERT_TRUE(current1 != nullptr);
    ASSERT_TRUE(current2 != nullptr);
    ASSERT_TRUE(current5 != nullptr);
    EXPECT_EQ(current1->name(), "first_change");
    EXPECT_EQ(current2->name(), "test2");
    EXPECT_EQ(current5->name(), "temp_object");
    EXPECT_TRUE(current6 == nullptr);

    txn.rollback_to(sp1);

    current1 = query_one(*m_table, mc::dict{{"id", 1}});
    current5 = query_one(*m_table, mc::dict{{"id", 5}});
    ASSERT_TRUE(current1 != nullptr);
    EXPECT_EQ(current1->name(), "test1");
    EXPECT_TRUE(current5 == nullptr);

    m_db.add("test_table", mc::dict{{"id", 7}, {"name", "after_rollback"}, {"value", 700}}, &txn);

    auto current7 = query_one(*m_table, mc::dict{{"id", 7}});
    ASSERT_TRUE(current7 != nullptr);
    EXPECT_EQ(current7->name(), "after_rollback");

    txn.commit();

    current1 = query_one(*m_table, mc::dict{{"id", 1}});
    current7 = query_one(*m_table, mc::dict{{"id", 7}});
    ASSERT_TRUE(current1 != nullptr);
    ASSERT_TRUE(current7 != nullptr);
    EXPECT_EQ(current1->name(), "test1");
    EXPECT_EQ(current7->name(), "after_rollback");
}

TEST_F(database_test, multi_table_transaction)
{
    auto second_table = std::make_shared<test_table>("second_table");
    m_db.register_table(second_table);
    m_db.add("second_table", mc::dict{{"id", 1}, {"name", "second1"}, {"value", 100}});
    m_db.add("second_table", mc::dict{{"id", 2}, {"name", "second2"}, {"value", 200}});

    auto& txn = mdb::transaction::get_instance();
    auto& sp1 = txn.alloc_savepoint();

    auto first_obj1  = query_one(*m_table, mc::dict{{"id", 1}});
    auto second_obj1 = query_one(*second_table, mc::dict{{"id", 1}});
    ASSERT_TRUE(first_obj1 != nullptr);
    ASSERT_TRUE(second_obj1 != nullptr);
    ASSERT_TRUE(m_table->update(first_obj1, test_object(1, "first_table_change1", 100), &txn) != nullptr);
    ASSERT_TRUE(second_table->update(second_obj1, test_object(1, "second_table_change1", 100), &txn) != nullptr);
    m_db.add("test_table", mc::dict{{"id", 10}, {"name", "first_table_new1"}, {"value", 1000}}, &txn);
    m_db.add("second_table", mc::dict{{"id", 10}, {"name", "second_table_new1"}, {"value", 1000}}, &txn);

    auto& sp2 = txn.alloc_savepoint();

    auto first_obj2  = query_one(*m_table, mc::dict{{"id", 2}});
    auto second_obj2 = query_one(*second_table, mc::dict{{"id", 2}});
    ASSERT_TRUE(first_obj2 != nullptr);
    ASSERT_TRUE(second_obj2 != nullptr);
    ASSERT_TRUE(m_table->update(first_obj2, test_object(2, "first_table_change2", 200), &txn) != nullptr);
    ASSERT_TRUE(second_table->update(second_obj2, test_object(2, "second_table_change2", 200), &txn) != nullptr);
    m_db.add("test_table", mc::dict{{"id", 20}, {"name", "first_table_new2"}, {"value", 2000}}, &txn);
    m_db.add("second_table", mc::dict{{"id", 20}, {"name", "second_table_new2"}, {"value", 2000}}, &txn);

    txn.rollback_to(sp2);

    auto first_changed1  = query_one(*m_table, mc::dict{{"id", 1}});
    auto first_changed2  = query_one(*m_table, mc::dict{{"id", 2}});
    auto first_added10   = query_one(*m_table, mc::dict{{"id", 10}});
    auto first_added20   = query_one(*m_table, mc::dict{{"id", 20}});
    auto second_changed1 = query_one(*second_table, mc::dict{{"id", 1}});
    auto second_changed2 = query_one(*second_table, mc::dict{{"id", 2}});
    auto second_added10  = query_one(*second_table, mc::dict{{"id", 10}});
    auto second_added20  = query_one(*second_table, mc::dict{{"id", 20}});

    ASSERT_TRUE(first_changed1 != nullptr);
    ASSERT_TRUE(first_changed2 != nullptr);
    ASSERT_TRUE(first_added10 != nullptr);
    ASSERT_TRUE(second_changed1 != nullptr);
    ASSERT_TRUE(second_changed2 != nullptr);
    ASSERT_TRUE(second_added10 != nullptr);
    EXPECT_EQ(first_changed1->name(), "first_table_change1");
    EXPECT_EQ(first_changed2->name(), "test2");
    EXPECT_EQ(first_added10->name(), "first_table_new1");
    EXPECT_TRUE(first_added20 == nullptr);
    EXPECT_EQ(second_changed1->name(), "second_table_change1");
    EXPECT_EQ(second_changed2->name(), "second2");
    EXPECT_EQ(second_added10->name(), "second_table_new1");
    EXPECT_TRUE(second_added20 == nullptr);

    txn.rollback_to(sp1);

    first_changed1  = query_one(*m_table, mc::dict{{"id", 1}});
    first_added10   = query_one(*m_table, mc::dict{{"id", 10}});
    second_changed1 = query_one(*second_table, mc::dict{{"id", 1}});
    second_added10  = query_one(*second_table, mc::dict{{"id", 10}});
    ASSERT_TRUE(first_changed1 != nullptr);
    ASSERT_TRUE(second_changed1 != nullptr);
    EXPECT_EQ(first_changed1->name(), "test1");
    EXPECT_TRUE(first_added10 == nullptr);
    EXPECT_EQ(second_changed1->name(), "second1");
    EXPECT_TRUE(second_added10 == nullptr);

    auto first_obj3 = query_one(*m_table, mc::dict{{"id", 3}});
    ASSERT_TRUE(first_obj3 != nullptr);
    ASSERT_TRUE(m_table->update(first_obj3, test_object(3, "after_all_rollbacks_1", 300), &txn) != nullptr);

    second_obj1 = query_one(*second_table, mc::dict{{"id", 1}});
    ASSERT_TRUE(second_obj1 != nullptr);
    ASSERT_TRUE(second_table->update(second_obj1, test_object(1, "after_all_rollbacks_2", 100), &txn) != nullptr);

    txn.commit();

    first_obj3  = query_one(*m_table, mc::dict{{"id", 3}});
    second_obj1 = query_one(*second_table, mc::dict{{"id", 1}});
    ASSERT_TRUE(first_obj3 != nullptr);
    ASSERT_TRUE(second_obj1 != nullptr);
    EXPECT_EQ(first_obj3->name(), "after_all_rollbacks_1");
    EXPECT_EQ(second_obj1->name(), "after_all_rollbacks_2");

    m_db.unregister_table("second_table");
}

} // namespace
