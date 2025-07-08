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

#ifndef MC_DBUS_MATCH_H
#define MC_DBUS_MATCH_H

#include <dbus/dbus.h>
#include <mc/dbus/message.h>

#define BUILD_TYPE_DT (0x0a)

#if (defined(BUILD_TYPE) && defined(BUILD_TYPE_DT) && BUILD_TYPE != BUILD_TYPE_DT) || \
    (defined(ENABLE_CONAN_COMPILE) && ENABLE_CONAN_COMPILE == 1)
#include <dbus/match/matchs.h>
#include <dbus/shm_tree/object.h>
#include <dbus/shm_tree/shared_memory.h>
#include <dbus/shm_tree/shared_memory_base.h>
#include <dbus/shm_tree/tree.h>
#else
// DT环境下如果禁用共享内存，使用打桩的定义
#include <mc/dbus/shm/mock_shm.h>
#endif

namespace mc::dbus {
constexpr std::string_view DBUS_PROPERTIES_INTERFACE     = "org.freedesktop.DBus.Properties";
constexpr std::string_view PROPERTIES_CHANGED_MEMBER     = "PropertiesChanged";
constexpr std::string_view DBUS_OBJECT_MANAGER_INTERFACE = "org.freedesktop.DBus.ObjectManager";
constexpr std::string_view INTERFACES_ADDED_MEMBER       = "InterfacesAdded";
constexpr std::string_view INTERFACES_REMOVED_MEMBER     = "InterfacesRemoved";

template <typename Fn, typename... Args>
auto shm_lock_call(Fn&& callback, Args&&... args) {
    auto& ins = shm::shared_memory::get_instance();
    ins.lock();
    if constexpr (std::is_void_v<std::invoke_result_t<Fn, Args...>>) {
        std::invoke(std::forward<Fn>(callback), std::forward<Args>(args)...);
        ins.unlock();
    } else {
        auto result = std::invoke(std::forward<Fn>(callback), std::forward<Args>(args)...);
        ins.unlock();
        return result;
    }
}

using match_cb_t = std::function<void(mc::dbus::message&)>;

class match_rule {
public:
    match_rule(DBus::Match::MessageType type, const std::string_view& member,
               const std::string_view& interface);
    static match_rule     new_signal(const std::string_view& member, const std::string_view& interface);
    void                  with_interface(const std::string_view& interface);
    void                  with_member(const std::string_view& member);
    void                  with_path(const std::string_view& path);
    void                  with_path_namespace(const std::string_view& path_namespace);
    void                  with_sender(const std::string_view& sender);
    void                  with_type(DBus::Match::MessageType type);
    void                  with_destination(const std::string_view& destination);
    match_rule            clone() const;
    std::string           as_string() const;
    DBus::Match::RulePtr& rule();

private:
    DBus::Match::RulePtr m_rule;
};

class match {
public:
    match();
    void add_rule(match_rule& rule, match_cb_t&& cb, uint64_t id);
    void remove_rule(uint64_t id);
    bool run_msg(DBusMessage* msg);
    bool test_match(DBusMessage* msg);

private:
    DBus::Match::Matchs                                m_matchs;
    std::unordered_map<uint64_t, DBus::Match::RulePtr> m_rules;
};

} // namespace mc::dbus

#endif // MC_DBUS_MATCH_H