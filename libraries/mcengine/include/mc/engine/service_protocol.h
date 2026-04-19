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

#ifndef MC_ENGINE_SERVICE_PROTOCOL_H
#define MC_ENGINE_SERVICE_PROTOCOL_H

#include <mc/common.h>
#include <mc/exception.h>
#include <mc/memory.h>
#include <mc/engine/service_operation.h>
#include <mc/result.h>
#include <mc/string_view.h>

#include <functional>

namespace mc::engine {

class abstract_object;
class service;

} // namespace mc::engine

namespace mc::dbus {
class match_rule;
class message;
}

namespace mc::engine {

class MC_API service_protocol : public mc::shared_base {
public:
    virtual ~service_protocol() = default;

    virtual mc::string_view name() const noexcept = 0;

    // attach() may inspect the current object table and perform an initial sync.
    virtual void attach(service& service) = 0;
    virtual void detach()                 = 0;

    virtual void register_object(abstract_object& obj)   = 0;
    virtual void unregister_object(mc::string_view path) = 0;

    virtual mc::variant            request(const service_operation& operation)              = 0;
    virtual mc::result<mc::variant> async_request(const service_operation& operation)       = 0;
    virtual uint64_t add_match(mc::dbus::match_rule&, std::function<void(mc::dbus::message&)>&&)
    {
        MC_THROW(mc::invalid_op_exception, "service protocol does not support add_match");
    }
    virtual void remove_match(uint64_t)
    {
        MC_THROW(mc::invalid_op_exception, "service protocol does not support remove_match");
    }
};

using service_protocol_ptr = mc::shared_ptr<service_protocol>;

} // namespace mc::engine

#endif // MC_ENGINE_SERVICE_PROTOCOL_H
