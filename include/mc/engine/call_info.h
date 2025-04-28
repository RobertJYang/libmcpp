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

#ifndef MC_ENGINE_CONTEXT_H
#define MC_ENGINE_CONTEXT_H
#include <mc/dbus/message.h>
#include <mc/dict.h>
#include <mc/engine/error.h>
#include <mc/variant.h>

namespace mc::engine {
class object_base;

class dbus_call_info {
public:
    dbus_call_info(const mc::dbus::message& request, object_base* object)
        : m_request(request), m_object(object) {
    }

    void set_request(const mc::dbus::message& request) {
        m_request = request;
    }

    void set_response(const mc::dbus::message& response) {
        m_response = response;
    }

    void set_error(const mc::engine::error& error) {
        m_error = error;
    }

    mc::dbus::message& get_request() {
        return m_request;
    }

    mc::dbus::message& get_response() {
        return m_response;
    }

    mc::engine::error& get_error() {
        return m_error;
    }

    bool has_error() const {
        return m_error.is_set();
    }

    template <typename T>
    void set_arg(std::string_view key, T&& value) {
        m_args[key] = std::forward<T>(value);
    }

    mc::variant get_arg(std::string_view key) const {
        if (m_args.find(key) == m_args.end()) {
            return mc::variant();
        }

        return m_args[key];
    }

    const mc::dict& get_args() const {
        return m_args;
    }

    mc::dict& get_args() {
        return m_args;
    }

    void set_args(mc::dict args) {
        m_args = std::move(args);
    }

    object_base* get_object() const {
        return m_object;
    }

protected:
    mc::dbus::message m_request;
    mc::dbus::message m_response;
    mc::engine::error m_error;
    mc::dict          m_args;

    object_base* m_object;
};

} // namespace mc::engine
#endif // MC_ENGINE_CONTEXT_H