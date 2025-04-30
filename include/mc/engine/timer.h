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

#ifndef MC_ENGINE_TIMER_H
#define MC_ENGINE_TIMER_H

#include <mc/time.h>

#include <functional>
#include <memory>

namespace mc::engine {
class abstract_object;

enum class timer_type {
    ONCE,
    REPEAT,
};

enum class timer_state {
    IDLE,
    RUNNING,
    STOPPED,
    CANCELLED,
};

/*
 * 定时器
 *
 * 需要绑定到 abstract_object 上才能工作
 */
class timer {
public:
    using timer_cb = std::function<void()>;

    timer(timer_type type = timer_type::REPEAT);
    ~timer();

    timer(const timer&)            = delete;
    timer& operator=(const timer&) = delete;

    timer(timer&& other) noexcept;
    timer& operator=(timer&& other) noexcept;

    void             set_object(abstract_object& obj);
    abstract_object* get_object() const;

    void start(mc::milliseconds ms, timer_cb&& cb);
    void stop();
    void restart();

protected:
    timer_type       m_type;
    abstract_object* m_object{nullptr};
    timer_state      m_state{timer_state::IDLE};

private:
    struct timer_impl;
    timer_impl* m_impl{nullptr};
};

/*
 * 一个通过虚函数分发的定时器
 *
 * 子类需要实现 on_timeout 方法
 */
class basic_timer : public timer {
public:
    virtual ~basic_timer();

    virtual void on_timeout() = 0;

    void start(mc::milliseconds ms);
};

} // namespace mc::engine

#endif // MC_ENGINE_TIMER_H
