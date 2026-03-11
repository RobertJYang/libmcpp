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

#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <mc/im/radix_tree.h>
#include <string>
#include <vector>

namespace mc::im::tests {

class TransactionTest : public ::testing::Test {
protected:
    // 使用显式的模板参数
    using txn_type     = transaction<>;
    using node_type    = typename txn_type::node_type;
    using ref_ptr_type = typename node_type::ref_ptr_type;
    using tree_type    = typename txn_type::tree_type;

    void SetUp() override
    {
        // 创建一些测试用的叶子值
        for (int i = 0; i < 5; i++) {
            leaves.push_back(reinterpret_cast<void*>(static_cast<intptr_t>(i + 1)));
        }

        // 创建事务对象
        txn = std::make_unique<txn_type>();
    }

    void TearDown() override
    {
        // 清理测试资源
        txn.reset();
        leaves.clear();
    }

    // 辅助方法：将指针值转换为整数，方便测试
    intptr_t ptr_to_int(void* ptr)
    {
        return reinterpret_cast<intptr_t>(ptr);
    }

    std::unique_ptr<txn_type> txn;
    std::vector<void*>        leaves;
};

// 测试基本事务操作
TEST_F(TransactionTest, BasicOperations)
{
    // 验证初始状态
    EXPECT_EQ(0, txn->root().size());

    // 插入数据并验证
    auto [old_val1, updated1] = txn->insert("test", leaves[0]);
    EXPECT_EQ(std::nullopt, old_val1);
    EXPECT_FALSE(updated1);
    EXPECT_EQ(1, txn->root().size());

    // 更新数据并验证
    auto [old_val2, updated2] = txn->insert("test", leaves[1]);
    EXPECT_EQ(leaves[0], old_val2);
    EXPECT_TRUE(updated2);
    EXPECT_EQ(1, txn->root().size());

    // 提交事务并验证
    tree_type tree = txn->commit();
    EXPECT_EQ(1, tree.size());

    // 查询数据并验证
    auto val = tree.get("test");
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(leaves[1], val.value());
}

// 测试插入和获取操作
TEST_F(TransactionTest, InsertAndGet)
{
    // 插入多个键值对
    txn->insert("a", leaves[0]);
    txn->insert("ab", leaves[1]);
    txn->insert("abc", leaves[2]);
    txn->insert("b", leaves[3]);

    // 获取并验证值
    auto val_a = txn->get("a");
    EXPECT_TRUE(val_a.has_value());
    EXPECT_EQ(leaves[0], val_a.value());

    auto val_ab = txn->get("ab");
    EXPECT_TRUE(val_ab.has_value());
    EXPECT_EQ(leaves[1], val_ab.value());

    auto val_abc = txn->get("abc");
    EXPECT_TRUE(val_abc.has_value());
    EXPECT_EQ(leaves[2], val_abc.value());

    auto val_b = txn->get("b");
    EXPECT_TRUE(val_b.has_value());
    EXPECT_EQ(leaves[3], val_b.value());

    // 验证不存在的键
    auto val_not_exist = txn->get("c");
    EXPECT_FALSE(val_not_exist.has_value());

    // 验证大小
    EXPECT_EQ(4, txn->root().size());
}

// 测试节点分裂场景
TEST_F(TransactionTest, NodeSplit)
{
    // 首先插入一个键值对
    txn->insert("romane", leaves[0]);

    // 验证初始状态
    auto val1 = txn->get("romane");
    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(leaves[0], val1.value());
    EXPECT_EQ(1, txn->root().size());

    // 插入一个共享前缀但末端不同的键，触发节点分裂
    txn->insert("romano", leaves[1]);

    // 验证两个键都能被正确查找到
    auto val_romane = txn->get("romane");
    EXPECT_TRUE(val_romane.has_value());
    EXPECT_EQ(leaves[0], val_romane.value());

    auto val_romano = txn->get("romano");
    EXPECT_TRUE(val_romano.has_value());
    EXPECT_EQ(leaves[1], val_romano.value());

    // 验证大小为2
    EXPECT_EQ(2, txn->root().size());

    // 再插入一个不同前缀的键，进一步测试树结构
    txn->insert("romanus", leaves[2]);

    auto val_romanus = txn->get("romanus");
    EXPECT_TRUE(val_romanus.has_value());
    EXPECT_EQ(leaves[2], val_romanus.value());

    // 验证大小为3
    EXPECT_EQ(3, txn->root().size());

    // 插入更多相关键以测试复杂分裂场景
    txn->insert("romulus", leaves[3]);
    txn->insert("rubens", leaves[4]);

    // 验证所有键都能正确查找
    auto val_romulus = txn->get("romulus");
    EXPECT_TRUE(val_romulus.has_value());
    EXPECT_EQ(leaves[3], val_romulus.value());

    auto val_rubens = txn->get("rubens");
    EXPECT_TRUE(val_rubens.has_value());
    EXPECT_EQ(leaves[4], val_rubens.value());

    // 验证最终大小
    EXPECT_EQ(5, txn->root().size());

    // 提交事务并验证树的结构保持完整
    tree_type tree = txn->commit();
    EXPECT_EQ(5, tree.size());

    // 验证所有键在提交后的树中仍然可用
    auto tree_val1 = tree.get("romane");
    EXPECT_TRUE(tree_val1.has_value());
    EXPECT_EQ(leaves[0], tree_val1.value());

    auto tree_val2 = tree.get("romano");
    EXPECT_TRUE(tree_val2.has_value());
    EXPECT_EQ(leaves[1], tree_val2.value());
}

// 测试删除操作
TEST_F(TransactionTest, DeleteKey)
{
    // 插入数据
    txn->insert("a", leaves[0]);
    txn->insert("ab", leaves[1]);
    txn->insert("abc", leaves[2]);

    // 删除并验证
    auto val_a = txn->remove("a");
    EXPECT_TRUE(val_a.has_value());
    EXPECT_EQ(leaves[0], val_a.value());

    // 验证删除后的状态
    auto val_a_after = txn->get("a");
    EXPECT_FALSE(val_a_after.has_value());

    // 再次删除
    auto val_a_after2 = txn->remove("a");
    EXPECT_FALSE(val_a_after2.has_value());

    // 验证其他键仍然存在
    auto val_ab = txn->get("ab");
    EXPECT_TRUE(val_ab.has_value());
    EXPECT_EQ(leaves[1], val_ab.value());

    // 验证大小
    EXPECT_EQ(2, txn->root().size());

    // 删除不存在的键
    auto val_not_exist = txn->remove("c");
    EXPECT_FALSE(val_not_exist.has_value());

    // 验证大小不变
    EXPECT_EQ(2, txn->root().size());
}

// 测试删除操作触发的特殊情况
TEST_F(TransactionTest, DeleteSpecialCases)
{
    // 插入数据构造特定的树结构
    txn->insert("abc", leaves[0]);
    txn->insert("abcd", leaves[1]);
    txn->insert("abce", leaves[2]);

    // 验证初始状态
    EXPECT_EQ(3, txn->root().size());

    // 验证所有键都存在
    auto val_abc = txn->get("abc");
    EXPECT_TRUE(val_abc.has_value());
    EXPECT_EQ(leaves[0], val_abc.value());

    auto val_abcd = txn->get("abcd");
    EXPECT_TRUE(val_abcd.has_value());
    EXPECT_EQ(leaves[1], val_abcd.value());

    auto val_abce = txn->get("abce");
    EXPECT_TRUE(val_abce.has_value());
    EXPECT_EQ(leaves[2], val_abce.value());

    // 删除一个中间节点，触发节点合并
    // 删除abc后，abc节点将只有一条边（到d的边），应触发合并
    auto val_deleted = txn->remove("abc");
    EXPECT_TRUE(val_deleted.has_value());
    EXPECT_EQ(leaves[0], val_deleted.value());

    // 验证删除成功
    auto val_abc_after = txn->get("abc");
    EXPECT_FALSE(val_abc_after.has_value());

    // 验证其他键仍然存在
    auto val_abcd_after = txn->get("abcd");
    EXPECT_TRUE(val_abcd_after.has_value());
    EXPECT_EQ(leaves[1], val_abcd_after.value());

    auto val_abce_after = txn->get("abce");
    EXPECT_TRUE(val_abce_after.has_value());
    EXPECT_EQ(leaves[2], val_abce_after.value());

    // 验证大小减少了
    EXPECT_EQ(2, txn->root().size());
}

// 测试返回{nullptr, nullptr}的情况
TEST_F(TransactionTest, NullPtrReturnCases)
{
    // 在空树上尝试获取不存在的键
    auto val1 = txn->get("nonexistent");
    EXPECT_FALSE(val1.has_value());

    // 在空树上尝试删除不存在的键
    auto val2 = txn->remove("nonexistent");
    EXPECT_FALSE(val2.has_value());

    // 添加一些数据
    txn->insert("test", leaves[0]);

    // 尝试获取树中不存在的键
    auto val3 = txn->get("another");
    EXPECT_FALSE(val3.has_value());

    // 尝试删除树中不存在的键
    auto val4 = txn->remove("another");
    EXPECT_FALSE(val4.has_value());

    // 测试边界情况：删除与现有键有部分共同前缀但不完全匹配的键
    auto val5 = txn->remove("tes");
    EXPECT_FALSE(val5.has_value());

    auto val6 = txn->remove("testz");
    EXPECT_FALSE(val6.has_value());
}

// 测试复杂的树结构删除导致的多级节点合并
TEST_F(TransactionTest, ComplexMergeAfterDelete)
{
    // 构建一个有多级分支的树
    txn->insert("a", leaves[0]);
    txn->insert("ab", leaves[1]);
    txn->insert("abc", leaves[2]);
    txn->insert("abcd", leaves[3]);
    txn->insert("abce", leaves[4]);

    // 验证初始状态
    EXPECT_EQ(5, txn->root().size());

    // 依次删除叶子节点，然后检查中间节点是否被正确合并
    // 删除abcd，此时abc节点应该有一个指向e的边
    auto val1 = txn->remove("abcd");
    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(leaves[3], val1.value());
    EXPECT_EQ(4, txn->root().size());

    // 删除abce，此时abc节点应该没有任何边，但它自己是叶子节点
    auto val2 = txn->remove("abce");
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(leaves[4], val2.value());
    EXPECT_EQ(3, txn->root().size());

    // 删除abc，此时应触发ab节点的合并
    auto val3 = txn->remove("abc");
    EXPECT_TRUE(val3.has_value());
    EXPECT_EQ(leaves[2], val3.value());
    EXPECT_EQ(2, txn->root().size());

    // 删除ab，此时应触发根节点下的合并
    auto val4 = txn->remove("ab");
    EXPECT_TRUE(val4.has_value());
    EXPECT_EQ(leaves[1], val4.value());
    EXPECT_EQ(1, txn->root().size());

    // 删除最后一个节点
    auto val5 = txn->remove("a");
    EXPECT_TRUE(val5.has_value());
    EXPECT_EQ(leaves[0], val5.value());
    EXPECT_EQ(0, txn->root().size());
}

// 测试特定的nullptr返回情况（针对第442行）
TEST_F(TransactionTest, SpecificNullptrReturn)
{
    // 构造一个树，包含特定键值对
    txn->insert("romane", leaves[0]);
    txn->insert("romanus", leaves[1]);

    // 测试查找一个与现有键共享前缀、但在中间某处不匹配的键
    // 这种情况应该会命中transaction.h第442行的代码路径
    auto val = txn->get("romant");
    EXPECT_FALSE(val.has_value());

    // 测试删除一个与现有键共享前缀、但在中间某处不匹配的键
    auto del_val = txn->remove("romant");
    EXPECT_FALSE(del_val.has_value());

    // 测试查找一个比现有键更短但是其前缀的键
    auto val2 = txn->get("roma");
    EXPECT_FALSE(val2.has_value());

    // 再增加一个键，使树结构更复杂
    txn->insert("romans", leaves[2]);

    // 测试查找一个与多个现有键共享前缀的不存在键
    auto val3 = txn->get("romanz");
    EXPECT_FALSE(val3.has_value());
}

// 测试特定的节点合并场景（针对第473行的merge_child调用）
TEST_F(TransactionTest, SpecificMergeChildCall)
{
    // 构造一个特定的树结构，其中包含类似这样的结构：
    // root -> "te" -> "st" (值为leaves[0])
    //              -> "am" (值为leaves[1])
    txn->insert("test", leaves[0]);
    txn->insert("team", leaves[1]);

    // 确认初始状态
    EXPECT_EQ(2, txn->root().size());
    auto val1 = txn->get("test");
    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(leaves[0], val1.value());

    auto val2 = txn->get("team");
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(leaves[1], val2.value());

    // 删除"team"，此时"te"节点下只有一条边"st"，应该触发merge_child
    auto del_val = txn->remove("team");
    EXPECT_TRUE(del_val.has_value());
    EXPECT_EQ(leaves[1], del_val.value());

    // 验证"test"仍然可以访问
    auto val_after = txn->get("test");
    EXPECT_TRUE(val_after.has_value());
    EXPECT_EQ(leaves[0], val_after.value());

    // 验证大小
    EXPECT_EQ(1, txn->root().size());

    // 尝试更复杂的结构和操作
    txn = std::make_unique<txn_type>();
    txn->insert("abc", leaves[0]);
    txn->insert("abcd", leaves[1]);
    txn->insert("abcde", leaves[2]);

    // 删除中间节点，触发特定的合并情况
    txn->remove("abcd");

    // 验证结果
    auto val_abc = txn->get("abc");
    EXPECT_TRUE(val_abc.has_value());
    EXPECT_EQ(leaves[0], val_abc.value());

    auto val_abcde = txn->get("abcde");
    EXPECT_TRUE(val_abcde.has_value());
    EXPECT_EQ(leaves[2], val_abcde.value());

    EXPECT_EQ(2, txn->root().size());
}

// 测试事务回滚
TEST_F(TransactionTest, Rollback)
{
    // 插入数据
    txn->insert("a", leaves[0]);

    // 分配保存点
    int save_point_id = txn->save_point();

    // 添加更多数据
    txn->insert("b", leaves[1]);
    txn->insert("c", leaves[2]);

    // 验证当前状态
    EXPECT_EQ(3, txn->root().size());

    // 回滚到保存点
    txn->rollback(save_point_id);

    // 验证回滚后的状态
    EXPECT_EQ(1, txn->root().size());

    auto val_a = txn->get("a");
    EXPECT_TRUE(val_a.has_value());
    EXPECT_EQ(leaves[0], val_a.value());

    auto val_b = txn->get("b");
    EXPECT_FALSE(val_b.has_value());
    txn->commit();
}

// 测试完全回滚
TEST_F(TransactionTest, CompleteRollback)
{
    // 插入数据
    txn->insert("a", leaves[0]);
    txn->insert("b", leaves[1]);

    // 验证当前状态
    EXPECT_EQ(2, txn->root().size());

    // 完全回滚
    txn->rollback();

    // 提交并获取树
    tree_type tree = txn->commit();

    // 验证回滚后的状态
    EXPECT_EQ(0, tree.size());
}

// 测试共享前缀
TEST_F(TransactionTest, SharedPrefix)
{
    // 插入具有共享前缀的键
    txn->insert("test1", leaves[0]);
    txn->insert("test2", leaves[1]);
    txn->insert("test3", leaves[2]);

    // 获取并验证
    auto val1 = txn->get("test1");
    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(leaves[0], val1.value());

    auto val2 = txn->get("test2");
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(leaves[1], val2.value());

    auto val3 = txn->get("test3");
    EXPECT_TRUE(val3.has_value());
    EXPECT_EQ(leaves[2], val3.value());

    // 删除中间键
    auto del_val = txn->remove("test2");
    EXPECT_TRUE(del_val.has_value());
    EXPECT_EQ(leaves[1], del_val.value());

    // 验证其他键仍然存在
    auto val1_after = txn->get("test1");
    EXPECT_TRUE(val1_after.has_value());
    EXPECT_EQ(leaves[0], val1_after.value());

    auto val3_after = txn->get("test3");
    EXPECT_TRUE(val3_after.has_value());
    EXPECT_EQ(leaves[2], val3_after.value());
}

// 测试事务锁定和解锁
TEST_F(TransactionTest, LockAndUnlock)
{
    // 锁定数据库
    txn->lock_db();

    // 插入数据
    txn->insert("test", leaves[0]);

    // 解锁数据库
    txn->unlock_db();

    // 验证数据已成功插入
    auto val = txn->get("test");
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(leaves[0], val.value());
}

// 测试空事务
TEST_F(TransactionTest, EmptyTransaction)
{
    // 不进行任何操作，直接提交空事务
    tree_type tree = txn->commit();

    // 验证状态
    EXPECT_EQ(0, tree.size());
}

// 测试事务的清空操作
TEST_F(TransactionTest, ClearTransaction)
{
    // 插入数据
    txn->insert("a", leaves[0]);
    txn->insert("b", leaves[1]);

    // 验证当前状态
    EXPECT_EQ(2, txn->root().size());

    // 清空事务
    txn = std::make_unique<txn_type>();

    // 验证清空后的状态
    EXPECT_EQ(0, txn->root().size());

    // 插入新数据
    txn->insert("c", leaves[2]);

    // 验证新状态
    EXPECT_EQ(1, txn->root().size());

    auto val = txn->get("c");
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(leaves[2], val.value());
}

// 测试多个保存点
TEST_F(TransactionTest, MultipleSavePoints)
{
    int save_point_id = txn->save_point();

    // 插入初始数据
    txn->insert("a", leaves[0]);

    // 创建第一个保存点
    int save_point_id1 = txn->save_point();

    // 添加更多数据
    txn->insert("b", leaves[1]);

    // 创建第二个保存点
    int save_point_id2 = txn->save_point();

    // 添加更多数据
    txn->insert("c", leaves[2]);

    // 验证当前状态
    EXPECT_EQ(3, txn->root().size());

    // 回滚到第二个保存点
    txn->rollback(save_point_id2);

    // 验证状态
    EXPECT_EQ(2, txn->root().size());

    auto val_c = txn->get("c");
    EXPECT_FALSE(val_c.has_value());

    // 回滚到第一个保存点
    txn->rollback(save_point_id1);

    // 验证状态
    EXPECT_EQ(1, txn->root().size());

    auto val_b = txn->get("b");
    EXPECT_FALSE(val_b.has_value());

    auto val_a = txn->get("a");
    EXPECT_TRUE(val_a.has_value());
    EXPECT_EQ(leaves[0], val_a.value());

    // 回滚到初始保存点
    txn->rollback(save_point_id);

    auto val_a1 = txn->get("a");
    EXPECT_FALSE(val_a1.has_value());

    auto val_b1 = txn->get("b");
    EXPECT_FALSE(val_b1.has_value());
}

// 测试保存点 commit() 方法
TEST_F(TransactionTest, TransactionSavePointCommit)
{
    // 插入数据
    txn->insert("a", leaves[0]);
    txn->insert("b", leaves[1]);

    // 创建保存点
    int save_point_id = txn->save_point();

    // 继续修改
    txn->insert("c", leaves[2]);

    // 提交事务，这会调用所有保存点的 commit() 方法
    tree_type tree = txn->commit();

    // 验证所有数据都被提交
    EXPECT_EQ(3, tree.size());
    auto val_a = tree.get("a");
    EXPECT_TRUE(val_a.has_value());
    EXPECT_EQ(leaves[0], val_a.value());

    auto val_b = tree.get("b");
    EXPECT_TRUE(val_b.has_value());
    EXPECT_EQ(leaves[1], val_b.value());

    auto val_c = tree.get("c");
    EXPECT_TRUE(val_c.has_value());
    EXPECT_EQ(leaves[2], val_c.value());
}

// 测试回滚到指定保存点
TEST_F(TransactionTest, TransactionRollbackToSavePoint)
{
    // 插入初始数据
    txn->insert("a", leaves[0]);

    // 创建第一个保存点
    int save_point_id1 = txn->save_point();

    // 添加更多数据
    txn->insert("b", leaves[1]);

    // 创建第二个保存点
    int save_point_id2 = txn->save_point();

    // 添加更多数据
    txn->insert("c", leaves[2]);

    // 验证当前状态
    EXPECT_EQ(3, txn->root().size());

    // 回滚到第二个保存点
    txn->rollback(save_point_id2);

    // 验证状态
    EXPECT_EQ(2, txn->root().size());
    auto val_a = txn->get("a");
    EXPECT_TRUE(val_a.has_value());
    EXPECT_EQ(leaves[0], val_a.value());

    auto val_b = txn->get("b");
    EXPECT_TRUE(val_b.has_value());
    EXPECT_EQ(leaves[1], val_b.value());

    auto val_c = txn->get("c");
    EXPECT_FALSE(val_c.has_value());

    // 回滚到第一个保存点
    txn->rollback(save_point_id1);

    // 验证状态
    EXPECT_EQ(1, txn->root().size());
    val_a = txn->get("a");
    EXPECT_TRUE(val_a.has_value());
    EXPECT_EQ(leaves[0], val_a.value());

    val_b = txn->get("b");
    EXPECT_FALSE(val_b.has_value());
}

// 测试删除后插入导致节点合并
TEST_F(TransactionTest, TransactionInsertAfterDeleteCausesMerge)
{
    // 插入 "abc" 和 "abd"
    txn->insert("abc", leaves[0]);
    txn->insert("abd", leaves[1]);

    // 验证插入成功
    EXPECT_EQ(2, txn->root().size());

    // 删除 "abc"
    auto deleted = txn->remove("abc");
    EXPECT_TRUE(deleted.has_value());
    EXPECT_EQ(leaves[0], deleted.value());

    // 验证删除后状态
    EXPECT_EQ(1, txn->root().size());

    // 插入 "ab"，这应该导致节点合并
    txn->insert("ab", leaves[2]);

    // 验证插入成功
    EXPECT_EQ(2, txn->root().size());

    auto val_ab = txn->get("ab");
    EXPECT_TRUE(val_ab.has_value());
    EXPECT_EQ(leaves[2], val_ab.value());

    auto val_abd = txn->get("abd");
    EXPECT_TRUE(val_abd.has_value());
    EXPECT_EQ(leaves[1], val_abd.value());

    // 提交事务
    tree_type tree = txn->commit();
    EXPECT_EQ(2, tree.size());
}

// 测试更新后删除导致节点合并
TEST_F(TransactionTest, TransactionUpdateCausesMerge)
{
    // 插入 "abc" 和 "abd"
    txn->insert("abc", leaves[0]);
    txn->insert("abd", leaves[1]);

    // 验证插入成功
    EXPECT_EQ(2, txn->root().size());

    // 更新 "abc" 的值
    auto [old_val, updated] = txn->insert("abc", leaves[2]);
    EXPECT_TRUE(updated);
    EXPECT_EQ(leaves[0], old_val.value());

    // 验证更新后状态
    EXPECT_EQ(2, txn->root().size());

    // 删除 "abd"，这应该导致节点合并
    auto deleted = txn->remove("abd");
    EXPECT_TRUE(deleted.has_value());
    EXPECT_EQ(leaves[1], deleted.value());

    // 验证删除后状态
    EXPECT_EQ(1, txn->root().size());

    auto val_abc = txn->get("abc");
    EXPECT_TRUE(val_abc.has_value());
    EXPECT_EQ(leaves[2], val_abc.value());

    // 提交事务
    tree_type tree = txn->commit();
    EXPECT_EQ(1, tree.size());
}

} // namespace mc::im::tests