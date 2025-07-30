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

namespace mc::engine::detail {

template <typename Key, typename Value = unsigned char>
class MC_API call_stack {
public:
    using call_stack_type = call_stack<Key, Value>;

    class MC_API context : private mc::noncopyable {
    public:
        explicit context(Key* k);

        // 将键值对推入调用栈
        context(Key* k, Value& v);

        // 从调用栈中弹出键值对
        ~context();

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

    // 判断指定所有者是否在调用栈中。如果存在，返回地址，否则返回 nullptr
    static MC_API Value* contains(Key* k);

    // 获取调用栈顶的值
    static MC_API Value* top_value();

    // 获取调用栈顶的键
    static MC_API Key* top_key();
};

template <typename Key, typename Value>
struct call_stack_impl {
    // 当前线程的调用栈顶
    static inline thread_local typename call_stack<Key, Value>::context* s_top = nullptr;
};

} // namespace mc::engine::detail

// ======= 调用栈实例化宏 =======
#define MC_ENGINE_CALL_STACK_IMPL(Key, Value)                                      \
    template class mc::engine::detail::call_stack<Key, Value>;                     \
    template <>                                                                    \
    Value* mc::engine::detail::call_stack<Key, Value>::top_value() {               \
        auto* elem = call_stack_impl<Key, Value>::s_top;                           \
        return elem ? elem->m_value : nullptr;                                     \
    }                                                                              \
    template <>                                                                    \
    Key* mc::engine::detail::call_stack<Key, Value>::top_key() {                   \
        auto* elem = call_stack_impl<Key, Value>::s_top;                           \
        return elem ? elem->m_key : nullptr;                                       \
    }                                                                              \
    template <>                                                                    \
    Value* mc::engine::detail::call_stack<Key, Value>::contains(Key* k) {          \
        auto* elem = call_stack_impl<Key, Value>::s_top;                           \
        while (elem) {                                                             \
            if (elem->m_key == k)                                                  \
                return elem->m_value;                                              \
            elem = elem->m_next;                                                   \
        }                                                                          \
        return nullptr;                                                            \
    }                                                                              \
    template <>                                                                    \
    mc::engine::detail::call_stack<Key, Value>::context::context(Key* k)           \
        : m_key(k), m_next(call_stack_impl<Key, Value>::s_top) {                   \
        m_value                            = reinterpret_cast<Value*>(this);       \
        call_stack_impl<Key, Value>::s_top = this;                                 \
    }                                                                              \
    template <>                                                                    \
    mc::engine::detail::call_stack<Key, Value>::context::context(Key* k, Value& v) \
        : m_key(k), m_value(&v), m_next(call_stack_impl<Key, Value>::s_top) {      \
        call_stack_impl<Key, Value>::s_top = this;                                 \
    }                                                                              \
    template <>                                                                    \
    mc::engine::detail::call_stack<Key, Value>::context::~context() {              \
        call_stack_impl<Key, Value>::s_top = m_next;                               \
    }

#endif // MC_ENGINE_CALL_STACK_H
