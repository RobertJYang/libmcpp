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

#ifndef MC_DBUS_SHM_MOCK_SHM_H
#define MC_DBUS_SHM_MOCK_SHM_H

#include <dbus/dbus.h>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

static constexpr int SHM_LOCK_TIMEOUT_DEFAULT_MS = 60000;

namespace shmlock {

class LockHandle {
public:
    LockHandle() {
    }

    void release() {
    }
};

class ShmLockManager;
static std::mutex                      _shm_instance_mutex;
static std::unique_ptr<ShmLockManager> _shm_instance;

class ShmLockManager {
public:
    ShmLockManager() {
    }

    static ShmLockManager& get_instance() {
        std::lock_guard lock(_shm_instance_mutex);

        if (!_shm_instance) {
            _shm_instance = std::make_unique<ShmLockManager>();
        }

        return *_shm_instance;
    }

    static uint16_t allocate_service_id() {
        return 1;
    }

    LockHandle acquire_read_lock(uint64_t object_id, uint16_t service_id, uint32_t timeout_ms = SHM_LOCK_TIMEOUT_DEFAULT_MS) {
        return LockHandle();
    }

    LockHandle acquire_write_lock(uint64_t object_id, uint16_t service_id, uint32_t timeout_ms = SHM_LOCK_TIMEOUT_DEFAULT_MS) {
        return LockHandle();
    }
};
} // namespace shmlock

namespace DBus {
namespace Match {

enum class MessageType : int {
    invalid       = DBUS_MESSAGE_TYPE_INVALID,
    signal        = DBUS_MESSAGE_TYPE_SIGNAL,
    method_call   = DBUS_MESSAGE_TYPE_METHOD_CALL,
    method_return = DBUS_MESSAGE_TYPE_METHOD_RETURN,
    error         = DBUS_MESSAGE_TYPE_ERROR
};

class shared_memory;

class Rule {
public:
    Rule() : m_type(MessageType::signal), m_path_namespace(false) {
    }

    void member(const std::string_view& member) {
        m_member = std::string(member);
    }

    std::string_view member() const {
        return m_member;
    }

    void interface(const std::string_view& interface) {
        m_interface = std::string(interface);
    }

    std::string_view interface() const {
        return m_interface;
    }

    void path(const std::string_view& path) {
        m_path = std::string(path);
    }

    std::string_view path() const {
        return m_path;
    }

    void path_namespace(const std::string_view& path_namespace) {
        m_path = std::string(path_namespace);
        m_path_namespace = true;
    }

    bool path_namespace() const {
        return m_path_namespace;
    }

    void sender(const std::string_view& sender) {
        m_sender = std::string(sender);
    }

    std::string_view sender() const {
        return m_sender;
    }

    void type(MessageType type) {
        m_type = type;
    }

    MessageType type() const {
        return m_type;
    }

    bool is_connected() {
        return false;
    }

    void disconnect() {
    }

    void destination(const std::string_view& destination) {
        m_destination = std::string(destination);
    }

    std::string_view destination() const {
        return m_destination;
    }

    std::string as_string() const {
        std::string result;
        result.reserve(200);
        
        // type 字段
        if (m_type == MessageType::signal) {
            result += "type='signal'";
        } else if (m_type == MessageType::method_call) {
            result += "type='method_call'";
        } else if (m_type == MessageType::method_return) {
            result += "type='method_return'";
        } else if (m_type == MessageType::error) {
            result += "type='error'";
        }
        
        // interface 字段
        if (!m_interface.empty()) {
            if (!result.empty()) result += ",";
            result += "interface='";
            result += m_interface;
            result += "'";
        }
        
        // member 字段
        if (!m_member.empty()) {
            if (!result.empty()) result += ",";
            result += "member='";
            result += m_member;
            result += "'";
        }
        
        // path 或 path_namespace 字段
        if (!m_path.empty()) {
            if (!result.empty()) result += ",";
            if (m_path_namespace) {
                result += "path_namespace='";
            } else {
                result += "path='";
            }
            result += m_path;
            result += "'";
        }
        
        // sender 字段
        if (!m_sender.empty()) {
            if (!result.empty()) result += ",";
            result += "sender='";
            result += m_sender;
            result += "'";
        }
        
        // destination 字段
        if (!m_destination.empty()) {
            if (!result.empty()) result += ",";
            result += "destination='";
            result += m_destination;
            result += "'";
        }
        
        return result;
    }

private:
    MessageType m_type;
    std::string m_member;
    std::string m_interface;
    std::string m_path;
    bool m_path_namespace;
    std::string m_sender;
    std::string m_destination;
};

using RulePtr = std::shared_ptr<Rule>;

struct Context {
    void set_req(DBusMessage* msg) {
        req = msg;
    }

    DBusMessage* req;
};

class Matchs {
public:
    void add_rule(RulePtr& rule, std::function<void(Context&)> cb) {
        m_rules.push_back({rule, cb});
    }

    bool run(Context& ctx) {
        if (!ctx.req) {
            return false;
        }
        
        bool matched = false;
        for (auto& [rule, cb] : m_rules) {
            if (test_rule_match(rule.get(), ctx.req)) {
                matched = true;
                cb(ctx);
            }
        }
        return matched;
    }

    bool test_match(Context& ctx) {
        if (!ctx.req) {
            return false;
        }
        
        for (auto& [rule, cb] : m_rules) {
            if (test_rule_match(rule.get(), ctx.req)) {
                return true;
            }
        }
        return false;
    }

private:
    bool test_rule_match(const Rule* rule, DBusMessage* msg) {
        // 检查消息类型
        int msg_type = dbus_message_get_type(msg);
        if (rule->type() == MessageType::signal && msg_type != DBUS_MESSAGE_TYPE_SIGNAL) {
            return false;
        }
        if (rule->type() == MessageType::method_call && msg_type != DBUS_MESSAGE_TYPE_METHOD_CALL) {
            return false;
        }
        if (rule->type() == MessageType::method_return && msg_type != DBUS_MESSAGE_TYPE_METHOD_RETURN) {
            return false;
        }
        if (rule->type() == MessageType::error && msg_type != DBUS_MESSAGE_TYPE_ERROR) {
            return false;
        }
        
        // 检查 interface
        const char* msg_interface = dbus_message_get_interface(msg);
        std::string_view rule_interface = rule->interface();
        if (!rule_interface.empty()) {
            if (!msg_interface || rule_interface != msg_interface) {
                return false;
            }
        }
        
        // 检查 member
        const char* msg_member = dbus_message_get_member(msg);
        std::string_view rule_member = rule->member();
        if (!rule_member.empty()) {
            if (!msg_member || rule_member != msg_member) {
                return false;
            }
        }
        
        // 检查 path
        const char* msg_path = dbus_message_get_path(msg);
        std::string_view rule_path = rule->path();
        if (!rule_path.empty()) {
            if (!msg_path) {
                return false;
            }
            if (rule->path_namespace()) {
                // path_namespace 匹配：消息路径必须以规则路径开头
                std::string_view msg_path_sv(msg_path);
                if (msg_path_sv.size() < rule_path.size() || 
                    msg_path_sv.substr(0, rule_path.size()) != rule_path) {
                    return false;
                }
            } else {
                // 精确 path 匹配
                if (rule_path != msg_path) {
                    return false;
                }
            }
        }
        
        // 检查 sender
        const char* msg_sender = dbus_message_get_sender(msg);
        std::string_view rule_sender = rule->sender();
        if (!rule_sender.empty()) {
            if (!msg_sender || rule_sender != msg_sender) {
                return false;
            }
        }
        
        // 检查 destination
        const char* msg_destination = dbus_message_get_destination(msg);
        std::string_view rule_destination = rule->destination();
        if (!rule_destination.empty()) {
            if (!msg_destination || rule_destination != msg_destination) {
                return false;
            }
        }
        
        return true;
    }

    std::vector<std::pair<RulePtr, std::function<void(Context&)>>> m_rules;
};

} // namespace Match
} // namespace DBus

namespace shm {
template <typename T>
using shared_ptr = std::shared_ptr<T>;

class shared_memory;
class message_queue_t {
public:
    bool pop_front(std::function<void(const std::string_view&, int)> handler, int timeout_ms,
                   int max_read_count, std::string& read_buf) {
        return true;
    }

    bool push_back(const std::string_view& data, int timeout_ms) {
        return true;
    }
};

class property {
public:
    property() : m_data(""), m_signature("") {
    }

    property(const std::string_view& signature) : m_data(""), m_signature(signature) {
    }

    void set_read_privilege(int read_privilege) {
    }

    void set_write_privilege(int write_privilege) {
    }

    void set_flags(int flags) {
    }

    void set_data(shared_memory& ins, const std::string_view& value) {
        m_data = std::string(value);
    }

    std::optional<std::string_view> get_data() {
        return m_data;
    }

    std::string_view get_signature() {
        return m_signature;
    }

    std::string m_data;
    std::string m_signature;
};

class method {
public:
    void set_privilege(int privilege) {
    }

    void set_flags(int flags) {
    }
};

class signal {
public:
    void set_flags(int flags) {
    }
};

class interface {
public:
    shared_ptr<property> add_p(shared_memory& ins, const std::string_view& name,
                               const std::string_view& signature) {
        return std::make_shared<property>(signature);
    }

    method& add_m(shared_memory& ins, const std::string_view& name,
                  const std::string_view& signature, const std::string_view& return_signature) {
        return m_method;
    }

    signal& add_s(shared_memory& ins, const std::string_view& name,
                  const std::string_view& signature) {
        return m_signal;
    }

    shared_ptr<property> find_p(const std::string_view& name) {
        return std::make_shared<property>();
    }

    method m_method;
    signal m_signal;
};

class object {
public:
    using interface_map_t = std::unordered_map<std::string_view, interface*>;

    interface& register_interface(shared_memory& ins, bool is_remote,
                                  const std::string_view& name) {
        auto p = new interface();
        m_interfaces.emplace(name, p);
        return *p;
    }

    void add_named_object_view(shared_memory& ins, const std::string_view& name) {
    }

    interface_map_t& interfaces() {
        return m_interfaces;
    }

    interface_map_t m_interfaces;
};

class object_tree {
public:
    object_tree() : m_unique_name("1.23"), m_harbor_name("") {
    }

    std::string_view unique_name() const {
        return m_unique_name;
    }

    std::string_view wellknow_name() const {
        return m_wellknow_name;
    }

    std::string_view harbor_name() const {
        return m_harbor_name;
    }

    void set_unique_name(const std::string_view& name) {
        m_unique_name = std::string(name);
    }

    void set_harbor_name(const std::string_view& name) {
        m_harbor_name = std::string(name);
    }

    object& register_object(shared_memory& ins, const std::string_view& napathme) {
        return m_object;
    }

    void unregister_object(shared_memory& ins, const std::string_view& name) {
    }

    message_queue_t& create_message_queue(shared_memory& ins, int size) {
        return m_queue;
    }

    message_queue_t* get_message_queue() {
        return &m_queue;
    }

    object* find_object(const std::string_view& path) {
        return &m_object;
    }

    std::string     m_unique_name;
    std::string     m_wellknow_name;
    std::string     m_harbor_name;
    message_queue_t m_queue;
    object          m_object;
};

class matchs {
public:
    using match_cb_t = std::function<void(DBus::Match::Context&, object_tree*)>;

    void run(DBus::Match::Context& ctx, match_cb_t cb) {
    }

    std::function<void()> add_rule(shared_memory& ins, DBus::Match::Rule& rule, object_tree* tree) {
        return []() {
        };
    }
};

struct shared_memory_base {
    matchs _matchs;
};

class shared_memory {
public:
    using tree_map_t        = std::unordered_map<std::string_view, object_tree*>;
    using unique_name_map_t = std::unordered_map<std::string, std::string>;

    shared_memory() {
    }

    static shared_memory& get_instance() {
        static shared_memory instance;
        return instance;
    }

    shm::shared_memory_base& get_base() {
        return m_base;
    }

    void lock() {
    }

    void unlock() {
    }

    void set_harbor_name(const std::string_view& name, const std::string_view& harbor_name) {
        unique_name_map[std::string(name)] = std::string(harbor_name);
    }

    std::string_view get_harbor_name(const std::string_view& name) {
        std::string_view harbor_name;
        if (tree_map.find(name) != tree_map.end()) {
            harbor_name = unique_name_map[std::string(tree_map[name]->unique_name())];
        } else {
            harbor_name = unique_name_map[std::string(name)];
        }
        if (harbor_name.empty()) {
            return "harbor.bmc.kepler.mock";
        }
        return harbor_name;
    }

    object_tree* get_tree(const std::string_view& name) {
        auto it = tree_map.find(name);
        if (it == tree_map.end()) {
            auto p   = new object_tree();
            auto ret = tree_map.emplace(name, p);
            it       = ret.first;
        }
        return it->second;
    }

    tree_map_t& get_object_tree_map(const std::string_view& harbor_name) {
        if (tree_map.find(harbor_name) == tree_map.end()) {
            auto p = new object_tree();
            tree_map.emplace(harbor_name, p);
        }
        return tree_map;
    }

    tree_map_t         tree_map;
    unique_name_map_t  unique_name_map;
    shared_memory_base m_base;
};

} // namespace shm

#endif // MC_DBUS_SHM_MOCK_SHM_H
