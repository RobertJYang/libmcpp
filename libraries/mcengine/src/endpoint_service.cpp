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

#include <mc/engine/endpoint_service.h>

#include <mc/engine/engine.h>
#include <mc/engine/service_proto.h>
#include <mc/log/log.h>
#include <mc/runtime.h>
#include <mc/shm/default_runtime.h>
#include <mc/shm/message_queue/mq_channel.h>
#include <mc/shm/message_queue/mq_proto.h>
#include <mc/shm/message_queue/mq_transport_proto.h>
#include <mc/shm/shm_runtime.h>

#include "match/shared_table.h"

#include <unistd.h>

namespace mc::engine {

struct endpoint_service::endpoint_service_impl {
    explicit endpoint_service_impl(mc::string_view name, std::shared_ptr<mc::shm::shm_runtime> rt)
        : m_endpoint_name(name), m_runtime(std::move(rt))
    {}

    mc::string                            m_endpoint_name;
    mc::string                            m_effective_name;
    std::shared_ptr<mc::shm::shm_runtime> m_runtime;

    endpoint_name_conflict_policy m_conflict_policy{endpoint_name_conflict_policy::refuse};

    mc::shm::endpoint           m_endpoint{};
    mc::engine::service_proto   m_service_proto;
    mc::shm::mq_proto           m_mq_proto;
    mc::shm::mq_transport_proto m_transport;
    mc::shm::mq_channel         m_channel;

    bool m_channel_started{false};
    bool m_endpoint_registered{false};
};

endpoint_service::endpoint_service(mc::string_view endpoint_name, std::shared_ptr<mc::shm::shm_runtime> runtime,
                                   mc::object* parent)
    : service(endpoint_name, parent),
      m_endpoint_impl(std::make_unique<endpoint_service_impl>(endpoint_name, std::move(runtime)))
{
    m_endpoint_impl->m_service_proto.add_child(m_endpoint_impl->m_mq_proto);
    m_endpoint_impl->m_mq_proto.add_child(m_endpoint_impl->m_transport);
    m_endpoint_impl->m_channel.set_protocol(&m_endpoint_impl->m_service_proto);

    m_endpoint_impl->m_service_proto.set_inbound_handler([](message msg) -> message {
        mc::runtime::post([msg = std::move(msg)]() mutable {
            mc::engine::engine::route_inbound(msg);
        }, mc::runtime::io_executor);
        return {};
    });
}

endpoint_service::endpoint_service(mc::string_view endpoint_name, mc::object* parent)
    : endpoint_service(endpoint_name, nullptr, parent)
{}

endpoint_service::~endpoint_service()
{
    // service::~service() 会调 stop() → on_stop()，正常情况下资源已经释放。
    // 这里兜底补一次，避免异常退出时漏 stop。
    if (m_endpoint_impl && m_endpoint_impl->m_channel_started) {
        m_endpoint_impl->m_channel.stop();
        m_endpoint_impl->m_channel_started = false;
    }
}

std::uint16_t endpoint_service::endpoint_id() const noexcept
{
    return m_endpoint_impl ? m_endpoint_impl->m_endpoint.endpoint_id : static_cast<std::uint16_t>(0);
}

std::uint32_t endpoint_service::instance_id() const noexcept
{
    return m_endpoint_impl ? m_endpoint_impl->m_endpoint.instance_id : 0U;
}

mc::string_view endpoint_service::queue_name() const noexcept
{
    if (!m_endpoint_impl) {
        return {};
    }
    return m_endpoint_impl->m_endpoint.queue_name;
}

std::shared_ptr<mc::shm::shm_runtime> endpoint_service::get_runtime() const noexcept
{
    return m_endpoint_impl ? m_endpoint_impl->m_runtime : std::shared_ptr<mc::shm::shm_runtime>{};
}

mc::shm::mq_channel* endpoint_service::get_mq_channel() const noexcept
{
    if (!m_endpoint_impl || !m_endpoint_impl->m_channel_started) {
        return nullptr;
    }
    return &m_endpoint_impl->m_channel;
}

match::table_ptr endpoint_service::create_match_table() const
{
    if (!m_endpoint_impl || !m_endpoint_impl->m_endpoint_registered || !m_endpoint_impl->m_runtime) {
        return {};
    }
    try {
        return mc::make_shared<match::shared_table>(*m_endpoint_impl->m_runtime,
                                                    m_endpoint_impl->m_endpoint.endpoint_id,
                                                    m_endpoint_impl->m_endpoint.instance_id);
    } catch (const std::exception& ex) {
        elog("endpoint_service.create_match_table failed: ${what}", ("what", ex.what()));
        return {};
    }
}

void endpoint_service::set_name_conflict_policy(endpoint_name_conflict_policy policy) noexcept
{
    if (m_endpoint_impl) {
        m_endpoint_impl->m_conflict_policy = policy;
    }
}

mc::string_view endpoint_service::effective_endpoint_name() const noexcept
{
    if (!m_endpoint_impl) {
        return {};
    }
    return m_endpoint_impl->m_effective_name;
}

bool endpoint_service::on_start()
{
    if (!m_endpoint_impl) {
        return false;
    }
    if (m_endpoint_impl->m_endpoint_name.empty()) {
        elog("endpoint_service.on_start: endpoint_name 为空，拒绝启动");
        return false;
    }
    if (!m_endpoint_impl->m_runtime) {
        try {
            m_endpoint_impl->m_runtime =
                std::shared_ptr<mc::shm::shm_runtime>(&mc::shm::default_runtime(), [](mc::shm::shm_runtime*) {
            });
        } catch (...) {
            elog("endpoint_service.on_start: default_runtime 不可用");
            return false;
        }
    }
    if (!m_endpoint_impl->m_runtime || !m_endpoint_impl->m_runtime->is_valid()) {
        elog("endpoint_service.on_start: shm_runtime 无效");
        return false;
    }

    auto& rt = *m_endpoint_impl->m_runtime;

    mc::string final_name(m_endpoint_impl->m_endpoint_name);

    if (auto existing = rt.find_endpoint_by_name(final_name); existing.has_value()) {
        if (rt.is_endpoint_alive(existing->endpoint_id)) {
            switch (m_endpoint_impl->m_conflict_policy) {
                case endpoint_name_conflict_policy::refuse:
                    elog("endpoint_service.on_start: endpoint '${name}' 已被活进程 pid=${pid} 占用",
                         ("name", final_name)("pid", existing->owner_pid));
                    return false;
                case endpoint_name_conflict_policy::append_pid: {
                    mc::string renamed = final_name;
                    renamed += '-';
                    renamed += mc::to_string(static_cast<std::int64_t>(::getpid()));
                    if (auto clash = rt.find_endpoint_by_name(renamed);
                        clash.has_value() && rt.is_endpoint_alive(clash->endpoint_id)) {
                        elog("endpoint_service.on_start: append_pid 兜底名 '${name}' 仍冲突 pid=${pid}",
                             ("name", renamed)("pid", clash->owner_pid));
                        return false;
                    }
                    final_name = std::move(renamed);
                    break;
                }
            }
        }
    }

    auto registered = rt.register_endpoint(final_name);
    if (!registered.has_value()) {
        elog("endpoint_service.on_start: register_endpoint 失败 name=${name}", ("name", final_name));
        return false;
    }
    m_endpoint_impl->m_endpoint            = *registered;
    m_endpoint_impl->m_effective_name      = std::move(final_name);
    m_endpoint_impl->m_endpoint_registered = true;

    if (!rt.mark_endpoint_running(m_endpoint_impl->m_endpoint)) {
        elog("endpoint_service.on_start: mark_endpoint_running 失败");
        rt.abort_endpoint(m_endpoint_impl->m_endpoint);
        m_endpoint_impl->m_endpoint_registered = false;
        return false;
    }

    try {
        m_endpoint_impl->m_channel.start(m_endpoint_impl->m_runtime, m_endpoint_impl->m_endpoint,
                                         m_endpoint_impl->m_endpoint.instance_id);
    } catch (const std::exception& ex) {
        elog("endpoint_service.on_start: mq_channel.start 抛异常: ${what}", ("what", ex.what()));
        rt.abort_endpoint(m_endpoint_impl->m_endpoint);
        m_endpoint_impl->m_endpoint_registered = false;
        return false;
    }
    m_endpoint_impl->m_channel_started = true;
    return true;
}

bool endpoint_service::on_stop()
{
    if (!m_endpoint_impl) {
        return true;
    }
    if (m_endpoint_impl->m_channel_started) {
        m_endpoint_impl->m_channel.stop();
        m_endpoint_impl->m_channel_started = false;
    }
    if (m_endpoint_impl->m_endpoint_registered && m_endpoint_impl->m_runtime) {
        m_endpoint_impl->m_runtime->abort_endpoint(m_endpoint_impl->m_endpoint);
        m_endpoint_impl->m_endpoint_registered = false;
    }
    return true;
}

} // namespace mc::engine
