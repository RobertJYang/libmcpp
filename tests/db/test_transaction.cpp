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
#include <test_utilities/test_base.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <mc/db/table.h>
#include <mc/db/transaction.h>

namespace {

namespace mdb = mc::db;

// 定义标签类型
struct by_age : mdb::tag_base<by_age> {};
struct by_name_age : mdb::tag_base<by_name_age> {};

class user : public mdb::object_base {
public:
    user() = default;

    user(std::string name, int age) : m_name(std::move(name)), m_age(age) {
    }

    ~user() override {
    }

    const std::string& name() const {
        return m_name;
    }

    int get_age() const {
        return m_age;
    }

    std::string m_name;
    int         m_age;
};

// 使用限定命名空间访问
using user_table = mdb::table<
    user, mdb::indexed_by<mdb::ordered_unique<&user::m_name>,
                          mdb::ordered_non_unique<&user::get_age, by_age::tag>,
                          mdb::ordered_non_unique<&user::m_name, &user::m_age, by_name_age::tag>>>;

} // namespace

// 事务测试类
class transaction_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        mc::singleton<mdb::transaction, mdb::default_transaction_tag>::reset_for_test();

        // 创建测试数据
        u1 = user("张三", 25);
        u2 = user("李四", 30);
        u3 = user("王五", 25);
        u4 = user("赵六", 35);
    }

    void TearDown() override {
        mc::singleton<mdb::transaction, mdb::default_transaction_tag>::reset_for_test();
    }

    user       u1, u2, u3, u4;
    user_table users;
};

// 测试基本事务添加操作
TEST_F(transaction_test, transaction_add) {
    // 获取事务
    auto& txn = mdb::transaction::get_instance();

    // 添加用户到表中，使用事务
    EXPECT_TRUE(users.add(u1, &txn));
    EXPECT_TRUE(users.add(u2, &txn));

    // 验证用户此时可见
    auto it1 = users.get<1>().find("张三");
    ASSERT_FALSE(it1.is_end());
    EXPECT_EQ(it1->name(), "张三");

    // 提交事务
    txn.commit();

    // 用户应该仍然可见
    it1 = users.get<1>().find("张三");
    ASSERT_FALSE(it1.is_end());
    EXPECT_EQ(it1->name(), "张三");

    auto it2 = users.get<1>().find("李四");
    ASSERT_FALSE(it2.is_end());
    EXPECT_EQ(it2->name(), "李四");
}

// 测试事务回滚功能
TEST_F(transaction_test, transaction_rollback) {
    // 获取事务
    auto& txn = mdb::transaction::get_instance();

    // 添加用户到表中，使用事务
    EXPECT_TRUE(users.add(u1, &txn));
    EXPECT_TRUE(users.add(u2, &txn));

    // 验证用户此时可见
    auto it1 = users.get<1>().find("张三");
    ASSERT_FALSE(it1.is_end());
    EXPECT_EQ(it1->name(), "张三");

    auto it2 = users.get<1>().find("李四");
    ASSERT_FALSE(it2.is_end());
    EXPECT_EQ(it2->name(), "李四");

    // 回滚事务
    txn.rollback();

    // 验证用户不可见（已回滚）
    it1 = users.get<1>().find("张三");
    EXPECT_TRUE(it1.is_end());

    it2 = users.get<1>().find("李四");
    EXPECT_TRUE(it2.is_end());
}

// 测试事务保存点
TEST_F(transaction_test, savepoint) {
    // 获取事务
    auto& txn = mdb::transaction::get_instance();

    // 添加第一批用户
    EXPECT_TRUE(users.add(u1, &txn));
    EXPECT_TRUE(users.add(u2, &txn));

    // 验证第一批用户可见
    auto it1 = users.get<1>().find("张三");
    ASSERT_FALSE(it1.is_end());
    EXPECT_EQ(it1->name(), "张三");

    auto it2 = users.get<1>().find("李四");
    ASSERT_FALSE(it2.is_end());
    EXPECT_EQ(it2->name(), "李四");

    // 创建保存点
    auto& savepoint = txn.alloc_savepoint();

    // 添加第二批用户
    EXPECT_TRUE(users.add(u3, &txn));
    EXPECT_TRUE(users.add(u4, &txn));

    // 验证第二批用户可见
    auto it3 = users.get<1>().find("王五");
    ASSERT_FALSE(it3.is_end());
    EXPECT_EQ(it3->name(), "王五");

    auto it4 = users.get<1>().find("赵六");
    ASSERT_FALSE(it4.is_end());
    EXPECT_EQ(it4->name(), "赵六");

    // 回滚到保存点，只回滚第二批用户
    savepoint.rollback();

    // 验证第一批用户仍在
    it1 = users.get<1>().find("张三");
    ASSERT_FALSE(it1.is_end());
    EXPECT_EQ(it1->name(), "张三");

    it2 = users.get<1>().find("李四");
    ASSERT_FALSE(it2.is_end());
    EXPECT_EQ(it2->name(), "李四");

    // 验证第二批用户已被回滚移除
    it3 = users.get<1>().find("王五");
    EXPECT_TRUE(it3.is_end());

    it4 = users.get<1>().find("赵六");
    EXPECT_TRUE(it4.is_end());

    // 提交事务
    txn.commit();

    // 验证最终状态
    it1 = users.get<1>().find("张三");
    ASSERT_FALSE(it1.is_end());
    EXPECT_EQ(it1->name(), "张三");

    it2 = users.get<1>().find("李四");
    ASSERT_FALSE(it2.is_end());
    EXPECT_EQ(it2->name(), "李四");

    it3 = users.get<1>().find("王五");
    EXPECT_TRUE(it3.is_end());

    it4 = users.get<1>().find("赵六");
    EXPECT_TRUE(it4.is_end());
}

// 测试更新和删除操作
TEST_F(transaction_test, transaction_update_remove) {
    // 添加初始用户（不使用事务）
    auto u1_ptr = users.add(u1);
    auto u2_ptr = users.add(u2);
    EXPECT_TRUE(u1_ptr != nullptr);
    EXPECT_TRUE(u2_ptr != nullptr);

    // 获取事务
    auto& txn = mdb::transaction::get_instance();

    // 更新张三的年龄
    user u1_updated("张三", 26);
    EXPECT_TRUE(users.update(u1_ptr, u1_updated, &txn));

    // 删除李四
    EXPECT_TRUE(users.remove(u2_ptr, &txn));

    // 验证更新和删除已经可见
    auto it1 = users.get<1>().find("张三");
    ASSERT_FALSE(it1.is_end());
    EXPECT_EQ(it1->get_age(), 26); // 更新后的年龄

    auto it2 = users.get<1>().find("李四");
    EXPECT_TRUE(it2.is_end()); // 李四已被删除

    // 回滚事务
    txn.rollback();

    // 验证原始状态已恢复
    it1 = users.get<1>().find("张三");
    ASSERT_FALSE(it1.is_end());
    EXPECT_EQ(it1->get_age(), 25); // 原始年龄

    it2 = users.get<1>().find("李四");
    ASSERT_FALSE(it2.is_end()); // 李四恢复
    EXPECT_EQ(it2->name(), "李四");

    txn.commit();
}

// 测试资源合并功能
TEST_F(transaction_test, transaction_merge) {
    auto& txn = mdb::transaction::get_instance();

    // 添加用户到表中，使用事务
    EXPECT_TRUE(users.add(u1, &txn));
    EXPECT_TRUE(users.add(u2, &txn));

    // 测试同一个事务中对同一个资源的多次操作合并
    // 1. 添加用户
    user u3("test3", 30);
    auto u3_ptr = users.add(u3, &txn);
    EXPECT_TRUE(u3_ptr != nullptr);

    // 2. 更新同一个用户
    user u3_updated("test3", 31);
    auto u3_ptr2 = users.update(u3_ptr, u3_updated, &txn);
    EXPECT_TRUE(u3_ptr2 != nullptr);

    // 3. 再次更新同一个用户
    user u3_updated2("test3", 32);
    auto u3_ptr3 = users.update(u3_ptr2, u3_updated2, &txn);
    EXPECT_TRUE(u3_ptr3 != nullptr);

    // 验证最终状态
    auto it = users.get<1>().find("test3");
    ASSERT_FALSE(it.is_end());
    EXPECT_EQ(it->m_name, "test3");
    EXPECT_EQ(it->m_age, 32); // 应该是最后一次更新的值

    // 测试不同事务保存点之间无法合并
    // 第一个保存点：添加用户
    auto& sp1 = txn.alloc_savepoint();
    user  u4("test4", 40);
    auto  u4_ptr = users.add(u4, &txn);
    EXPECT_TRUE(u4_ptr != nullptr);

    // 第二个保存点：更新用户
    auto& sp2 = txn.alloc_savepoint();
    user  u4_updated("test4", 41);
    EXPECT_TRUE(users.update(u4_ptr, u4_updated, &txn) != nullptr);

    // 回滚到第二个保存点，只回滚更新操作
    txn.rollback_to(sp2);

    // 验证回滚后状态 - 用户应该存在但年龄是原始值
    auto it2 = users.get<1>().find("test4");
    ASSERT_FALSE(it2.is_end());
    EXPECT_EQ(it2->m_name, "test4");
    EXPECT_EQ(it2->m_age, 40); // 应该是第一个保存点的值

    // 回滚到第一个保存点，回滚添加操作
    txn.rollback_to(sp1);

    // 验证回滚后状态 - 用户应该不存在
    auto it3 = users.get<1>().find("test4");
    EXPECT_TRUE(it3.is_end()); // 回滚后资源应该不存在

    // 测试同一个保存点内资源合并
    auto& sp3 = txn.alloc_savepoint();

    // 添加用户
    user u5("test5", 50);
    auto u5_ptr = users.add(u5, &txn);
    EXPECT_TRUE(u5_ptr != nullptr);

    // 更新用户（同一保存点内）
    user u5_updated("test5", 51);
    auto u5_ptr2 = users.update(u5_ptr, u5_updated, &txn);
    EXPECT_TRUE(u5_ptr2 != nullptr);

    // 再次更新用户（同一保存点内）
    user u5_updated2("test5", 52);
    EXPECT_TRUE(users.update(u5_ptr2, u5_updated2, &txn) != nullptr);

    // 验证最终状态 - 多次更新应该合并
    auto it4 = users.get<1>().find("test5");
    ASSERT_FALSE(it4.is_end());
    EXPECT_EQ(it4->m_name, "test5");
    EXPECT_EQ(it4->m_age, 52); // 应该是最后一次更新的值

    // 回滚保存点，所有操作都应该回滚
    txn.rollback_to(sp3);

    // 验证回滚后状态 - 用户应该不存在
    auto it5 = users.get<1>().find("test5");
    EXPECT_TRUE(it5.is_end()); // 回滚后资源应该不存在

    // 提交事务
    txn.commit();
}

// 测试资源合并功能 - 详细测试不同场景
TEST_F(transaction_test, transaction_merge_detailed) {
    auto& txn = mdb::transaction::get_instance();

    // 构造资源ID的辅助函数
    auto make_resource_id = [&](const auto& obj_ptr) -> uint64_t {
        // 使用transaction的静态方法获取表ID
        static uint32_t table_id = users.get_table_id();
        return (static_cast<uint64_t>(table_id) << 32) | obj_ptr->get_object_id();
    };

    // 场景1: 添加后更新（add + update）
    // 期望: 合并为一个添加操作，资源链表长度为1
    // 测试逻辑: 添加用户后立即更新，检查资源数量和链表长度，验证数据正确性
    {
        // 首先记录当前资源数量
        size_t initial_resource_count = txn.get_resource_count();

        // 添加用户
        user u_add_update("测试_添加更新", 10);
        auto u_add_update_ptr = users.add(u_add_update, &txn);
        ASSERT_TRUE(u_add_update_ptr != nullptr);

        // 记录添加后的资源ID和数量
        uint64_t resource_id = make_resource_id(u_add_update_ptr);
        EXPECT_TRUE(txn.has_resource(resource_id));

        size_t after_add_resource_count = txn.get_resource_count();
        EXPECT_EQ(after_add_resource_count, initial_resource_count + 1);
        EXPECT_EQ(txn.get_resource_chain_length(resource_id), 1);

        // 更新用户
        user u_add_update_updated("测试_添加更新", 11);
        auto updated_ptr = users.update(u_add_update_ptr, u_add_update_updated, &txn);
        ASSERT_TRUE(updated_ptr != nullptr);

        // 验证合并结果：资源列表长度应该不变，链表长度应该为1
        // 这表明add和update操作被合并为一个操作
        EXPECT_EQ(txn.get_resource_count(), after_add_resource_count);
        EXPECT_EQ(txn.get_resource_chain_length(resource_id), 1);

        // 验证用户数据正确 - 应该是更新后的值
        auto it = users.get<1>().find("测试_添加更新");
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->m_age, 11);

        // 提交事务，清理测试数据
        txn.commit();
    }

    // 场景2: 添加后删除（add + remove）
    // 期望: 资源被标记为无效并从资源映射表中移除
    // 测试逻辑: 添加用户后立即删除，检查资源映射表中是否还存在该资源，验证用户不存在
    {
        // 首先记录当前资源数量
        size_t initial_resource_count = txn.get_resource_count();

        // 添加用户
        user u_add_remove("测试_添加删除", 20);
        auto u_add_remove_ptr = users.add(u_add_remove, &txn);
        ASSERT_TRUE(u_add_remove_ptr != nullptr);

        // 记录添加后的资源ID和数量
        uint64_t resource_id = make_resource_id(u_add_remove_ptr);
        EXPECT_TRUE(txn.has_resource(resource_id));

        size_t after_add_resource_count = txn.get_resource_count();
        EXPECT_EQ(after_add_resource_count, initial_resource_count + 1);

        // 删除用户
        EXPECT_TRUE(users.remove(u_add_remove_ptr, &txn));

        // 验证合并结果：资源应该被标记为无效，不再在资源映射表中
        // 这表明add和remove操作被合并，结果是完全移除了资源
        EXPECT_FALSE(txn.has_resource(resource_id));

        // 验证用户已被删除 - 应该不存在
        auto it = users.get<1>().find("测试_添加删除");
        EXPECT_TRUE(it.is_end());

        // 提交事务，清理测试数据
        txn.commit();
    }

    // 场景3: 多次更新同一对象（update + update + update）
    // 期望: 所有更新合并为一个更新操作，资源链表长度为1
    // 测试逻辑: 先添加用户（不在事务中），然后在事务中多次更新，验证只有最后一次更新生效
    {
        // 首先添加一个用户（不在事务中）
        user u_update("测试_多次更新", 30);
        auto u_update_ptr = users.add(u_update);
        ASSERT_TRUE(u_update_ptr != nullptr);

        // 首先记录当前资源数量
        size_t initial_resource_count = txn.get_resource_count();

        // 第一次更新
        user u_update1("测试_多次更新", 31);
        auto u_update1_ptr = users.update(u_update_ptr, u_update1, &txn);
        ASSERT_TRUE(u_update1_ptr != nullptr);

        // 记录更新后的资源ID和数量
        uint64_t resource_id = make_resource_id(u_update1_ptr);
        EXPECT_TRUE(txn.has_resource(resource_id));

        size_t after_update1_resource_count = txn.get_resource_count();
        EXPECT_EQ(after_update1_resource_count, initial_resource_count + 1);

        // 第二次更新
        user u_update2("测试_多次更新", 32);
        auto u_update2_ptr = users.update(u_update1_ptr, u_update2, &txn);
        ASSERT_TRUE(u_update2_ptr != nullptr);

        // 第三次更新
        user u_update3("测试_多次更新", 33);
        auto u_update3_ptr = users.update(u_update2_ptr, u_update3, &txn);
        ASSERT_TRUE(u_update3_ptr != nullptr);

        // 验证合并结果：资源列表长度应该不变，链表长度应该为1
        // 这表明所有update操作被合并为一个
        EXPECT_EQ(txn.get_resource_count(), after_update1_resource_count);
        EXPECT_EQ(txn.get_resource_chain_length(resource_id), 1);

        // 验证用户数据为最后一次更新的值 - 应该是33
        auto it = users.get<1>().find("测试_多次更新");
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->m_age, 33);

        // 提交事务，清理测试数据
        txn.commit();

        // 清理测试数据
        users.remove(u_update3_ptr);
    }

    // 场景4: 更新后删除（update + remove）
    // 期望: 资源被标记为无效并从资源映射表中移除
    // 测试逻辑: 先添加用户（不在事务中），然后在事务中更新后删除，验证资源被移除
    {
        // 首先添加一个用户（不在事务中）
        user u_update_remove("测试_更新删除", 40);
        auto u_update_remove_ptr = users.add(u_update_remove);
        ASSERT_TRUE(u_update_remove_ptr != nullptr);

        // 首先记录当前资源数量
        size_t initial_resource_count = txn.get_resource_count();

        // 更新用户
        user u_updated("测试_更新删除", 41);
        auto u_updated_ptr = users.update(u_update_remove_ptr, u_updated, &txn);
        ASSERT_TRUE(u_updated_ptr != nullptr);

        // 记录更新后的资源ID和数量
        uint64_t resource_id = make_resource_id(u_updated_ptr);
        EXPECT_TRUE(txn.has_resource(resource_id));

        size_t after_update_resource_count = txn.get_resource_count();
        EXPECT_EQ(after_update_resource_count, initial_resource_count + 1);

        // 删除用户
        EXPECT_TRUE(users.remove(u_updated_ptr, &txn));

        // 验证合并结果：资源应该被标记为无效，不再在资源映射表中
        // 这表明update和remove操作被合并，结果是移除了资源
        EXPECT_FALSE(txn.has_resource(resource_id));

        // 验证用户已被删除 - 应该不存在
        auto it = users.get<1>().find("测试_更新删除");
        EXPECT_TRUE(it.is_end());

        // 提交事务，清理测试数据
        txn.commit();
    }

    // 场景5: 不同保存点下的合并
    // 期望: 不同保存点间的操作不能合并，相同保存点内的操作可以合并
    // 测试逻辑: 添加用户 -> 创建保存点 -> 更新用户 -> 回滚到保存点 -> 再次更新用户
    {
        // 添加用户
        user u_complex("测试_复杂场景", 50);
        auto u_complex_ptr = users.add(u_complex, &txn);
        ASSERT_TRUE(u_complex_ptr != nullptr);

        // 记录资源ID
        uint64_t resource_id = make_resource_id(u_complex_ptr);
        EXPECT_TRUE(txn.has_resource(resource_id));
        EXPECT_EQ(txn.get_resource_chain_length(resource_id), 1);

        // 创建保存点 - 这个保存点将用于测试跨保存点合并
        auto&   sp    = txn.alloc_savepoint();
        int32_t sp_id = txn.last_savepoint_id();

        // 更新用户 - 在新保存点下
        user u_complex_updated("测试_复杂场景", 51);
        auto u_complex_updated_ptr = users.update(u_complex_ptr, u_complex_updated, &txn);
        ASSERT_TRUE(u_complex_updated_ptr != nullptr);

        // 验证结果：应该有两个资源（链接在一起）
        // 这表明跨保存点的操作没有合并
        EXPECT_TRUE(txn.has_resource(resource_id));
        EXPECT_EQ(txn.get_resource_chain_length(resource_id), 2);

        // 回滚到保存点 - 撤销更新操作
        txn.rollback_to(sp);

        // 验证用户数据已回滚到原始状态
        auto it = users.get<1>().find("测试_复杂场景");
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->m_age, 50);

        // 再次更新用户 - 在同一个保存点内
        user u_complex_updated2("测试_复杂场景", 52);
        auto u_complex_updated2_ptr = users.update(u_complex_ptr, u_complex_updated2, &txn);
        ASSERT_TRUE(u_complex_updated2_ptr != nullptr);

        // 验证结果：资源应该合并（因为在同一个保存点）
        // 这表明同一保存点内的操作可以合并
        EXPECT_TRUE(txn.has_resource(resource_id));
        EXPECT_EQ(txn.get_resource_chain_length(resource_id), 1);

        // 验证用户数据 - 应该是最后一次更新的值
        it = users.get<1>().find("测试_复杂场景");
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->m_age, 52);

        txn.commit();
    }

    // 场景6: 删除后增加（remove + add）
    // 期望: 这种操作应该会创建新的资源条目，不会与之前的删除操作合并
    // 测试逻辑: 先添加用户 -> 删除用户 -> 再次添加同名用户 -> 验证资源状态
    {
        // 首先添加一个用户（不在事务中）
        user u_remove_add("测试_删除添加", 60);
        auto u_remove_add_ptr = users.add(u_remove_add);
        ASSERT_TRUE(u_remove_add_ptr != nullptr);

        // 记录当前资源数量
        size_t initial_resource_count = txn.get_resource_count();

        // 删除用户
        EXPECT_TRUE(users.remove(u_remove_add_ptr, &txn));

        // 删除后的资源数量
        size_t after_remove_resource_count = txn.get_resource_count();
        EXPECT_EQ(after_remove_resource_count, initial_resource_count + 1);

        // 再次添加同名用户（使用删除之前的对象ID，否则会认为是新对象）
        user u_remove_add2("测试_删除添加", 61);
        u_remove_add2.set_object_id(u_remove_add_ptr->get_object_id());
        auto u_remove_add_ptr2 = users.add(u_remove_add2, &txn);
        ASSERT_TRUE(u_remove_add_ptr2 != nullptr);

        // 记录添加后的资源ID
        uint64_t resource_id = make_resource_id(u_remove_add_ptr2);

        // 验证资源状态 - 应该有新的资源，不会与删除操作合并
        // 这是因为删除和添加操作涉及的是不同的对象实例（即使名称相同）
        size_t after_add_resource_count = txn.get_resource_count();
        EXPECT_EQ(after_add_resource_count, after_remove_resource_count + 1);

        // 验证用户数据 - 应该是新添加的用户
        auto it = users.get<1>().find("测试_删除添加");
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->m_age, 61);

        // 提交事务，清理测试数据
        txn.commit();
    }
}