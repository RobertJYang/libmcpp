/**
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

#ifndef MC_ENGINE_CALL_STACK_H
#define MC_ENGINE_CALL_STACK_H
#include <mc/common.h>

namespace mc::engine {

namespace detail {
template <typename Key, typename Value = unsigned char>
class call_stack {
public:
    using call_stack_type = call_stack<Key, Value>;

    class context : private mc::noncopyable {
    public:
        explicit context(Key* k) : m_key(k), m_next(call_stack_type::s_top) {
            m_value                = reinterpret_cast<unsigned char*>(this);
            call_stack_type::s_top = this;
        }

        // 将键值对推入调用栈
        context(Key* k, Value& v) : m_key(k), m_value(&v), m_next(call_stack_type::s_top) {
            call_stack_type::s_top = this;
        }

        // 从调用栈中弹出键值对
        ~context() {
            call_stack_type::s_top = m_next;
        }

    private:
        friend class call_stack<Key, Value>;

        // 与上下文关联的键
        Key* m_key;

        // 与上下文关联的值
        Value* m_value;

        // 下一个元素
        context* m_next;
    };

    friend class context;

    // 判断指定所有者是否在调用栈中。如果存在，返回地址，否则返回 0
    static Value* contains(Key* k) {
        context* elem = s_top;
        while (elem) {
            if (elem->m_key == k) {
                return elem->m_value;
            }
            elem = elem->m_next;
        }
        return 0;
    }

    // 获取调用栈顶的值
    static Value* top() {
        context* elem = s_top;
        return elem ? elem->m_value : 0;
    }

private:
    // 当前线程的调用栈顶
    static thread_local context* s_top;
};

template <typename Key, typename Value>
thread_local typename call_stack<Key, Value>::context* call_stack<Key, Value>::s_top;

} // namespace detail

} // namespace mc::engine

#endif // MC_ENGINE_CALL_STACK_H