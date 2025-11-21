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
#include <mc/core/service.h>
#include <mc/dbus/connection.h>
#include <mc/time.h>
#include <memory>

namespace mc::engine {
class engine;
struct service_impl;
class abstract_object;
class service_object_table;

class MC_API service : public mc::core::service_base, public mc::noncopyable_nonmovable {
public:
    service(std::string_view name);
    ~service() override;

    bool init(dict args = {}) override;
    bool start() override;
    bool stop() override;
    void cleanup() override;
    bool is_healthy() const override;

    void on_dump(std::map<std::string, std::string> context, std::string filepath) override;
    void on_detach_debug_console(std::map<std::string, std::string> context) override;
    void on_attach_debug_console(std::map<std::string, std::string> context, uint32_t port) override;
    void on_set_dlog_level(std::map<std::string, std::string> context, std::string level,
                           uint8_t effective_hours) override;
    void on_dlog_limit(std::map<std::string, std::string> context, bool enabled,
                       uint8_t duration_mins) override;
    int32_t on_reboot_prepare(std::map<std::string, std::string> context) override;
    int32_t on_reboot_process(std::map<std::string, std::string> context) override;
    int32_t on_reboot_action(std::map<std::string, std::string> context) override;
    void on_reboot_cancel(std::map<std::string, std::string> context) override;

    template <typename ObjectType>
    void register_object(mc::shared_ptr<ObjectType> obj) {
        register_object(*obj);
    }

    void register_object(abstract_object& obj);
    void register_object(abstract_object* obj) {
        if (obj == nullptr) {
            return;
        }

        register_object(*obj);
    }

    template <typename ObjectType>
    void unregister_object(mc::shared_ptr<ObjectType> obj) {
        unregister_object(obj->get_object_path());
    }
    void unregister_object(std::string_view path);

    service_object_table& get_object_table() const;
    mc::dbus::connection  get_connection() const;

    mc::variant timeout_call(mc::milliseconds timeout, std::string_view service_name,
                             std::string_view path, std::string_view interface,
                             std::string_view method, std::string_view signature,
                             const mc::variants& args);

    std::optional<mc::variant> shm_timeout_call(mc::milliseconds timeout,
                                                std::string_view service_name,
                                                std::string_view path, std::string_view interface,
                                                std::string_view method, std::string_view signature,
                                                const mc::variants& args);

    static std::string resolve_object_path(std::string_view       path_pattern,
                                           const abstract_object& obj);

    uint64_t add_match(mc::dbus::match_rule& rule, mc::dbus::match_cb_t&& cb);
    void     remove_match(uint64_t id);

protected:
    std::unique_ptr<service_impl> m_impl;
};

using service_ptr = mc::shared_ptr<service>;

} // namespace mc::engine

#endif // MC_ENGINE_MIDDLEWARE_SERVICE_H
