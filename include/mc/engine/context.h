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
#include <mc/engine/error_engine.h>
#include <mc/variant.h>

namespace mc::engine {
class object_base;

struct dbus_call_info {
    mc::dbus::message request;
    mc::dbus::message response;
};

struct shm_call_info {
    mc::variant request;
    mc::variant response;
};

using call_info = std::variant<dbus_call_info, shm_call_info>;

class context {
public:
    context(object_base* object);
    ~context();

    void        set_arg(std::string_view key, mc::variant value);
    mc::variant get_arg(std::string_view key) const;

    const mc::dict& get_args() const;
    mc::dict&       get_args();
    void            set_args(mc::dict args);

    object_base* get_object() const;

    call_info& get_call_info();
    void       set_call_info(call_info call_info);

    error& get_error();
    bool   has_error() const;
    void   report_error(std::string_view error_name, mc::dict args = {});

private:
    object_base*     m_object;
    error            m_error;
    mc::mutable_dict m_args;
    call_info        m_call_info;
};

} // namespace mc::engine
#endif // MC_ENGINE_CONTEXT_H