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

#ifndef MC_IM_NODE_LIST_H
#define MC_IM_NODE_LIST_H

#include <mc/memory.h>
#include <memory>

namespace mc::im {

/**
 * 引用列表，用于管理引用对象
 * 实现为循环双向链表，当列表非空时，第一个节点的 prev 和最后一个节点的 next 互相连接
 */
template <typename T, typename PointerType = T*>
class ref_list {
public:
    using ref_ptr_type   = shared_ptr<T, default_deleter<T>, PointerType>;
    using allocator_type = typename T::allocator_type;

    ref_list();

    ref_list(ref_list&& other) noexcept {
        m_root      = std::move(other.m_root);
        m_len       = other.m_len;
        other.m_len = 0;
    }

    ref_list& operator=(ref_list&& other) noexcept {
        if (this != &other) {
            m_root      = std::move(other.m_root);
            m_len       = other.m_len;
            other.m_len = 0;
        }
        return *this;
    }
    /**
     * 销毁节点列表
     */
    ~ref_list();

    /**
     * 清空节点列表
     */
    void clear();

    /**
     * 获取节点数量
     * @return 节点数量
     */
    size_t len() const;

    /**
     * 获取首节点
     * @return 首节点
     */
    ref_ptr_type front() const;

    /**
     * 获取尾节点
     * @return 尾节点
     */
    ref_ptr_type back() const;

    /**
     * 在指定位置之后插入节点
     * @param e 要插入的节点
     * @param at 插入位置
     * @return 插入的节点
     */
    ref_ptr_type insert(ref_ptr_type e, ref_ptr_type at);

    /**
     * 删除节点
     * @param e 要删除的节点
     * @return 删除的节点
     */
    ref_ptr_type remove(ref_ptr_type e);

    /**
     * 在列表前端添加节点
     * @param e 要添加的节点
     * @return 添加的节点
     */
    ref_ptr_type push_front(ref_ptr_type e);

    /**
     * 在列表后端添加节点
     * @param e 要添加的节点
     * @return 添加的节点
     */
    ref_ptr_type push_back(ref_ptr_type e);

    /**
     * 在指定节点之前插入节点
     * @param v 要插入的节点
     * @param mark 标记节点
     * @return 插入的节点
     */
    ref_ptr_type insert_before(ref_ptr_type v, ref_ptr_type mark);

    /**
     * 在指定节点之后插入节点
     * @param v 要插入的节点
     * @param mark 标记节点
     * @return 插入的节点
     */
    ref_ptr_type insert_after(ref_ptr_type v, ref_ptr_type mark);

    /**
     * 将节点移动到列表前端
     * @param e 要移动的节点
     */
    void move_to_front(ref_ptr_type e);

    /**
     * 将节点移动到列表后端
     * @param e 要移动的节点
     */
    void move_to_back(ref_ptr_type e);

    /**
     * 将节点移动到指定节点之前
     * @param e 要移动的节点
     * @param mark 标记节点
     */
    void move_before(ref_ptr_type e, ref_ptr_type mark);

    /**
     * 将节点移动到指定节点之后
     * @param e 要移动的节点
     * @param mark 标记节点
     */
    void move_after(ref_ptr_type e, ref_ptr_type mark);

private:
    /**
     * 初始化空列表并添加单个节点
     * @param e 要添加的节点
     * @return 添加的节点
     */
    ref_ptr_type init_single_node(ref_ptr_type e);

    /**
     * 预处理节点并准备添加到列表
     * 如果节点已经在链表中，会先断开连接
     * @param node 要处理的节点
     * @return 如果节点为空或列表为空并已初始化返回true，否则返回false
     */
    bool prepare_node_for_insertion(ref_ptr_type& node);

    /**
     * 将节点插入到另一个节点之后
     * @param element 要插入的节点
     * @param pos 插入位置
     */
    static void link_after(ref_ptr_type element, ref_ptr_type pos) {
        if (!element || !pos) {
            return;
        }

        element->m_next     = pos->m_next;
        element->m_prev     = pos;
        pos->m_next->m_prev = element;
        pos->m_next         = element;
    }

    static bool is_linked(const ref_ptr_type& element) {
        return element->m_next != nullptr && element->m_prev != nullptr;
    }

    static void unlink(const ref_ptr_type& element) {
        if (is_linked(element)) {
            element->m_prev->m_next = element->m_next;
            element->m_next->m_prev = element->m_prev;
            element->m_next         = nullptr;
            element->m_prev         = nullptr;
        }
    }

    /**
     * 将节点插入到另一个节点之前
     * @param element 要插入的节点
     * @param pos 插入位置
     */
    static void link_before(ref_ptr_type element, ref_ptr_type pos) {
        if (!element || !pos) {
            return;
        }

        element->m_next     = pos;
        element->m_prev     = pos->m_prev;
        pos->m_prev->m_next = element;
        pos->m_prev         = element;
    }

    ref_ptr_type m_root; // 列表头节点，为 nullptr 时表示空列表
    size_t       m_len;
};

template <typename T, typename PointerType>
ref_list<T, PointerType>::ref_list() : m_root(nullptr), m_len(0) {
}

template <typename T, typename PointerType>
ref_list<T, PointerType>::~ref_list() {
    clear();
}

template <typename T, typename PointerType>
void ref_list<T, PointerType>::clear() {
    while (m_root) {
        remove(m_root);
    }
}

template <typename T, typename PointerType>
size_t ref_list<T, PointerType>::len() const {
    return m_len;
}

template <typename T, typename PointerType>
typename ref_list<T, PointerType>::ref_ptr_type ref_list<T, PointerType>::front() const {
    return m_root;
}

template <typename T, typename PointerType>
typename ref_list<T, PointerType>::ref_ptr_type ref_list<T, PointerType>::back() const {
    if (!m_root) {
        return nullptr;
    }
    return m_root->m_prev; // 在循环链表中，头节点的前一个就是尾节点
}

template <typename T, typename PointerType>
typename ref_list<T, PointerType>::ref_ptr_type
ref_list<T, PointerType>::init_single_node(ref_ptr_type e) {
    if (!e) {
        return nullptr;
    }

    m_root    = e;
    e->m_next = e;
    e->m_prev = e;
    m_len     = 1;
    return e;
}

template <typename T, typename PointerType>
bool ref_list<T, PointerType>::prepare_node_for_insertion(ref_ptr_type& node) {
    if (!node) {
        return true;
    }

    // 如果节点已经在链表中，先断开连接
    if (is_linked(node)) {
        // 如果是头节点，需要更新头节点
        if (node == m_root) {
            m_root = node->m_next;
        }

        unlink(node);
        m_len--; // 会在后面增加回来
    }

    // 如果列表为空，初始化列表
    if (!m_root) {
        init_single_node(node);
        return true;
    }

    return false;
}

template <typename T, typename PointerType>
typename ref_list<T, PointerType>::ref_ptr_type ref_list<T, PointerType>::insert(ref_ptr_type e,
                                                                                 ref_ptr_type at) {
    if (prepare_node_for_insertion(e)) {
        return e;
    }

    // 如果 at 为空，添加到列表末尾
    if (!at) {
        at = m_root->m_prev; // 列表尾
    }

    // 在 at 后面插入 e
    link_after(e, at);

    m_len++;
    return e;
}

template <typename T, typename PointerType>
typename ref_list<T, PointerType>::ref_ptr_type ref_list<T, PointerType>::remove(ref_ptr_type e) {
    if (!e || !m_root) {
        return nullptr;
    }

    // 如果只有一个节点
    if (m_len == 1 && e == m_root) {
        m_root->m_next = nullptr;
        m_root->m_prev = nullptr;
        m_root         = nullptr;
        m_len          = 0;
        return e;
    }

    // 如果是头节点
    if (e == m_root) {
        m_root = e->m_next;
        unlink(e);
    } else {
        unlink(e);
    }

    m_len--;
    return e;
}

template <typename T, typename PointerType>
typename ref_list<T, PointerType>::ref_ptr_type
ref_list<T, PointerType>::push_front(ref_ptr_type e) {
    if (prepare_node_for_insertion(e)) {
        return e;
    }

    // 在头节点之前插入
    link_before(e, m_root);
    m_root = e; // 更新头节点

    m_len++;
    return e;
}

template <typename T, typename PointerType>
typename ref_list<T, PointerType>::ref_ptr_type
ref_list<T, PointerType>::push_back(ref_ptr_type e) {
    if (prepare_node_for_insertion(e)) {
        return e;
    }

    // 在尾部插入
    link_after(e, m_root->m_prev);

    m_len++;
    return e;
}

template <typename T, typename PointerType>
typename ref_list<T, PointerType>::ref_ptr_type
ref_list<T, PointerType>::insert_before(ref_ptr_type v, ref_ptr_type mark) {
    if (!v || !mark) {
        return nullptr;
    }

    if (prepare_node_for_insertion(v)) {
        return v;
    }

    // 在 mark 之前插入
    link_before(v, mark);

    // 如果mark是头节点，更新头节点为v
    if (mark == m_root) {
        m_root = v;
    }

    m_len++;
    return v;
}

template <typename T, typename PointerType>
typename ref_list<T, PointerType>::ref_ptr_type
ref_list<T, PointerType>::insert_after(ref_ptr_type v, ref_ptr_type mark) {
    if (!v || !mark) {
        return nullptr;
    }

    return insert(v, mark);
}

template <typename T, typename PointerType>
void ref_list<T, PointerType>::move_to_front(ref_ptr_type e) {
    if (!e || !m_root || m_len <= 1) {
        return;
    }

    if (e == m_root) {
        return; // 已经在头部
    }

    // 从当前位置移除并插入到头部
    push_front(e);
}

template <typename T, typename PointerType>
void ref_list<T, PointerType>::move_to_back(ref_ptr_type e) {
    if (!e || !m_root || m_len <= 1) {
        return;
    }

    if (e == m_root->m_prev) {
        return; // 已经在尾部
    }

    // 从当前位置移除并插入到尾部
    push_back(e);
}

template <typename T, typename PointerType>
void ref_list<T, PointerType>::move_before(ref_ptr_type e, ref_ptr_type mark) {
    if (!e || !mark || !m_root || m_len <= 1 || e == mark) {
        return;
    }

    insert_before(e, mark);
}

template <typename T, typename PointerType>
void ref_list<T, PointerType>::move_after(ref_ptr_type e, ref_ptr_type mark) {
    if (!e || !mark || !m_root || m_len <= 1 || e == mark) {
        return;
    }

    insert_after(e, mark);
}

} // namespace mc::im

#endif // MC_IM_NODE_LIST_H