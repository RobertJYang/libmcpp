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

#ifndef MC_IM_RAW_ITERATOR_H
#define MC_IM_RAW_ITERATOR_H

#include <mc/im/node.h>
#include <mc/im/iradix.h>
#include <vector>
#include <memory>
#include <tuple>
#include <functional>

namespace mc::im {

/**
 * 原始迭代器，用于遍历不可变基数树
 */
class raw_iterator {
public:
    /**
     * 创建新的迭代器
     */
    raw_iterator();

    /**
     * 初始化迭代器
     * @param tree 要遍历的树
     */
    void init(const iradix_tree* tree);

    /**
     * 移动到下一个节点
     * @return 如果成功移动到下一个节点则返回true
     */
    bool next();

    /**
     * 获取当前节点的键
     * @return 当前节点的键
     */
    const std::vector<uint8_t>& key() const;

    /**
     * 获取当前节点的值
     * @return 当前节点的值
     */
    void* leaf() const;

    /**
     * 查找指定前缀的节点
     * @param prefix 要查找的前缀
     * @return 如果找到则返回true
     */
    bool seek_prefix(const std::vector<uint8_t>& prefix);

    // 在 iradix.cpp 中需要访问的成员
    std::function<bool(uint8_t, uint8_t)> m_cmp_fn;

private:
    struct t_stack {
        std::shared_ptr<node> n;
        std::vector<uint8_t> key;
        size_t i;
    };

    std::vector<t_stack> m_stack;
    const iradix_tree* m_tree;

    /**
     * 创建新的栈
     * @param n 节点
     * @param key 键
     */
    void new_stack(std::shared_ptr<node> n, const std::vector<uint8_t>& key);

    /**
     * 弹出栈顶元素
     */
    void pop_stack();

    /**
     * 移动到下一个节点
     * @return 如果成功移动到下一个节点则返回true
     */
    bool next_node();
};

} // namespace mc::im

#endif // MC_IM_RAW_ITERATOR_H 