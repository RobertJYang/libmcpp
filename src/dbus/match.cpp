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

#include <mc/dbus/error.h>
#include <mc/dbus/match.h>
#include <mc/exception.h>
#include <mc/log.h>

namespace mc::dbus {

match::match() {
}

bool match::run_msg(DBusMessage* msg) {
    DBus::Match::Context ctx;
    ctx.set_req(msg);
    return m_matchs.run(ctx);
}

bool match::test_match(DBusMessage* msg) {
    DBus::Match::Context ctx;
    ctx.set_req(msg);
    return m_matchs.test_match(ctx);
}

void match::add_rule(match_rule& rule, match_cb_t&& cb, uint64_t id) {
    auto cb_ptr = std::make_shared<match_cb_t>(cb);
    m_matchs.add_rule(rule.rule(), [cb_ptr](DBus::Match::Context& ctx) {
        try {
            dbus_message_ref(ctx.req);
            message msg(ctx.req, true);
            return (*cb_ptr)(msg);
        } catch (std::exception& e) {
            elog("DBus error: match rule failed: ${error}", ("error", e.what()));
        } catch (...) {
            elog("DBus error: match rule failed");
        }
    });
    m_rules.emplace(id, rule.rule());
}

void match::remove_rule(uint64_t id) {
    auto it = m_rules.find(id);
    if (it == m_rules.end()) {
        return;
    }
    auto rule = it->second;
    m_rules.erase(it);
    if (rule->is_connected()) {
        rule->disconnect();
    }
}

template <dbus_bool_t (*f)(const char*, DBusError*)>
static void validate_dbus_str(const char* value) {
    mc::dbus::error err;
    if (!f(value, &err)) {
        MC_THROW(mc::invalid_arg_exception, "invalid dbus arg ${value}: ${error}",
                 ("value", value)("error", err.message));
    }
}

match_rule::match_rule(DBus::Match::MessageType type, const std::string_view& member, const std::string_view& interface)
    : m_rule(std::make_shared<DBus::Match::Rule>()) {
    m_rule->type(type);
    validate_dbus_str<dbus_validate_member>(member.data());
    m_rule->member(member);
    validate_dbus_str<dbus_validate_interface>(interface.data());
    m_rule->interface(interface);
}

match_rule match_rule::new_signal(const std::string_view& member,
                                  const std::string_view& interface) {
    return {DBus::Match::MessageType::signal, member, interface};
}

void match_rule::with_interface(const std::string_view& interface) {
    validate_dbus_str<dbus_validate_interface>(interface.data());
    m_rule->interface(interface);
}

void match_rule::with_member(const std::string_view& member) {
    validate_dbus_str<dbus_validate_member>(member.data());
    m_rule->member(member);
}

void match_rule::with_path(const std::string_view& path) {
    validate_dbus_str<dbus_validate_path>(path.data());
    m_rule->path(path);
}

void match_rule::with_path_namespace(const std::string_view& path_namespace) {
    validate_dbus_str<dbus_validate_path>(path_namespace.data());
    m_rule->path_namespace(path_namespace);
}

void match_rule::with_sender(const std::string_view& sender) {
    validate_dbus_str<dbus_validate_bus_name>(sender.data());
    m_rule->sender(sender);
}

void match_rule::with_type(DBus::Match::MessageType type) {
    m_rule->type(type);
}

void match_rule::with_destination(const std::string_view& destination) {
    validate_dbus_str<dbus_validate_bus_name>(destination.data());
    m_rule->destination(destination);
}

match_rule match_rule::clone() const {
    match_rule rule(m_rule->type(), m_rule->member(), m_rule->interface());
    auto path = m_rule->path();
    if (!path.empty()) {
        if (m_rule->path_namespace()) {
            rule.with_path_namespace(path);
        } else {
            rule.with_path(path);
        }
    }
    auto sender = m_rule->sender();
    if (!sender.empty()) {
        rule.with_sender(sender);
    }
    auto destination = m_rule->destination();
    if (!destination.empty()) {
        rule.with_destination(destination);
    }
    return rule;
}

DBus::Match::RulePtr& match_rule::rule() {
    return m_rule;
}

std::string match_rule::as_string() const {
    return m_rule->as_string();
}

} // namespace mc::dbus