/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_SHM_MESSAGE_QUEUE_PRIVATE_MQ_NOTIFIER_H
#define MC_SHM_MESSAGE_QUEUE_PRIVATE_MQ_NOTIFIER_H

#include <chrono>
#include <memory>

#include <mc/string.h>
#include <mc/string_view.h>

namespace mc::shm::detail {

class mq_notifier {
public:
    mq_notifier() noexcept;
    explicit mq_notifier(mc::string_view name);
    ~mq_notifier();

    mq_notifier(const mq_notifier&)            = delete;
    mq_notifier& operator=(const mq_notifier&) = delete;

    mq_notifier(mq_notifier&& other) noexcept;
    mq_notifier& operator=(mq_notifier&& other) noexcept;

    bool              is_valid() const noexcept;
    const mc::string& name() const noexcept;
    bool              notify() const noexcept;
    void              drain() const noexcept;
    bool              wait_until(std::chrono::steady_clock::time_point deadline) const noexcept;

#ifndef _WIN32
    int native_handle() const noexcept;
#endif

    static mc::string make_default_name(mc::string_view queue_name);
    static mc::string make_space_name(mc::string_view queue_name);
    static bool       remove(mc::string_view name) noexcept;

    struct impl;
    std::shared_ptr<impl> m_impl;
};

} // namespace mc::shm::detail

#endif // MC_SHM_MESSAGE_QUEUE_PRIVATE_MQ_NOTIFIER_H
