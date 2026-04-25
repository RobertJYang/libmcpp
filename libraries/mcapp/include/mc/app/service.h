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

#ifndef MC_APP_SERVICE_H
#define MC_APP_SERVICE_H

#include <mc/common.h>
#include <mc/dbus/connection.h>
#include <mc/dict.h>
#include <mc/engine/service.h>
#include <mc/memory.h>
#include <mc/string.h>

#include <memory>

#include <mc/app/service_context.h>

namespace mc::app {

class app_proto;

enum class service_state { created, configured, starting, running, stopping, stopped, failed };

class MC_API service : public mc::engine::service {
public:
    explicit service(mc::string name);
    ~service() override;

    service(const service&)            = delete;
    service& operator=(const service&) = delete;
    service(service&&)                 = delete;
    service& operator=(service&&)      = delete;

    const mc::string& path() const noexcept;
    service_state     state() const noexcept;
    const mc::dict&   properties() const noexcept;

    bool configure(const service_context& context, mc::dict properties);
    bool start();
    bool stop();
    void set_path(mc::string path);

    mc::dbus::connection& connection() noexcept;
    app_proto*            proto() noexcept;

protected:
    bool         on_init(mc::dict properties) override;
    virtual bool on_configure();
    virtual bool on_start() override = 0;
    virtual bool on_stop() override  = 0;

    const service_context& context() const;
    void                   set_state(service_state state) noexcept;

private:
    bool bring_up_dbus_transport();
    void tear_down_dbus_transport();

    mc::string                 m_path;
    service_state              m_state{service_state::created};
    mc::dict                   m_properties;
    service_context            m_context;
    bool                       m_has_context{false};
    mc::dbus::connection       m_connection;
    std::unique_ptr<app_proto> m_proto;
};

using service_ptr = mc::shared_ptr<service>;

} // namespace mc::app

#endif // MC_APP_SERVICE_H
