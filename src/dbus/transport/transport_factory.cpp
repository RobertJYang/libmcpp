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

// #include <mc/dbus/transport/tcp_transport.h>
#include <mc/dbus/transport/transport_factory.h>
#include <mc/dbus/transport/unix_socket_transport.h>
#include <mc/exception.h>
#include <mc/log.h>

namespace mc::dbus {

transport_factory::transport_factory() {
}

transport_factory::~transport_factory() {
}

bool transport_factory::register_creator(std::string method, creator_type creator) {
    if (m_creators.find(method) != m_creators.end()) {
        elog("DBus 传输方式已注册: ${method}", ("method", method));
        return false;
    }

    m_creators.emplace(std::move(method), std::move(creator));
    return true;
}

transport_ptr transport_factory::create(io_context_type& io_context, std::string_view address_str) {
    address addr = address::parse(address_str);
    for (const auto& entry : addr.get_entries()) {
        try {
            auto transport = create(io_context, entry);
            if (transport) {
                return transport;
            }
        } catch (const mc::exception& e) {
            wlog("创建传输层失败: ${error}, 尝试下一个地址", ("error", e.what()));
        }
    }
    return nullptr;
}

transport_ptr transport_factory::create(io_context_type& io_context, address_entry_ptr entry) {
    auto& method  = entry->get_method();
    auto  creator = m_creators.find(method);
    if (creator == m_creators.end()) {
        elog("不支持的传输方式: ${method}", ("method", method));
        return nullptr;
    }

    return creator->second(io_context, std::move(entry));
}

} // namespace mc::dbus
