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

#include <mc/core/service.h>
#include <mc/engine/engine.h>

#include <memory>

namespace mc::core {

struct service_base::impl {
    impl()
        : m_state(service_state::stopped),
          m_executor(mc::engine::engine::get_instance().get_io_context().get_executor()) {
    }

    std::string              m_name;
    std::vector<std::string> m_dependencies;
    service_state            m_state;
    executor_type            m_executor;
};

service_base::service_base(std::string name) : m_impl(std::make_unique<impl>()) {
    m_impl->m_name  = std::move(name);
    m_impl->m_state = service_state::stopped;
    set_service(this);
}

service_base::~service_base() {
}

void service_base::set_name(std::string name) {
    m_impl->m_name = std::move(name);
}

const std::string& service_base::name() const {
    return m_impl->m_name;
}

service_state service_base::get_state() const {
    return m_impl->m_state;
}

const service_config& service_base::get_config() const {
    static service_config empty_config;
    return empty_config;
}

const std::vector<std::string>& service_base::get_dependencies() const {
    return m_impl->m_dependencies;
}

void service_base::set_dependencies(const std::vector<std::string>& dependencies) {
    m_impl->m_dependencies = dependencies;
}

void service_base::set_state(service_state state) {
    m_impl->m_state = state;
}

object_base::executor_type& service_base::get_executor() const {
    return m_impl->m_executor;
}

} // namespace mc::core
