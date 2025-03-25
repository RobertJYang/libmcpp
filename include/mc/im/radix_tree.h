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

#ifndef MC_IM_RADIX_TREE_H
#define MC_IM_RADIX_TREE_H

#include <cstdint>
#include <functional>
#include <mc/im/node.h>
#include <memory>

namespace mc::im {

// 前向声明
template <typename Allocator, bool IsLess>
class transaction;

/**
 * 不可变基数树
 * 该树结构确保不会修改已有的数据结构，所有修改操作都会创建新节点
 * 设计为可在共享内存中使用
 */
template <typename Allocator = std::allocator<void>, bool IsLess = true>
class radix_tree {
public:
    using node_type = node<Allocator, IsLess>;
    using node_ptr  = typename node_type::ref_ptr_type;

    /**
     * 创建一个空的不可变基数树
     */
    radix_tree(const Allocator& alloc = Allocator())
        : m_root(allocate_ref<node_type>(alloc, nullptr)), m_size(0), m_allocator(alloc) {
    }

    /**
     * 使用现有节点创建不可变基数树
     * @param root 根节点
     * @param size 树的大小
     */
    radix_tree(node_ptr root, size_t size);

    ~radix_tree() {
        m_root.reset();
    }

    /**
     * 返回树中元素的数量
     * @return 元素数量
     */
    size_t size() const;

    /**
     * 获取根节点
     * @return 根节点的指针
     */
    const node_type* root() const;

    /**
     * 查找键对应的值
     * @param key 查找的键
     * @return 值和是否找到的标志
     */
    std::pair<leaf_type, bool> get(const key_view& key) const;

    Allocator get_allocator() const {
        return m_allocator;
    }

    /**
     * 计算两个字节数组的最长公共前缀长度
     */
    static size_t longest_prefix(const key_view& k1, const key_view& k2);

    // 允许事务类访问私有成员
    friend class transaction<Allocator, IsLess>;

private:
    node_ptr  m_root;
    size_t    m_size = 0;
    Allocator m_allocator;
};

// 模板函数实现

template <typename Allocator, bool IsLess>
radix_tree<Allocator, IsLess>::radix_tree(node_ptr root, size_t size)
    : m_root(std::move(root)), m_size(size) {
}

template <typename Allocator, bool IsLess>
size_t radix_tree<Allocator, IsLess>::size() const {
    return m_size;
}

template <typename Allocator, bool IsLess>
const typename radix_tree<Allocator, IsLess>::node_type*
radix_tree<Allocator, IsLess>::root() const {
    return m_root.get();
}

template <typename Allocator, bool IsLess>
std::pair<leaf_type, bool> radix_tree<Allocator, IsLess>::get(const key_view& key) const {
    if (!m_root) {
        return {nullptr, false};
    }
    return m_root->get(key);
}

template <typename Allocator, bool IsLess>
size_t radix_tree<Allocator, IsLess>::longest_prefix(const key_view& k1, const key_view& k2) {
    size_t max_len = std::min(k1.size(), k2.size());
    size_t i       = 0;
    for (; i < max_len; i++) {
        if (k1[i] != k2[i]) {
            break;
        }
    }
    return i;
}

} // namespace mc::im

#endif // MC_IM_RADIX_TREE_H