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

#include <cctype>
#include <dbus/dbus.h>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

static constexpr int SHM_LOCK_TIMEOUT_DEFAULT_MS = 60000;

// mock 内部使用：大小写不敏感的字符串比较（用于 map/set 排序和异构查找）
inline bool str_less_i(std::string_view a, std::string_view b)
{
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        const char          la = static_cast<char>(std::tolower(ca));
        const char          lb = static_cast<char>(std::tolower(cb));
        if (la < lb) {
            return true;
        }
        if (la > lb) {
            return false;
        }
    }
    return a.size() < b.size();
}

namespace shmlock {

class LockHandle {
public:
    LockHandle()
    {
    }

    void release()
    {
    }
};

class ShmLockManager;
static std::mutex                      _shm_instance_mutex;
static std::unique_ptr<ShmLockManager> _shm_instance;

class ShmLockManager {
public:
    ShmLockManager()
    {
    }

    static ShmLockManager& get_instance()
    {
        std::lock_guard lock(_shm_instance_mutex);

        if (!_shm_instance) {
            _shm_instance = std::make_unique<ShmLockManager>();
        }

        return *_shm_instance;
    }

    static uint16_t allocate_service_id()
    {
        return 1;
    }

    LockHandle acquire_read_lock(uint64_t object_id, uint16_t service_id, uint32_t timeout_ms = SHM_LOCK_TIMEOUT_DEFAULT_MS)
    {
        return LockHandle();
    }

    LockHandle acquire_write_lock(uint64_t object_id, uint16_t service_id, uint32_t timeout_ms = SHM_LOCK_TIMEOUT_DEFAULT_MS)
    {
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
    Rule()
        : m_type(MessageType::signal), m_path_namespace(false)
    {
    }

    void member(const std::string_view& member)
    {
        m_member = std::string(member);
    }

    std::string_view member() const
    {
        return m_member;
    }

    void interface(const std::string_view& interface)
    {
        m_interface = std::string(interface);
    }

    std::string_view interface() const
    {
        return m_interface;
    }

    void path(const std::string_view& path)
    {
        m_path = std::string(path);
    }

    std::string_view path() const
    {
        return m_path;
    }

    void path_namespace(const std::string_view& path_namespace)
    {
        m_path           = std::string(path_namespace);
        m_path_namespace = true;
    }

    bool path_namespace() const
    {
        return m_path_namespace;
    }

    void sender(const std::string_view& sender)
    {
        m_sender = std::string(sender);
    }

    std::string_view sender() const
    {
        return m_sender;
    }

    void type(MessageType type)
    {
        m_type = type;
    }

    MessageType type() const
    {
        return m_type;
    }

    bool is_connected()
    {
        return false;
    }

    void disconnect()
    {
    }

    void destination(const std::string_view& destination)
    {
        m_destination = std::string(destination);
    }

    std::string_view destination() const
    {
        return m_destination;
    }

    std::string as_string() const
    {
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
            if (!result.empty()) {
                result += ",";
            }
            result += "interface='";
            result += m_interface;
            result += "'";
        }

        // member 字段
        if (!m_member.empty()) {
            if (!result.empty()) {
                result += ",";
            }
            result += "member='";
            result += m_member;
            result += "'";
        }

        // path 或 path_namespace 字段
        if (!m_path.empty()) {
            if (!result.empty()) {
                result += ",";
            }
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
            if (!result.empty()) {
                result += ",";
            }
            result += "sender='";
            result += m_sender;
            result += "'";
        }

        // destination 字段
        if (!m_destination.empty()) {
            if (!result.empty()) {
                result += ",";
            }
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
    bool        m_path_namespace;
    std::string m_sender;
    std::string m_destination;
};

using RulePtr = std::shared_ptr<Rule>;

struct Context {
    void set_req(DBusMessage* msg)
    {
        req = msg;
    }

    DBusMessage* req;
};

class Matchs {
public:
    void add_rule(RulePtr& rule, std::function<void(Context&)> cb)
    {
        m_rules.push_back({rule, cb});
    }

    bool run(Context& ctx)
    {
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

    bool test_match(Context& ctx)
    {
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
    bool test_rule_match(const Rule* rule, DBusMessage* msg)
    {
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
        const char*      msg_interface  = dbus_message_get_interface(msg);
        std::string_view rule_interface = rule->interface();
        if (!rule_interface.empty()) {
            if (!msg_interface || rule_interface != msg_interface) {
                return false;
            }
        }

        // 检查 member
        const char*      msg_member  = dbus_message_get_member(msg);
        std::string_view rule_member = rule->member();
        if (!rule_member.empty()) {
            if (!msg_member || rule_member != msg_member) {
                return false;
            }
        }

        // 检查 path
        const char*      msg_path  = dbus_message_get_path(msg);
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
        const char*      msg_sender  = dbus_message_get_sender(msg);
        std::string_view rule_sender = rule->sender();
        if (!rule_sender.empty()) {
            if (!msg_sender || rule_sender != msg_sender) {
                return false;
            }
        }

        // 检查 destination
        const char*      msg_destination  = dbus_message_get_destination(msg);
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
template <typename T>
using weak_ptr = std::weak_ptr<T>;

class shared_memory;
class object_tree;
class message_queue_t {
public:
    bool pop_front(std::function<void(const std::string_view&, int)> handler, int timeout_ms,
                   int max_read_count, std::string& read_buf)
    {
        return true;
    }

    bool push_back(const std::string_view& data, int timeout_ms)
    {
        return true;
    }
};

class property {
public:
    property()
        : m_data(""), m_signature("")
    {
    }

    property(const std::string_view& signature)
        : m_data(""), m_signature(signature)
    {
    }

    void set_read_privilege(int read_privilege)
    {
    }

    void set_write_privilege(int write_privilege)
    {
    }

    void set_flags(int flags)
    {
    }

    void set_data(shared_memory& ins, const std::string_view& value)
    {
        m_data = std::string(value);
    }

    std::optional<std::string_view> get_data() const
    {
        return m_data;
    }

    std::string_view get_signature() const
    {
        return m_signature;
    }

    std::string m_data;
    std::string m_signature;
};

class method {
public:
    void set_privilege(int privilege)
    {
    }

    void set_flags(int flags)
    {
    }
};

class signal {
public:
    void set_flags(int flags)
    {
    }
};

class interface {
public:
    using properties_t = std::unordered_map<std::string, shared_ptr<property>>;

    shared_ptr<property> add_p(shared_memory& ins, const std::string_view& name,
                               const std::string_view& signature)
    {
        auto p = std::make_shared<property>(signature);
        m_properties.emplace(std::string(name), p);
        return p;
    }

    method& add_m(shared_memory& ins, const std::string_view& name,
                  const std::string_view& signature, const std::string_view& return_signature)
    {
        return m_method;
    }

    signal& add_s(shared_memory& ins, const std::string_view& name,
                  const std::string_view& signature)
    {
        return m_signal;
    }

    shared_ptr<property> find_p(const std::string_view& name)
    {
        auto it = m_properties.find(std::string(name));
        if (it == m_properties.end()) {
            return nullptr;
        }
        return it->second;
    }

    properties_t& properties()
    {
        return m_properties;
    }

    method       m_method;
    signal       m_signal;
    properties_t m_properties;
};

class object {
public:
    using interface_map_t = std::unordered_map<std::string_view, interface*>;

    interface& register_interface(shared_memory& ins, bool is_remote,
                                  const std::string_view& name)
    {
        auto p = new interface();
        m_interfaces.emplace(name, p);
        return *p;
    }

    void add_named_object_view(shared_memory& ins, const std::string_view& name)
    {
    }

    interface_map_t& interfaces()
    {
        return m_interfaces;
    }

    std::string_view path() const
    {
        return m_path;
    }

    object_tree* get_tree()
    {
        return m_tree;
    }

    interface_map_t m_interfaces;
    std::string     m_path;
    object_tree*    m_tree{nullptr};
};

class object_tree {
public:
    object_tree()
        : m_unique_name("1.23"), m_harbor_name("")
    {
    }

    std::string_view unique_name() const
    {
        return m_unique_name;
    }

    std::string_view wellknow_name() const
    {
        return m_wellknow_name;
    }

    std::string_view harbor_name() const
    {
        return m_harbor_name;
    }

    void set_unique_name(const std::string_view& name)
    {
        m_unique_name = std::string(name);
    }

    void set_harbor_name(const std::string_view& name)
    {
        m_harbor_name = std::string(name);
    }

    object& register_object(shared_memory& ins, const std::string_view& napathme)
    {
        return m_object;
    }

    void unregister_object(shared_memory& ins, const std::string_view& name)
    {
    }

    message_queue_t& create_message_queue(shared_memory& ins, int size)
    {
        return m_queue;
    }

    message_queue_t* get_message_queue()
    {
        return &m_queue;
    }

    object* find_object(const std::string_view& path)
    {
        return &m_object;
    }

    template <typename Func>
    bool travel(Func&& f, int depth, bool leaf_only)
    {
        (void)depth;
        if (!leaf_only || !m_object.m_interfaces.empty()) {
            if (!f(m_object, 0)) {
                return false;
            }
        }
        return true;
    }

    std::string     m_unique_name;
    std::string     m_wellknow_name;
    std::string     m_harbor_name;
    message_queue_t m_queue;
    object          m_object;
};

// 允许 std::map 使用 std::string_view 进行异构查找（贴近原实现 str_less 的语义）
// 这里保持大小写不敏感比较，并支持 std::string_view 异构查找
struct str_less {
    using is_transparent = void;

    bool operator()(const std::string& lhs, const std::string& rhs) const
    {
        return str_less_i(std::string_view(lhs), std::string_view(rhs));
    }
    bool operator()(std::string_view lhs, std::string_view rhs) const
    {
        return str_less_i(lhs, rhs);
    }
    bool operator()(const std::string& lhs, std::string_view rhs) const
    {
        return str_less_i(std::string_view(lhs), rhs);
    }
    bool operator()(std::string_view lhs, const std::string& rhs) const
    {
        return str_less_i(lhs, std::string_view(rhs));
    }
};

// 与真实实现保持语义一致：well-known / unique 名称到 object_tree 的映射
// 原实现为 bip::map<shared_string, object_tree_ptr>，这里用 std::map 打桩其语义与遍历行为
using object_tree_map_t = std::map<std::string, object_tree*, str_less>;

class matchs {
public:
    using match_cb_t = std::function<void(DBus::Match::Context&, object_tree*)>;

    void run(DBus::Match::Context& ctx, match_cb_t cb)
    {
    }

    std::function<void()> add_rule(shared_memory& ins, DBus::Match::Rule& rule, object_tree* tree)
    {
        return []() {
        };
    }
};

// mdb_interface 结构体定义
// 注意：实际共享内存实现中，o 和 i 是 bip::offset_ptr<object> 和 bip::offset_ptr<interface> 类型的成员变量
// 在 mock 版本中使用原始指针来模拟
struct mdb_interface {
    object*    o{nullptr};
    interface* i{nullptr};
};

// mdb_object 类定义
class mdb_object {
public:
    mdb_object(shared_memory& shm, const std::string_view& path, void* t)
        : m_path(path)
    {
    }

    using mdb_interfaces     = std::unordered_map<std::string, mdb_interface>;
    using mdb_interfaces_all = std::unordered_map<std::string, mdb_interfaces>;

    mdb_interfaces* find_interfaces(const std::string_view& interface)
    {
        auto it = m_interfaces.find(std::string(interface));
        if (it != m_interfaces.end()) {
            return &it->second;
        }
        return nullptr;
    }

    bool empty() const
    {
        return m_interfaces.empty();
    }

    mdb_interfaces_all& interfaces()
    {
        return m_interfaces;
    }

    std::string_view path()
    {
        return m_path;
    }

private:
    mdb_interfaces_all m_interfaces;
    std::string        m_path;
};

// mdb_objects_tree 树结构定义
class mdb_objects_tree {
public:
    using travel_cb_t = std::function<bool(mdb_object&, int level)>;

    mdb_objects_tree()
    {
    }

    template <typename Func>
    void travel(Func&& f, uint32_t depth, bool)
    {
        // 打桩实现：不执行任何操作
    }

    mdb_objects_tree* find_node(const std::string_view& path, bool ignore_case)
    {
        // 打桩实现：返回 nullptr
        return nullptr;
    }
};

struct shared_memory_base {
    // well-known 名称映射
    object_tree_map_t wellknow_names;
    // unique 名称映射（这里只保留结构，不关心具体语义）
    object_tree_map_t unique_names;

    using object_ptr       = object*;
    using object_map_t     = std::map<std::string, object_ptr, str_less>;
    using twi_object_map_t = std::map<std::string, object_map_t, str_less>;
    using tri_object_map_t = std::map<std::string, twi_object_map_t, str_less>;

    tri_object_map_t object_map;
    twi_object_map_t object_names;
    matchs           _matchs;
    mdb_objects_tree objects;
};

class shared_memory {
public:
    using tree_map_t        = std::unordered_map<std::string_view, object_tree*>;
    using unique_name_map_t = std::unordered_map<std::string, std::string>;

    shared_memory()
    {
    }

    static shared_memory& get_instance()
    {
        static shared_memory instance;
        return instance;
    }

    shm::shared_memory_base& get_base()
    {
        return m_base;
    }

    const shm::shared_memory_base& get_base() const
    {
        return m_base;
    }

    // 打桩实现：返回 well-known 名称到 object_tree 的映射
    object_tree_map_t& get_wellknow_names()
    {
        return m_base.wellknow_names;
    }

    void lock()
    {
    }

    void unlock()
    {
    }

    void lock_shared()
    {
    }

    void unlock_shared()
    {
    }

    void set_harbor_name(const std::string_view& name, const std::string_view& harbor_name)
    {
        unique_name_map[std::string(name)] = std::string(harbor_name);
    }

    std::string_view get_harbor_name(const std::string_view& name)
    {
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

    object_tree* get_tree(const std::string_view& name)
    {
        auto it = tree_map.find(name);
        if (it == tree_map.end()) {
            auto p   = new object_tree();
            auto ret = tree_map.emplace(name, p);
            it       = ret.first;
        }
        return it->second;
    }

    tree_map_t& get_object_tree_map(const std::string_view& harbor_name)
    {
        if (tree_map.find(harbor_name) == tree_map.end()) {
            auto p = new object_tree();
            tree_map.emplace(harbor_name, p);
        }
        return tree_map;
    }

    mdb_object* find_mdb_object(const std::string_view& path, bool ignore_case)
    {
        // 打桩实现：返回 nullptr
        return nullptr;
    }

    using query_interface_view_t = std::function<bool(mdb_interface&)>;
    mdb_interface* query_interface_view(const std::string_view&       iface_name,
                                        const query_interface_view_t& filter)
    {
        // 打桩实现：返回 nullptr
        return nullptr;
    }

    tree_map_t         tree_map;
    unique_name_map_t  unique_name_map;
    shared_memory_base m_base;
};

} // namespace shm

#endif // MC_DBUS_SHM_MOCK_SHM_H
