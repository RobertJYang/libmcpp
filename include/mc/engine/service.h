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
#include <mc/time.h>
#include <memory>
#include <mc/dbus/connection.h>

namespace mc::engine {
class engine;
struct service_impl;
class abstract_object;
class service_object_table;

class service : public mc::core::service_base, public mc::noncopyable_nonmovable {
public:
    service(std::string_view name);
    ~service() override;

    bool init(dict args = {}) override;
    bool start() override;
    bool stop() override;
    void cleanup() override;
    bool is_healthy() const override;

    template <typename ObjectType>
    void register_object(mc::im::ref_ptr<ObjectType> obj) {
        register_object(*obj);
    }

    template <typename ObjectType>
    void register_object(ObjectType* obj) {
        register_object(*obj);
    }

    template <typename ObjectType>
    void unregister_object(mc::im::ref_ptr<ObjectType> obj) {
        unregister_object(obj->get_object_path());
    }
    void unregister_object(std::string_view path);

    service_object_table& get_object_table() const;
    mc::dbus::connection_ptr get_connection();

    mc::variant timeout_call(mc::milliseconds timeout, std::string_view service_name,
                             std::string_view path, std::string_view interface,
                             std::string_view method, std::string_view signature,
                             const mc::variants& args);

    std::optional<mc::variant> shm_timeout_call(mc::milliseconds timeout,
                                                std::string_view service_name,
                                                std::string_view path, std::string_view interface,
                                                std::string_view method, std::string_view signature,
                                                const mc::variants& args);

protected:
    void register_object(abstract_object& obj);

    std::unique_ptr<service_impl> m_impl;
};

using service_ptr = std::shared_ptr<service>;

} // namespace mc::engine

#endif // MC_ENGINE_MIDDLEWARE_SERVICE_H
