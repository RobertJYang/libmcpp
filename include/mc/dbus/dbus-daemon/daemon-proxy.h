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

#ifndef MC_DBUS_DAEMON_PROXY_H
#define MC_DBUS_DAEMON_PROXY_H
#include <memory>

namespace mc::dbus {

class connection;
using connection_ptr = std::shared_ptr<connection>;

struct daemon_proxy {
    daemon_proxy(connection_ptr conn);
    ~daemon_proxy();

    void hello();

private:
    connection_ptr m_conn;
};

using daemon_proxy_ptr = std::shared_ptr<daemon_proxy>;

} // namespace mc::dbus

#endif // MC_DBUS_DAEMON_PROXY_H