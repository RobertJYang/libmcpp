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

#include <mc/core/object.h>
#include <mc/time.h>

namespace mc::core {

class timer;
using timer_ptr = mc::core::ref_ptr<timer>;

/*
 * 定时器
 */
class timer : public object {
public:
    timer(object* parent = nullptr);
    ~timer() override;

    timer(timer&& other)            = delete;
    timer& operator=(timer&& other) = delete;

    /**
     * @brief 获取定时器间隔时间
     * @return 定时器间隔时间
     */
    mc::milliseconds interval() const;

    /**
     * @brief 设置定时器间隔时间
     * @param msec 定时器间隔时间
     */
    void set_interval(mc::milliseconds msec);

    /**
     * @brief 获取定时器是否只执行一次
     * @return 如果定时器只执行一次则返回true，否则返回false
     */
    bool is_single_shot() const;

    /**
     * @brief 设置定时器是否只执行一次
     * @param single_shot 如果定时器只执行一次则设置为true，否则设置为false
     */
    void set_single_shot(bool single_shot);

    /**
     * @brief 获取定时器是否正在运行
     * @return 如果定时器正在运行则返回true，否则返回false
     */
    bool is_active() const;

    /**
     * @brief 启动定时器
     * @param msec 定时器间隔时间
     *
     * 如果定时器已经启动，则重启定时器并设置定时器间隔为新的时间
     * 如果 single_shot 为 true，则定时器只执行一次
     */
    void start(mc::milliseconds msec = mc::milliseconds(0));

    /**
     * @brief 停止定时器
     */
    void stop();

    mc::signal<void()> timeout;

    template <typename Object>
    static timer_ptr single_shot(mc::milliseconds msec, ref_ptr<Object> receiver,
                                 void (std::decay_t<Object>::*method)()) {
        if (!receiver || !method) {
            return {};
        }

        return single_shot(msec, receiver, [receiver = std::move(receiver), method]() {
            (const_cast<Object*>(receiver.get())->*method)();
        });
    }
    static timer_ptr single_shot(mc::milliseconds msec, std::function<void()> functor);

    template <typename Object>
    static timer_ptr single_shot(mc::milliseconds msec, ref_ptr<Object> context,
                                 std::function<void()>&& functor) {
        if (!context || !functor) {
            return {};
        }

        return single_shot(msec, context.get(), std::forward<std::function<void()>>(functor));
    }

    static timer_ptr single_shot(mc::milliseconds msec, const object* context,
                                 std::function<void()> functor);

private:
    bool check_active() const;

private:
    mc::milliseconds m_interval{0};
    bool             m_single_shot{false};

    struct timer_impl;
    std::unique_ptr<timer_impl> m_impl;
};

} // namespace mc::core

#endif // MC_ENGINE_TIMER_H
