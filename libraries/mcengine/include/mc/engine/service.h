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
#include <mc/engine/match.h>
#include <mc/engine/message.h>
#include <mc/engine/proxy.h>
#include <mc/future.h>
#include <mc/object.h>
#include <mc/signal/signal.h>
#include <mc/string.h>
#include <mc/string_view.h>
#include <mc/time.h>
#include <mc/variant.h>

#include <map>
#include <memory>

namespace mc::engine {
class engine;
struct service_impl;
class abstract_object;
class service_object_table;
class service_proto;
class service_backend;
} // namespace mc::engine

namespace mc::engine {

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

    std::size_t gc_isolated();

    service_object_table& get_object_table() const;

    void           set_proto(service_proto* proto);
    service_proto* get_proto() const;
    void           set_backend(service_backend* backend);

    void emit(const message& msg) const;

    match_id add_match(match_rule rule, filter_spec spec, match_callback callback) const;
    void     remove_match(match_id id) const;
    void     dispatch_event(const message& msg) const;

    mc::signal<void(match_id, const match_rule&)> on_match_added;
    mc::signal<void(match_id)>                    on_match_removed;

    message             send_with_reply(message request, mc::milliseconds timeout = mc::milliseconds(5000)) const;
    mc::future<message> async_send_with_reply(message request, mc::milliseconds timeout = mc::milliseconds(5000)) const;

    void send(message request) const;

    mc::variant call(mc::string_view path, mc::string_view service_name, mc::string_view interface_name,
                     mc::string_view method_name, const mc::variants& args = {}, mc::string_view signature = {},
                     mc::milliseconds timeout = mc::milliseconds(5000)) const;

    // 兼容旧版接口，参数顺序为 (timeout, service_name, path, interface, method, signature, args)
    mc::variant timeout_call(mc::milliseconds timeout, mc::string_view service_name, mc::string_view path,
                             mc::string_view interface_name, mc::string_view method_name, mc::string_view signature,
                             const mc::variants& args = {}) const
    {
        return call(path, service_name, interface_name, method_name, args, signature, timeout);
    }

    static mc::string resolve_object_path(mc::string_view path_pattern, const abstract_object& obj);

    template <typename ProxyT>
    std::unique_ptr<ProxyT> get_proxy(mc::string_view path, proxy_policy policy = {}) const
    {
        auto seed = mc::engine::make_proxy_seed(path, policy);
        if (seed.is_valid()) {
            auto proxy = std::make_unique<ProxyT>();
            proxy->bind_proxy(seed);
            return proxy;
        }
        return nullptr;
    }

    template <typename ProxyT>
    std::unique_ptr<ProxyT> get_proxy(mc::string_view path, mc::string_view service_hint,
                                      proxy_policy policy = {}) const
    {
        auto seed = mc::engine::make_proxy_seed(path, service_hint, policy);
        if (seed.is_valid()) {
            auto proxy = std::make_unique<ProxyT>();
            proxy->bind_proxy(seed);
            return proxy;
        }

        if (!service_hint.empty()) {
            mc::engine::object_proxy_seed remote_seed;
            remote_seed.ref.path    = mc::string(path);
            remote_seed.ref.service = mc::string(service_hint);
            remote_seed.svc         = this;
            remote_seed.resolver    = mc::engine::make_default_proxy_shm_resolver();
            remote_seed.policy      = policy;

            auto proxy = std::make_unique<ProxyT>();
            proxy->bind_proxy(remote_seed);
            return proxy;
        }

        return nullptr;
    }

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
