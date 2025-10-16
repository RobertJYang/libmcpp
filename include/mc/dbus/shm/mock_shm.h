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
    Rule() {
    }

    void member(const std::string_view& member) {
    }

    std::string_view member() const {
        return "";
    }

    void interface(const std::string_view& interface) {
    }

    std::string_view interface() const {
        return "";
    }

    void path(const std::string_view& path) {
    }

    std::string_view path() const {
        return "";
    }

    void path_namespace(const std::string_view& path_namespace) {
    }

    bool path_namespace() const {
        return false;
    }

    void sender(const std::string_view& sender) {
    }

    std::string_view sender() const {
        return "";
    }

    void type(MessageType type) {
    }

    MessageType type() const {
        return MessageType::signal;
    }

    bool is_connected() {
        return false;
    }

    void disconnect() {
    }

    void destination(const std::string_view& destination) {
    }

    std::string_view destination() const {
        return "";
    }

    std::string as_string() const {
        return "";
    }
};

using RulePtr = std::shared_ptr<Rule>;

struct Context {
    void set_req(DBusMessage* msg) {
    }

    DBusMessage* req;
};

class Matchs {
public:
    void add_rule(RulePtr& rule, std::function<void(Context&)> cb) {
    }

    bool run(Context& ctx) {
        return true;
    }

    bool test_match(Context& ctx) {
        return true;
    }
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
    object_tree() : m_unique_name("1.23"), m_harbor_name("harbor.bmc.kepler.mock") {
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
