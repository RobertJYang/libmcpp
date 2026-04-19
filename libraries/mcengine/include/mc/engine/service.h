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

#ifndef MC_ENGINE_MIDDLEWARE_DBUS_SERVICE_H
#define MC_ENGINE_MIDDLEWARE_DBUS_SERVICE_H
#include <mc/common.h>
#include <mc/dict.h>
#include <mc/engine/service_protocol.h>
#include <mc/object.h>
#include <mc/string.h>
#include <mc/string_view.h>

#include <map>
#include <memory>

namespace mc::engine {
class engine;
struct service_impl;
class abstract_object;
class service_object_table;

class MC_API service : public mc::object {
public:
    explicit service(mc::string_view name, mc::object* parent = nullptr);
    ~service() override;

    mc::string_view name() const;

    bool init(dict args = {});
    bool start();
    bool stop();
    void cleanup();
    bool is_healthy() const;

    void    on_dump(mc::dict context, mc::string filepath);
    void    on_detach_debug_console(mc::dict context);
    int32_t on_reboot_prepare(mc::dict context);
    int32_t on_reboot_process(mc::dict context);
    int32_t on_reboot_action(mc::dict context);
    void    on_reboot_cancel(mc::dict context);

    template <typename ObjectType>
    void register_object(mc::shared_ptr<ObjectType> obj)
    {
        register_object(*obj);
    }

    void register_object(abstract_object& obj);
    void register_object(abstract_object* obj)
    {
        if (obj == nullptr) {
            return;
        }

        register_object(*obj);
    }

    template <typename ObjectType>
    void unregister_object(mc::shared_ptr<ObjectType> obj)
    {
        unregister_object(obj->get_object_path());
    }
    void unregister_object(mc::string_view path);

    service_object_table& get_object_table() const;
    void                  set_protocol(service_protocol_ptr protocol);
    service_protocol_ptr  get_protocol() const;
    mc::variant           request(const service_operation& operation) const;
    mc::result<mc::variant> async_request(const service_operation& operation) const;
    mc::variant             timeout_call(mc::milliseconds timeout, mc::string_view endpoint, mc::string_view path,
                                         mc::string_view interface_name, mc::string_view method_name,
                                         mc::string_view signature, mc::variants args = {}, mc::dict context = {}) const;
    mc::result<mc::variant> async_timeout_call(mc::milliseconds timeout, mc::string_view endpoint, mc::string_view path,
                                               mc::string_view interface_name, mc::string_view method_name,
                                               mc::string_view signature, mc::variants args = {},
                                               mc::dict context = {}) const;
    uint64_t                add_match(mc::dbus::match_rule& rule,
                                      std::function<void(mc::dbus::message&)>&& cb) const;
    void                    remove_match(uint64_t id) const;

    static mc::string resolve_object_path(mc::string_view path_pattern, const abstract_object& obj);

protected:
    virtual bool on_init(dict args);
    virtual bool on_start();
    virtual bool on_stop();
    virtual void on_cleanup();

    mc::string                    m_name;
    std::unique_ptr<service_impl> m_impl;
};

using service_ptr = mc::shared_ptr<service>;

} // namespace mc::engine

#endif // MC_ENGINE_MIDDLEWARE_SERVICE_H
