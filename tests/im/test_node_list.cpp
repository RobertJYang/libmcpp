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
#include <list>
#include <mc/im/radix_tree/node.h>
#include <vector>

namespace mc::im::tests {

static int allocation_count   = 0;
static int deallocation_count = 0;

// 定义一个简单的自定义分配器用于测试
template <typename T>
class test_allocator : public std::allocator<T> {
public:
    using base      = std::allocator<T>;
    using pointer   = typename std::allocator_traits<base>::pointer;
    using size_type = typename std::allocator_traits<base>::size_type;

    template <typename U>
    struct rebind {
        using other = test_allocator<U>;
    };

    test_allocator() {
    }

    template <typename U>
    test_allocator(const test_allocator<U>& other) : base(other) {
    }

    pointer allocate(size_type n) {
        ++allocation_count;
        return base::allocate(n);
    }

    void deallocate(pointer p, size_type n) {
        ++deallocation_count;
        base::deallocate(p, n);
    }

    int get_allocation_count() const {
        return allocation_count;
    }
    int get_deallocation_count() const {
        return deallocation_count;
    }
};

// 定义默认配置
using default_config = tree_config<>;
using node_type      = node<default_config>;
using node_list_type = node_list<node_type>;
using shared_ptr     = typename node_type::ref_ptr_type;

// 测试夹具
class NodeListTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建一些测试用的节点
        for (int i = 0; i < 5; ++i) {
            nodes.push_back(make_shared<node_type>(reinterpret_cast<void*>(static_cast<intptr_t>(i))));
        }
    }

    void TearDown() override {
        nodes.clear();
    }

    // 辅助函数，检查列表是否为空
    bool is_empty(const node_list_type& list) {
        return list.len() == 0 && list.front() == nullptr && list.back() == nullptr;
    }

    // 辅助函数，验证列表中的节点顺序
    bool verify_order(const node_list_type& list, const std::vector<shared_ptr>& expected) {
        if (list.len() != expected.size()) {
            return false;
        }

        if (expected.empty()) {
            return is_empty(list);
        }

        shared_ptr current = list.front();
        for (size_t i = 0; i < expected.size(); ++i) {
            if (current != expected[i]) {
                return false;
            }
            current = current->m_next;
        }

        // 检查是否形成循环
        return list.front()->m_prev == list.back() && list.back()->m_next == list.front();
    }

    // 测试节点
    std::vector<shared_ptr> nodes;
};

// 测试空列表
TEST_F(NodeListTest, EmptyList) {
    node_list_type list;
    EXPECT_TRUE(is_empty(list));
    EXPECT_EQ(list.len(), 0);
    EXPECT_EQ(list.front(), nullptr);
    EXPECT_EQ(list.back(), nullptr);
}

// 测试单节点列表
TEST_F(NodeListTest, SingleNodeList) {
    node_list_type list;
    list.push_back(nodes[0]);

    EXPECT_EQ(list.len(), 1);
    EXPECT_EQ(list.front(), nodes[0]);
    EXPECT_EQ(list.back(), nodes[0]);
    EXPECT_EQ(nodes[0]->m_next, nodes[0]);
    EXPECT_EQ(nodes[0]->m_prev, nodes[0]);
}

// 测试多节点列表
TEST_F(NodeListTest, MultiNodeList) {
    node_list_type list;

    // 依次添加节点
    for (auto& node : nodes) {
        list.push_back(node);
    }

    EXPECT_EQ(list.len(), nodes.size());
    EXPECT_EQ(list.front(), nodes[0]);
    EXPECT_EQ(list.back(), nodes[4]);

    // 验证节点顺序和链接
    EXPECT_TRUE(verify_order(list, nodes));
}

// 测试push_front方法
TEST_F(NodeListTest, PushFront) {
    node_list_type list;

    // 从前端添加节点
    for (int i = nodes.size() - 1; i >= 0; --i) {
        list.push_front(nodes[i]);
    }

    // 验证顺序应该和原始顺序相反
    std::vector<shared_ptr> expected_order = nodes;
    std::reverse(expected_order.begin(), expected_order.end());

    EXPECT_EQ(list.len(), nodes.size());
    EXPECT_EQ(list.front(), nodes[0]);
    EXPECT_EQ(list.back(), nodes[4]);
    EXPECT_TRUE(verify_order(list, nodes));
}

// 测试insert方法
TEST_F(NodeListTest, Insert) {
    node_list_type list;

    // 测试在空列表中插入
    list.insert(nodes[0], nullptr);
    EXPECT_EQ(list.len(), 1);
    EXPECT_EQ(list.front(), nodes[0]);

    // 测试在节点后插入
    list.insert(nodes[1], nodes[0]);
    list.insert(nodes[3], nodes[1]);
    list.insert(nodes[2], nodes[1]); // 在nodes[1]后插入nodes[2]

    // 预期顺序：0, 1, 2, 3
    std::vector<shared_ptr> expected = {nodes[0], nodes[1], nodes[2], nodes[3]};
    EXPECT_TRUE(verify_order(list, expected));

    // 测试在列表末尾插入（at为nullptr）
    list.insert(nodes[4], nullptr);
    expected.push_back(nodes[4]);
    EXPECT_TRUE(verify_order(list, expected));
}

// 测试remove方法
TEST_F(NodeListTest, Remove) {
    node_list_type list;

    // 添加所有节点
    for (auto& node : nodes) {
        list.push_back(node);
    }

    // 移除中间节点
    list.remove(nodes[2]);

    // 预期顺序：0, 1, 3, 4
    std::vector<shared_ptr> expected = {nodes[0], nodes[1], nodes[3], nodes[4]};
    EXPECT_TRUE(verify_order(list, expected));
    EXPECT_EQ(list.len(), 4);

    // 移除头节点
    list.remove(nodes[0]);

    // 预期顺序：1, 3, 4
    expected = {nodes[1], nodes[3], nodes[4]};
    EXPECT_TRUE(verify_order(list, expected));
    EXPECT_EQ(list.len(), 3);

    // 移除尾节点
    list.remove(nodes[4]);

    // 预期顺序：1, 3
    expected = {nodes[1], nodes[3]};
    EXPECT_TRUE(verify_order(list, expected));
    EXPECT_EQ(list.len(), 2);

    // 移除所有节点
    list.remove(nodes[1]);
    list.remove(nodes[3]);

    EXPECT_TRUE(is_empty(list));
}

// 测试插入节点到指定位置前
TEST_F(NodeListTest, InsertBefore) {
    node_list_type list;

    // 在空列表中插入
    list.insert_before(nodes[2], nodes[2]);
    EXPECT_EQ(list.len(), 1);
    EXPECT_EQ(list.front(), nodes[2]);

    // 在头部前插入
    list.insert_before(nodes[1], nodes[2]);
    list.insert_before(nodes[0], nodes[1]);

    // 在尾部前插入
    list.push_back(nodes[4]);
    list.insert_before(nodes[3], nodes[4]);

    // 预期顺序：0, 1, 2, 3, 4
    EXPECT_TRUE(verify_order(list, nodes));
}

// 测试移动节点到前端
TEST_F(NodeListTest, MoveToFront) {
    node_list_type list;

    // 添加所有节点
    for (auto& node : nodes) {
        list.push_back(node);
    }

    // 移动中间节点到前端
    list.move_to_front(nodes[2]);

    // 预期顺序：2, 0, 1, 3, 4
    std::vector<shared_ptr> expected = {nodes[2], nodes[0], nodes[1], nodes[3], nodes[4]};
    EXPECT_TRUE(verify_order(list, expected));

    // 移动尾节点到前端
    list.move_to_front(nodes[4]);

    // 预期顺序：4, 2, 0, 1, 3
    expected = {nodes[4], nodes[2], nodes[0], nodes[1], nodes[3]};
    EXPECT_TRUE(verify_order(list, expected));

    // 移动已经在前端的节点
    list.move_to_front(nodes[4]);

    // 顺序应保持不变
    EXPECT_TRUE(verify_order(list, expected));
}

// 测试移动节点到后端
TEST_F(NodeListTest, MoveToBack) {
    node_list_type list;

    // 添加所有节点
    for (auto& node : nodes) {
        list.push_back(node);
    }

    // 移动头节点到后端
    list.move_to_back(nodes[0]);

    // 预期顺序：1, 2, 3, 4, 0
    std::vector<shared_ptr> expected = {nodes[1], nodes[2], nodes[3], nodes[4], nodes[0]};
    EXPECT_TRUE(verify_order(list, expected));

    // 移动中间节点到后端
    list.move_to_back(nodes[2]);

    // 预期顺序：1, 3, 4, 0, 2
    expected = {nodes[1], nodes[3], nodes[4], nodes[0], nodes[2]};
    EXPECT_TRUE(verify_order(list, expected));

    // 移动已经在后端的节点
    list.move_to_back(nodes[2]);

    // 顺序应保持不变
    EXPECT_TRUE(verify_order(list, expected));
}

// 测试移动节点到指定位置前
TEST_F(NodeListTest, MoveBefore) {
    node_list_type list;

    // 添加所有节点
    for (auto& node : nodes) {
        list.push_back(node);
    }

    // 移动节点到另一个节点之前
    list.move_before(nodes[3], nodes[1]);

    // 预期顺序：0, 3, 1, 2, 4
    std::vector<shared_ptr> expected = {nodes[0], nodes[3], nodes[1], nodes[2], nodes[4]};
    EXPECT_TRUE(verify_order(list, expected));

    // 移动头节点到中间位置
    list.move_before(nodes[0], nodes[2]);

    // 预期顺序：3, 1, 0, 2, 4
    expected = {nodes[3], nodes[1], nodes[0], nodes[2], nodes[4]};
    EXPECT_TRUE(verify_order(list, expected));

    // 移动节点到自己之前（应无效）
    list.move_before(nodes[2], nodes[2]);

    // 顺序应保持不变
    EXPECT_TRUE(verify_order(list, expected));
}

// 测试移动节点到指定位置后
TEST_F(NodeListTest, MoveAfter) {
    node_list_type list;

    // 添加所有节点
    for (auto& node : nodes) {
        list.push_back(node);
    }

    // 移动节点到另一个节点之后
    list.move_after(nodes[3], nodes[0]);

    // 预期顺序：0, 3, 1, 2, 4
    std::vector<shared_ptr> expected = {nodes[0], nodes[3], nodes[1], nodes[2], nodes[4]};
    EXPECT_TRUE(verify_order(list, expected));

    // 移动尾节点到中间位置
    list.move_after(nodes[4], nodes[3]);

    // 预期顺序：0, 3, 4, 1, 2
    expected = {nodes[0], nodes[3], nodes[4], nodes[1], nodes[2]};
    EXPECT_TRUE(verify_order(list, expected));

    // 移动节点到自己之后（应无效）
    list.move_after(nodes[1], nodes[1]);

    // 顺序应保持不变
    EXPECT_TRUE(verify_order(list, expected));
}

// 测试清空列表
TEST_F(NodeListTest, Init) {
    node_list_type list;

    // 添加所有节点
    for (auto& node : nodes) {
        list.push_back(node);
    }

    EXPECT_EQ(list.len(), nodes.size());

    // 清空列表
    list.clear();

    EXPECT_TRUE(is_empty(list));

    // 确保节点被正确断开链接
    for (auto& node : nodes) {
        EXPECT_EQ(node->m_next, nullptr);
        EXPECT_EQ(node->m_prev, nullptr);
    }
}

// 测试自定义分配器
TEST_F(NodeListTest, CustomAllocator) {
    test_allocator<char> alloc;

    // 定义使用自定义分配器的配置
    using custom_config    = tree_config<void*, test_allocator<char>>;
    using custom_node_type = node<custom_config>;

    {
        auto node0 =
            mc::allocate_shared<custom_node_type>(alloc, reinterpret_cast<void*>(static_cast<intptr_t>(0)));
        auto node1 =
            mc::allocate_shared<custom_node_type>(alloc, reinterpret_cast<void*>(static_cast<intptr_t>(1)));

        typename custom_node_type::list_type list;

        // 测试基本操作
        list.push_back(node0);
        list.push_back(node1);

        EXPECT_EQ(list.len(), 2);
        EXPECT_EQ(list.front(), node0);
        EXPECT_EQ(list.back(), node1);
    }

    // 获取分配器
    EXPECT_EQ(2, alloc.get_allocation_count());
    EXPECT_EQ(2, alloc.get_deallocation_count());
}

// 测试循环性质
TEST_F(NodeListTest, CircularNature) {
    node_list_type list;

    // 添加所有节点
    for (auto& node : nodes) {
        list.push_back(node);
    }

    // 验证循环性
    EXPECT_EQ(list.back()->m_next, list.front());
    EXPECT_EQ(list.front()->m_prev, list.back());

    // 从头开始遍历一圈
    shared_ptr current = list.front();
    size_t     count   = 0;
    do {
        current = current->m_next;
        ++count;
    } while (current != list.front() && count < 10);

    EXPECT_EQ(count, nodes.size());
}

} // namespace mc::im::tests