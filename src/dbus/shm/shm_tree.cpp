/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <mc/dbus/shm/serialize.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/variant.h>

namespace mc::dbus {
static uint32_t g_serial = 0;

shm_tree::shm_tree(std::string_view harbor_name, std::string_view service_name,
                   std::string_view unique_name)
    : m_service_name(service_name), m_unique_name(unique_name),
      m_tree(create_shm_tree(harbor_name, service_name, unique_name)) {
}

void shm_tree::register_object(mc::engine::abstract_object& obj) {
#if defined(ENABLE_CONAN_COMPILE) && ENABLE_CONAN_COMPILE == 1
    auto&           ins     = shm::shared_memory::get_instance();
    auto            path    = obj.get_object_path();
    shm::object&    shm_obj = m_tree->register_object(ins, path);
    shm_obj_visitor visitor(shm_obj);
    obj.visit(visitor);
    // 需要创建上下文，common_properties_interface才能获取到对象信息
    mc::engine::object_call_stack::context object_ctx{obj.get_service(), obj};
    mc::engine::common_properties_interface::get_instance().visit(visitor);
    obj.property_changed().connect([this, &obj](const mc::variant& value, const auto& prop) {
        auto iface     = prop.get_interface()->get_interface_name();
        auto prop_name = prop.get_name();
        set_property(m_service_name, obj.get_object_path(), iface, prop_name, value);
    });
#endif
}

void shm_tree::unregister_object(std::string_view path) {
    auto& ins = shm::shared_memory::get_instance();
    m_tree->unregister_object(ins, path);
}

static uint32_t set_serial(local_msg& msg) {
    uint32_t serial = msg.get_serial();
    if (serial != 0) {
        return serial;
    }
    if (g_serial == UINT32_MAX) {
        g_serial = 0;
    }
    g_serial++;
    msg.set_serial(g_serial);
    return g_serial;
}

std::optional<mc::variants>
shm_tree::timeout_call(mc::milliseconds timeout, std::string_view service_name,
                       std::string_view path, std::string_view interface, std::string_view method,
                       std::string_view signature, const variants& args) {
    local_msg msg(service_name, path, interface, method);
    if (!signature.empty()) {
        msg.append_args(signature, args);
    }
    msg.set_local_call(false);
    auto msg_queue = harbor::get_destination_msg_queue(service_name);
    if (msg_queue == nullptr) {
        elog("failed to get message queue of destination service: ${service_name}",
             ("service_name", service_name));
        return std::nullopt;
    }
    msg.set_sender(m_unique_name);
    uint32_t serial = set_serial(msg);
    auto     data   = msg.pack();
    if (!msg_queue->push_back(data, MSG_QUEUE_PUSH_TIMEOUT)) {
        elog("failed to push to message queue of destination service: ${service_name}",
             ("service_name", service_name));
        return std::nullopt;
    }
    auto  promise = mc::make_promise<local_msg>();
    auto  future  = promise.get_future();
    auto& harbor  = mc::dbus::harbor::get_instance();
    if (!harbor.send_shm_msg(m_unique_name, serial, promise)) {
        MC_THROW(mc::exception, "failed to save promise");
    }
    if (future.wait_for(std::chrono::milliseconds(timeout)) ==
        mc::futures::future_status::timeout) {
        MC_THROW(mc::exception,
                 "shm method call timeout, service name: ${service_name}, method: ${method}",
                 ("service_name", service_name)("method", method));
    }
    auto reply_msg = future.get();
    if (reply_msg.msg_type() == DBUS_MESSAGE_TYPE_ERROR) {
        auto [error_name, error_message] = reply_msg.get_error();
        MC_THROW(mc::exception,
                 "shm method call failed, service name: ${service_name}, method: ${method}, error "
                 "name: ${error_name}, error message: ${error_message}",
                 ("service_name", service_name)("method", method)("error_name", error_name)(
                     "error_message", error_message));
    }
    return reply_msg.read();
}

static shm::property* find_shm_property(std::string_view service_name, std::string_view path,
                                        std::string_view interface, std::string_view property) {
    auto& ins  = shm::shared_memory::get_instance();
    auto  tree = ins.get_tree(service_name);
    if (tree == nullptr) {
        MC_THROW(mc::exception, "failed to get tree, service_name: ${service_name}",
                 ("service_name", service_name));
    }
    auto obj = tree->find_object(path);
    if (obj == nullptr) {
        MC_THROW(mc::exception, "failed to find object, path: ${path}", ("path", path));
    }
    auto it = obj->interfaces().find(interface);
    if (it == obj->interfaces().end()) {
        MC_THROW(mc::exception, "failed to find interface: ${interface}", ("interface", interface));
    }
    auto intf = &*it->second;
    auto prop = intf->find_p(property);
    if (prop == nullptr) {
        MC_THROW(mc::exception, "failed to find property: ${property}", ("property", property));
    }
    return prop;
}

static std::optional<variant> get_property_inner(std::string_view service_name,
                                                 std::string_view path, std::string_view interface,
                                                 std::string_view property) {
    auto prop = find_shm_property(service_name, path, interface, property);
    auto data = prop->get_data();
    if (data == std::nullopt) {
        return std::nullopt;
    }
    std::string_view prop_value = data.value();
    size_t           p_data_len = prop_value.size();
    void*            p_data     = g_malloc0(p_data_len);
    if (p_data == nullptr) {
        MC_THROW(mc::exception, "g_malloc0 failed, len: ${len}", ("len", p_data_len));
    }
    std::memcpy(p_data, prop_value.data(), p_data_len);
    std::string_view signature = prop->get_signature();
    GVariant*        v         = g_variant_new_from_data(G_VARIANT_TYPE(signature.data()), p_data, p_data_len,
                                                         false, g_free, p_data);
    return gvariant_convert::to_mc_variant(v);
}

variant shm_tree::get_property(std::string_view service_name, std::string_view path,
                               std::string_view interface, std::string_view property) {
    auto res = shm_lock_call([&]() {
        return get_property_inner(service_name, path, interface, property);
    });
    if (res == std::nullopt) {
        MC_THROW(mc::exception, "failed to get value of property: ${property}",
                 ("property", property));
    }
    return res.value();
}

void shm_tree::set_property_inner(shm::property& prop, const variant& value) {
    auto& ins = shm::shared_memory::get_instance();
    if (value.is_null()) {
        prop.set_data(ins, std::string_view());
        return;
    }
    std::string_view signature = prop.get_signature();
    GVariant*        v         = gvariant_convert::to_gvariant(value, signature.data());
    auto             data      = g_variant_get_data(v);
    auto             size      = g_variant_get_size(v);
    prop.set_data(ins, std::string_view(static_cast<const char*>(data), size));
}

void shm_tree::set_property(std::string_view service_name, std::string_view path,
                            std::string_view interface, std::string_view property,
                            const variant& value) {
    shm_lock_call([&]() {
        auto prop = find_shm_property(service_name, path, interface, property);
        set_property_inner(*prop, value);
    });
}

void shm_tree::add_match(match_rule& rule, mc::dbus::match_cb_t&& cb, uint64_t id) {
    auto& harbor      = harbor::get_instance();
    auto  harbor_name = harbor.get_harbor_name();
    harbor.add_rule(rule, std::forward<mc::dbus::match_cb_t>(cb), id);
    shm_lock_call([this, &rule, harbor_name, id]() {
        auto& ins      = shm::shared_memory::get_instance();
        auto& tree_map = ins.get_object_tree_map(harbor_name);
        auto  it       = tree_map.find(harbor_name);
        if (it == tree_map.end()) {
            return;
        }
        auto shm_slot = ins.get_base()._matchs.add_rule(ins, *rule.rule(), &*it->second);
        m_shm_slots.emplace(id, shm_slot);
    });
}

void shm_tree::remove_match(uint64_t id) {
    auto it = m_shm_slots.find(id);
    if (it == m_shm_slots.end()) {
        return;
    }
    auto slot = it->second;
    m_shm_slots.erase(it);
    shm_lock_call([slot]() {
        slot();
    });
}
} // namespace mc::dbus