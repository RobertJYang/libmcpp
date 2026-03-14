/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/**
 * @file op_queue.h
 * @brief 操作队列
 */

#ifndef MC_RUNTIME_OP_QUEUE_H
#define MC_RUNTIME_OP_QUEUE_H

#include <cstddef>

namespace mc::runtime {

template <typename Operation>
class op_queue {
public:
    using operation_type = Operation;

    op_queue() : m_front(nullptr), m_back(nullptr)
    {}

    ~op_queue()
    {
        // 清空队列,不删除操作对象，由调用者负责
        clear();
    }

    Operation* front()
    {
        return m_front;
    }

    const Operation* front() const
    {
        return m_front;
    }

    Operation* back()
    {
        return m_back;
    }

    const Operation* back() const
    {
        return m_back;
    }

    void pop_front()
    {
        if (m_front) {
            m_front = static_cast<Operation*>(m_front->next);
            if (!m_front) {
                m_back = nullptr;
            }
        }
    }

    // 将操作添加到队列头部
    void push_front(Operation& op)
    {
        if (!m_front) {
            m_front = &op;
            m_back  = &op;
        } else {
            op.next = m_front;
            m_front = &op;
        }
    }

    void push_back(Operation& op)
    {
        if (!m_back) {
            m_front = &op;
            m_back  = &op;
        } else {
            m_back->next = &op;
            m_back       = &op;
        }
    }

    bool empty() const
    {
        return m_front == nullptr;
    }

    void clear()
    {
        m_front = nullptr;
        m_back  = nullptr;
    }

    // 将另一个队列的所有元素追加到当前队列的头部
    void splice_front(op_queue& other)
    {
        if (other.empty()) {
            return;
        }

        if (m_front) {
            other.m_back->next = m_front;
            m_front            = other.m_front;
        } else {
            m_front = other.m_front;
            m_back  = other.m_back;
        }

        // 清空 other
        other.m_front = nullptr;
        other.m_back  = nullptr;
    }

    // 将另一个队列的所有元素追加到当前队列的尾部
    void splice_back(op_queue& other)
    {
        if (other.empty()) {
            return;
        }

        if (m_back) {
            m_back->next = other.m_front;
            m_back       = other.m_back;
        } else {
            m_front = other.m_front;
            m_back  = other.m_back;
        }

        // 清空 other
        other.m_front = nullptr;
        other.m_back  = nullptr;
    }

    size_t size() const
    {
        size_t count = 0;
        for (Operation* op = m_front; op != nullptr; op = static_cast<Operation*>(op->next)) {
            ++count;
        }
        return count;
    }

private:
    Operation* m_front; // 链表头部
    Operation* m_back;  // 链表尾部
};

} // namespace mc::runtime

#endif // MC_RUNTIME_OP_QUEUE_H
