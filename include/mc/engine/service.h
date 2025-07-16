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
    MC_API service(std::string_view name);
    MC_API ~service() override;

    MC_API bool init(dict args = {}) override;
    MC_API bool start() override;
    MC_API bool stop() override;
    MC_API void cleanup() override;
    MC_API bool is_healthy() const override;

    template <typename ObjectType>
    void register_object(mc::shared_ptr<ObjectType> obj) {
        register_object(*obj);
    }

    MC_API void register_object(abstract_object& obj);
    void        register_object(abstract_object* obj) {
        if (obj == nullptr) {
            return;
        }

        register_object(*obj);
    }

    template <typename ObjectType>
    void unregister_object(mc::shared_ptr<ObjectType> obj) {
        unregister_object(obj->get_object_path());
    }
    MC_API void unregister_object(std::string_view path);

    MC_API service_object_table& get_object_table() const;
    MC_API mc::dbus::connection get_connection() const;

    MC_API mc::variant timeout_call(mc::milliseconds timeout, std::string_view service_name,
                                    std::string_view path, std::string_view interface,
                                    std::string_view method, std::string_view signature,
                                    const mc::variants& args);

    MC_API std::optional<mc::variant> shm_timeout_call(mc::milliseconds timeout,
                                                       std::string_view service_name,
                                                       std::string_view path, std::string_view interface,
                                                       std::string_view method, std::string_view signature,
                                                       const mc::variants& args);

    MC_API static std::string resolve_object_path(std::string_view       path_pattern,
                                                  const abstract_object& obj);

    MC_API uint64_t add_match(mc::dbus::match_rule& rule, mc::dbus::match_cb_t&& cb);
    MC_API void     remove_match(uint64_t id);

protected:
    std::unique_ptr<service_impl> m_impl;
};

using service_ptr = mc::shared_ptr<service>;

} // namespace mc::engine

#endif // MC_ENGINE_MIDDLEWARE_SERVICE_H
