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

#include <mc/mdb_service.h>

#include <mc/algorithm/lru_cache.h>
#include <mc/dbus/match.h>
#include <mc/dbus/sd_bus.h>
#include <mc/dbus/shm/harbor.h>
#include <mc/exception.h>
#include <mc/json.h>
#include <mc/log.h>
#include <mc/variant.h>

#include <atomic>
#include <optional>

namespace mc::mdb::service {

namespace {
constexpr mc::milliseconds MDB_SERVICE_TIMEOUT   = mc::milliseconds(10 * 60 * 1000);
constexpr std::string_view MDB_SERVICE_NAME      = "bmc.kepler.maca";
constexpr std::string_view MDB_SERVICE_PATH      = "/bmc/kepler/MdbService";
constexpr std::string_view MDB_SERVICE_INTERFACE = "bmc.kepler.Mdb";

// 缓存条目结构
struct cache_entry {
    std::string path;
    std::string interface;
    std::string service_name;
    std::string filter;
    bool        ignore_case;
    mc::dict    filter_dict;
};

// 信号订阅共享结构
struct signal_subscription {
    mc::dbus::sd_bus* bus; // 裸指针 - 生命周期由调用方管理
    uint64_t          interfaces_removed_match_id{0};
    uint64_t          properties_changed_match_id{0};
    size_t            ref_count{0};
};

// 类型别名
using signal_subscription_map    = std::unordered_map<std::string, signal_subscription>;
using creating_subscriptions_map = std::unordered_map<std::string, std::shared_ptr<std::condition_variable_any>>;
using creating_cache_map         = std::unordered_map<std::string, std::shared_ptr<std::condition_variable_any>>;

// 跟踪正在创建的订阅（key -> condition_variable）
std::unordered_map<std::string, std::shared_ptr<std::condition_variable_any>>& get_creating_subscriptions()
{
    static std::unordered_map<std::string, std::shared_ptr<std::condition_variable_any>> s_creating;
    return s_creating;
}

// 跟踪正在创建的缓存条目（cache_key -> condition_variable）
std::unordered_map<std::string, std::shared_ptr<std::condition_variable_any>>& get_creating_cache_entries()
{
    static std::unordered_map<std::string, std::shared_ptr<std::condition_variable_any>> s_creating;
    return s_creating;
}

// 前向声明
void remove_from_indices(const std::string& cache_key, const std::string& path, const std::string& interface,
                         const std::string& service_name);
void decrease_subscription_refcount(const std::string& path, const std::string& interface);
void add_to_indices(const std::string& cache_key, const std::string& path, const std::string& interface,
                    const std::string& service_name);

// 延迟清理队列：存储需要清理的订阅信息
// 使用线程局部变量，避免跨线程竞争
thread_local std::vector<std::pair<std::string, std::string>> t_pending_cleanup;

// 淘汰回调函数：清理被淘汰条目的资源
void on_cache_evict(const std::string& cache_key, cache_entry&& entry)
{
    try {
        // 从索引中移除（已在锁内，不需要再加锁）
        remove_from_indices(cache_key, entry.path, entry.interface, entry.service_name);

        // 将需要清理的订阅信息放入线程局部队列
        // 稍后在锁外统一处理
        t_pending_cleanup.emplace_back(entry.path, entry.interface);
    } catch (const std::exception& e) {
        // 淘汰回调不能抛出异常，否则会导致 lru_cache 状态不一致
        // 最坏情况是订阅引用计数泄漏，但不会破坏缓存结构
        elog("Cache eviction callback failed: cache_key=${cache_key}, error=${error}",
             ("cache_key", cache_key)("error", e.what()));
    }
}

// 处理延迟清理队列
void process_pending_cleanup()
{
    if (t_pending_cleanup.empty()) {
        return;
    }

    // 取出所有待清理任务
    auto cleanup_tasks = std::move(t_pending_cleanup);
    t_pending_cleanup.clear();

    // 在锁外处理订阅引用计数减少
    for (const auto& [path, interface] : cleanup_tasks) {
        decrease_subscription_refcount(path, interface);
    }
}

// 全局数据（单例模式，线程安全初始化）
mc::algorithm::lru_cache<std::string, cache_entry>& get_path_cache()
{
    static mc::algorithm::lru_cache<std::string, cache_entry> s_cache(400, on_cache_evict);
    return s_cache;
}

std::unordered_map<std::string, signal_subscription>& get_signal_subscriptions()
{
    static std::unordered_map<std::string, signal_subscription> s_subs;
    return s_subs;
}

// 反向索引
std::unordered_map<std::string, std::unordered_set<std::string>>& get_subscription_index()
{
    static std::unordered_map<std::string, std::unordered_set<std::string>> s_index;
    return s_index;
}

std::unordered_map<std::string, std::unordered_set<std::string>>& get_service_index()
{
    static std::unordered_map<std::string, std::unordered_set<std::string>> s_index;
    return s_index;
}

// 将 sd_bus 返回的 variants 转换为单个 variant
// 约定：0 个返回值 => 空 variant；1 个返回值 => 直接返回该值；多个返回值 => 作为数组返回
static mc::variant convert_method_result(const mc::variants& arr)
{
    if (arr.empty()) {
        return mc::variant();
    }
    if (arr.size() == 1) {
        return arr[0];
    }
    return arr;
}

// 使用 recursive_mutex 允许同一线程重入
// 原因：D-Bus同步调用期间可能触发信号回调，回调中可能需要访问缓存
std::recursive_mutex& get_cache_mutex()
{
    static std::recursive_mutex s_mutex;
    return s_mutex;
}

// 保护信号订阅 map 的互斥锁
// 使用 recursive_mutex 避免信号回调中的死锁
std::recursive_mutex& get_subscriptions_mutex()
{
    static std::recursive_mutex s_mutex;
    return s_mutex;
}

// 全局 NameOwnerChanged 订阅管理
struct global_name_owner_subscription {
    mc::dbus::connection conn; // 保存 connection 对象的拷贝（使用 shared_ptr 管理，生命周期安全）
    uint64_t             match_id{0};
    std::recursive_mutex mutex; // 使用 recursive_mutex 避免信号回调中的死锁
};

global_name_owner_subscription& get_global_name_owner_subscription()
{
    static global_name_owner_subscription s_sub;
    return s_sub;
}

// 前向声明

// 构造订阅key
std::string make_subscription_key(const std::string& path, const std::string& interface)
{
    return path + ":" + interface;
}

// 调用GetPath方法，返回{path, service_name}
std::pair<std::string, std::string> call_get_path(mc::dbus::sd_bus* bus, std::string_view interface,
                                                  std::string_view filter, bool ignore_case)
{
    auto results = bus->timeout_call(MDB_SERVICE_TIMEOUT, {MDB_SERVICE_NAME,
                                                           MDB_SERVICE_PATH,
                                                           MDB_SERVICE_INTERFACE,
                                                           "GetPath",
                                                           "a{ss}ssb",
                                                           {mc::dict(), interface, filter, ignore_case}});

    auto result = convert_method_result(results);
    if (result.is_array()) {
        auto arr = result.as<mc::variants>();
        if (arr.size() >= 2) {
            return {arr[0].as_string(), arr[1].as_string()};
        }
    }
    return {result.as_string(), ""};
}

// 比较属性名（考虑ignore_case）
bool match_property_name(const std::string& prop_name, const std::string& filter_key, bool ignore_case)
{
    if (!ignore_case) {
        return prop_name == filter_key;
    }

    if (prop_name.length() != filter_key.length()) {
        return false;
    }

    return std::equal(prop_name.begin(), prop_name.end(), filter_key.begin(), [](char a, char b) {
        return std::tolower(a) == std::tolower(b);
    });
}

// 检查filter中是否有任意属性变更
bool any_filter_property_changed(const mc::dict& filter_dict, const mc::dict& changed_props, bool ignore_case)
{
    if (filter_dict.empty()) {
        return false;
    }

    for (const auto& filter_entry : filter_dict) {
        auto filter_key = filter_entry.key.as_string();
        for (const auto& prop_entry : changed_props) {
            auto prop_name = prop_entry.key.as_string();
            if (match_property_name(prop_name, filter_key, ignore_case)) {
                return true;
            }
        }
    }
    return false;
}

// 添加到索引（需在持有锁的情况下调用）
void add_to_indices(const std::string& cache_key, const std::string& path, const std::string& interface,
                    const std::string& service_name)
{
    std::string sub_key = make_subscription_key(path, interface);
    get_subscription_index()[sub_key].insert(cache_key);

    if (!service_name.empty()) {
        get_service_index()[service_name].insert(cache_key);
    }
}

// 从索引中移除（需在持有锁的情况下调用）
void remove_from_indices(const std::string& cache_key, const std::string& path, const std::string& interface,
                         const std::string& service_name)
{
    std::string sub_key   = make_subscription_key(path, interface);
    auto&       sub_index = get_subscription_index();
    auto        sub_it    = sub_index.find(sub_key);
    if (sub_it != sub_index.end()) {
        sub_it->second.erase(cache_key);
        if (sub_it->second.empty()) {
            sub_index.erase(sub_it);
        }
    }

    if (!service_name.empty()) {
        auto& svc_index = get_service_index();
        auto  svc_it    = svc_index.find(service_name);
        if (svc_it != svc_index.end()) {
            svc_it->second.erase(cache_key);
            if (svc_it->second.empty()) {
                svc_index.erase(svc_it);
            }
        }
    }
}

// 减少订阅引用计数并在必要时移除
void decrease_subscription_refcount(const std::string& path, const std::string& interface)
{
    std::string sub_key = make_subscription_key(path, interface);

    // 需要先获取锁，因为可能同时有多个线程在操作 subs
    std::unique_lock<std::recursive_mutex> lock(get_subscriptions_mutex());

    auto& subs   = get_signal_subscriptions();
    auto  sub_it = subs.find(sub_key);
    if (sub_it == subs.end() || sub_it->second.ref_count == 0) {
        return;
    }

    sub_it->second.ref_count--;

    if (sub_it->second.ref_count == 0) {
        auto& sub = sub_it->second;
        // 在锁外调用 remove_match，避免在持有锁时调用可能阻塞的 D-Bus 操作
        // 先保存需要移除的 match_id 和 bus 指针
        uint64_t          interfaces_removed_match_id = sub.interfaces_removed_match_id;
        uint64_t          properties_changed_match_id = sub.properties_changed_match_id;
        mc::dbus::sd_bus* bus_ptr                     = sub.bus; // 保存 bus 指针

        // 从 map 中移除（在锁内）
        subs.erase(sub_it);

        // 释放锁后再调用 remove_match
        lock.unlock();

        try {
            // 使用保存的 bus 指针来移除订阅
            // 注意：NameOwnerChanged 是全局订阅，不在此处移除
            // 注意：如果 match_id 为 0，说明 add_match 时失败了，不需要调用 remove_match
            if (interfaces_removed_match_id > 0 && bus_ptr) {
                bus_ptr->remove_match(interfaces_removed_match_id);
            }
            if (properties_changed_match_id > 0 && bus_ptr) {
                bus_ptr->remove_match(properties_changed_match_id);
            }
        } catch (const std::exception& e) {
            elog("Failed to remove signal subscription: ${error}", ("error", e.what()));
        }
    }
}

// 清理单个缓存条目（需在持有锁的情况下调用）
// 注意：lru_cache::erase 不会触发 on_cache_evict 回调，需要手动处理清理逻辑
void cleanup_cache_entry_locked(const std::string& cache_key)
{
    auto& cache = get_path_cache();

    // 先获取缓存条目，以便清理索引和订阅
    auto val = cache.get(cache_key);
    if (!val.has_value()) {
        return;
    }

    // 复制条目信息（因为 erase 后引用失效）
    auto entry = val->get();

    // 先从缓存中删除（确保后续操作失败时缓存已经清理）
    cache.erase(cache_key);

    try {
        // 从索引中移除
        remove_from_indices(cache_key, entry.path, entry.interface, entry.service_name);

        // 将需要清理的订阅信息放入线程局部队列
        t_pending_cleanup.emplace_back(entry.path, entry.interface);
    } catch (const std::exception& e) {
        // 索引清理失败不应影响缓存删除
        // 最坏情况是索引残留，但不会导致缓存不一致
        elog("Cache entry cleanup failed: cache_key=${cache_key}, error=${error}",
             ("cache_key", cache_key)("error", e.what()));
    }
}

// 清理单个缓存条目并减少订阅引用
void cleanup_cache_entry(const std::string& cache_key)
{
    {
        std::lock_guard<std::recursive_mutex> lock(get_cache_mutex());
        cleanup_cache_entry_locked(cache_key);
    }
    // 注意：如果是手动调用 cleanup_cache_entry 且之后没有批量处理，
    // 调用者应该负责调用 process_pending_cleanup()。
    // 在信号处理函数中，我们已经在循环外添加了该调用。
}

// 处理服务退出信号
void handle_name_owner_changed(const std::string& name, const std::string& new_owner)
{
    if (!new_owner.empty()) {
        return;
    }

    std::vector<std::string> keys_to_remove;
    {
        std::lock_guard<std::recursive_mutex> lock(get_cache_mutex());
        auto&                                 svc_index = get_service_index();
        auto                                  svc_it    = svc_index.find(name);
        if (svc_it != svc_index.end()) {
            keys_to_remove.assign(svc_it->second.begin(), svc_it->second.end());
        }
    }

    if (!keys_to_remove.empty()) {
        try {
            for (const auto& key : keys_to_remove) {
                cleanup_cache_entry(key);
            }
        } catch (const std::exception& e) {
            elog("Handle NameOwnerChanged failed: name=${name}, error=${error}", ("name", name)("error", e.what()));
        }
        // 无论是否有异常，都要处理清理任务，避免订阅引用计数泄漏
        process_pending_cleanup();
    }
}

// 收集指定接口的所有缓存key
std::vector<std::string> collect_cache_keys_for_interface(const std::string& path, const std::string& interface)
{
    std::vector<std::string>              keys_to_remove;
    std::lock_guard<std::recursive_mutex> lock(get_cache_mutex());
    auto&                                 sub_index = get_subscription_index();
    std::string                           sub_key   = make_subscription_key(path, interface);
    auto                                  sub_it    = sub_index.find(sub_key);
    if (sub_it != sub_index.end()) {
        keys_to_remove.assign(sub_it->second.begin(), sub_it->second.end());
    }

    return keys_to_remove;
}

// 清理指定接口的缓存条目
bool cleanup_interface_cache_entries(const std::string& path, const std::string& interface)
{
    auto keys_to_remove = collect_cache_keys_for_interface(path, interface);
    if (keys_to_remove.empty()) {
        return false;
    }

    try {
        for (const auto& key : keys_to_remove) {
            cleanup_cache_entry(key);
        }
    } catch (const std::exception& e) {
        elog("Handle InterfacesRemoved failed: path=${path}, interface=${interface}, error=${error}",
             ("path", path)("interface", interface)("error", e.what()));
    }

    return true;
}

// 处理接口移除信号
void handle_interfaces_removed(const std::string& path, const mc::variants& interfaces)
{
    bool has_removed = false;

    for (const auto& intf_var : interfaces) {
        std::string interface = intf_var.as_string();
        if (cleanup_interface_cache_entries(path, interface)) {
            has_removed = true;
        }
    }

    if (has_removed) {
        process_pending_cleanup();
    }
}

// 检查缓存条目是否需要因属性变更而失效
bool should_invalidate_cache_entry(const cache_entry& entry, const mc::dict& changed_props)
{
    return any_filter_property_changed(entry.filter_dict, changed_props, entry.ignore_case);
}

// 收集需要失效的缓存条目
std::vector<std::string> collect_invalidated_cache_keys(const std::string& path, const std::string& interface,
                                                        const mc::dict& changed_props)
{
    std::vector<std::string>              keys_to_remove;
    std::lock_guard<std::recursive_mutex> lock(get_cache_mutex());
    auto&                                 sub_index = get_subscription_index();
    auto&                                 cache     = get_path_cache();
    std::string                           sub_key   = make_subscription_key(path, interface);
    auto                                  sub_it    = sub_index.find(sub_key);
    if (sub_it == sub_index.end()) {
        return keys_to_remove;
    }

    for (const auto& cache_key : sub_it->second) {
        auto val = cache.get(cache_key);
        if (!val.has_value()) {
            continue;
        }
        if (should_invalidate_cache_entry(val->get(), changed_props)) {
            keys_to_remove.push_back(cache_key);
        }
    }

    return keys_to_remove;
}

// 处理属性变更信号
void handle_properties_changed(const std::string& path, const std::string& interface, const mc::dict& changed_props)
{
    auto keys_to_remove = collect_invalidated_cache_keys(path, interface, changed_props);
    if (keys_to_remove.empty()) {
        return;
    }

    try {
        for (const auto& key : keys_to_remove) {
            cleanup_cache_entry(key);
        }
    } catch (const std::exception& e) {
        elog("Handle PropertiesChanged failed: path=${path}, interface=${interface}, error=${error}",
             ("path", path)("interface", interface)("error", e.what()));
    }

    process_pending_cleanup();
}

// 创建NameOwnerChanged信号订阅
// 确保全局 NameOwnerChanged 订阅已创建（单例模式）
void ensure_global_name_owner_subscription(mc::dbus::sd_bus* bus)
{
    auto&                                 global_sub = get_global_name_owner_subscription();
    std::lock_guard<std::recursive_mutex> lock(global_sub.mutex);

    // 已经订阅过，直接返回
    if (global_sub.match_id != 0) {
        return;
    }

    auto rule = mc::dbus::match_rule::new_signal("NameOwnerChanged", "org.freedesktop.DBus");

    uint64_t match_id = bus->add_match(rule, [bus](mc::dbus::message& msg) {
        try {
            auto args = msg.read_args();
            if (args.size() >= 3) {
                std::string name      = args[0].as_string();
                std::string new_owner = args[2].as_string();
                handle_name_owner_changed(name, new_owner);
            }
        } catch (const std::exception& e) {
            elog("Failed to subscribe NameOwnerChanged signal: ${error}", ("error", e.what()));
        }
    });

    global_sub.match_id = match_id;
}

// 创建InterfacesRemoved信号订阅
uint64_t create_interfaces_removed_subscription(mc::dbus::sd_bus* bus, const std::string& path)
{
    auto rule = mc::dbus::match_rule::new_signal("InterfacesRemoved", mc::dbus::DBUS_OBJECT_MANAGER_INTERFACE.data());
    rule.with_path(path);

    uint64_t match_id = bus->add_match(rule, [bus, path](mc::dbus::message& msg) {
        try {
            auto args = msg.read_args();
            if (args.size() >= 2) {
                std::string removed_path = args[0].as_string();
                if (removed_path == path) {
                    auto interfaces = args[1].as<mc::variants>();
                    handle_interfaces_removed(path, interfaces);
                }
            }
        } catch (const std::exception& e) {
            elog("Failed to subscribe InterfacesRemoved signal: ${error}", ("error", e.what()));
        }
    });

    return match_id;
}

// 创建PropertiesChanged信号订阅
uint64_t create_properties_changed_subscription(mc::dbus::sd_bus* bus, const std::string& path,
                                                const std::string& interface)
{
    auto rule = mc::dbus::match_rule::new_signal("PropertiesChanged", mc::dbus::DBUS_PROPERTIES_INTERFACE.data());
    rule.with_path(path);

    uint64_t match_id = bus->add_match(rule, [bus, path, interface](mc::dbus::message& msg) {
        try {
            auto args = msg.read_args();
            if (args.size() >= 2) {
                auto changed_props = args[1].as_dict();
                handle_properties_changed(path, interface, changed_props);
            }
        } catch (const std::exception& e) {
            elog("Failed to subscribe PropertiesChanged signal: ${error}", ("error", e.what()));
        }
    });

    return match_id;
}

// 等待其他线程完成创建或增加现有订阅的引用计数
// 返回值：true - 找到现有订阅并增加引用计数
//        false - 需要当前线程创建新订阅（包括超时情况）
bool try_increment_existing_subscription(const std::string& sub_key, std::unique_lock<std::recursive_mutex>& lock,
                                         signal_subscription_map& subs, creating_subscriptions_map& creating)
{
    constexpr int MAX_WAIT_ATTEMPTS = 100; // 最大等待次数
    int           wait_count        = 0;

    while (true) {
        auto sub_it = subs.find(sub_key);
        if (sub_it != subs.end()) {
            sub_it->second.ref_count++;
            return true;
        }

        auto creating_it = creating.find(sub_key);
        if (creating_it == creating.end()) {
            return false; // 没有其他线程在创建，需要当前线程创建
        }

        // 检查等待次数
        if (++wait_count > MAX_WAIT_ATTEMPTS) {
            wlog("Subscription creation wait timeout: sub_key=${sub_key}, wait_count=${count}, will create new "
                 "subscription",
                 ("sub_key", sub_key)("count", wait_count));
            // 超时后不再等待，直接自己创建（可能与其他线程重复创建，但通过 ref_count 机制最终会合并）
            return false;
        }

        auto cv = creating_it->second;
        cv->wait(lock);
    }
}

// 创建新的信号订阅
signal_subscription create_new_subscription(mc::dbus::sd_bus* bus, const std::string& path,
                                            const std::string& interface)
{
    signal_subscription sub;
    sub.bus       = bus; // 保存 bus 指针
    sub.ref_count = 1;

    sub.interfaces_removed_match_id = create_interfaces_removed_subscription(bus, path);

    try {
        sub.properties_changed_match_id = create_properties_changed_subscription(bus, path, interface);
    } catch (const std::exception& e) {
        elog("create_new_subscription: create_properties_changed_subscription FAILED: ${error}", ("error", e.what()));
        // 需要清理已创建的 interfaces_removed 订阅
        try {
            if (sub.interfaces_removed_match_id > 0) {
                auto& harbor = mc::dbus::harbor::get_instance();
                harbor.remove_match(sub.interfaces_removed_match_id);
            }
        } catch (const std::exception& cleanup_err) {
            elog("create_new_subscription: cleanup failed: ${error}", ("error", cleanup_err.what()));
        }
        throw;
    }

    return sub;
}

// 设置信号监听（支持订阅合并）
void setup_signal_watch(mc::dbus::sd_bus* bus, const std::string& path, const std::string& interface)
{
    ensure_global_name_owner_subscription(bus);

    std::string                            sub_key = make_subscription_key(path, interface);
    std::unique_lock<std::recursive_mutex> lock(get_subscriptions_mutex());
    auto&                                  subs     = get_signal_subscriptions();
    auto&                                  creating = get_creating_subscriptions();

    // 需要创建订阅的condition_variable，在try_increment_existing_subscription之前创建
    auto cv = std::make_shared<std::condition_variable_any>();

    // 尝试使用现有订阅或等待其他线程创建完成
    if (try_increment_existing_subscription(sub_key, lock, subs, creating)) {
        return;
    }

    // 标记为正在创建，原子性地注册到creating map
    // 此时仍然持有锁，避免竞态条件
    creating[sub_key] = cv;
    lock.unlock();

    // 清理函数：从creating map中移除并通知等待线程（需在持有锁时调用）
    auto cleanup_locked = [&]() {
        auto it = creating.find(sub_key);
        if (it != creating.end() && it->second == cv) {
            creating.erase(it);
        }
        cv->notify_all();
    };

    // 清理函数：获取锁后清理（用于异常路径）
    auto cleanup = [&]() {
        lock.lock();
        cleanup_locked();
        lock.unlock();
    };

    try {
        auto sub = create_new_subscription(bus, path, interface);
        lock.lock();
        subs[sub_key] = std::move(sub);
        cleanup_locked(); // 使用 cleanup_locked，因为此时已经持有锁
    } catch (...) {
        cleanup(); // 使用 cleanup，因为此时未持有锁
        throw;
    }
}

} // namespace

// 公有函数实现
mc::variant get_object(mc::dbus::sd_bus* bus, std::string_view path, const mc::variants& interfaces)
{
    auto results = bus->timeout_call(MDB_SERVICE_TIMEOUT, {MDB_SERVICE_NAME,
                                                           MDB_SERVICE_PATH,
                                                           MDB_SERVICE_INTERFACE,
                                                           "GetObject",
                                                           "a{ss}sas",
                                                           {mc::dict(), path, interfaces}});
    return convert_method_result(results);
}

mc::variant get_sub_objects(mc::dbus::sd_bus* bus, std::string_view path, int32_t depth, const mc::variants& interfaces)
{
    auto results = bus->timeout_call(MDB_SERVICE_TIMEOUT, {MDB_SERVICE_NAME,
                                                           MDB_SERVICE_PATH,
                                                           MDB_SERVICE_INTERFACE,
                                                           "GetSubObjects",
                                                           "a{ss}sias",
                                                           {mc::dict(), path, depth, interfaces}});
    return convert_method_result(results);
}

mc::variant get_sub_paths(mc::dbus::sd_bus* bus, std::string_view path, int32_t depth, const mc::variants& interfaces)
{
    auto results = bus->timeout_call(MDB_SERVICE_TIMEOUT, {MDB_SERVICE_NAME,
                                                           MDB_SERVICE_PATH,
                                                           MDB_SERVICE_INTERFACE,
                                                           "GetSubPaths",
                                                           "a{ss}sias",
                                                           {mc::dict(), path, depth, interfaces}});
    return convert_method_result(results);
}

mc::variant get_parent_objects(mc::dbus::sd_bus* bus, std::string_view path, const mc::variants& interfaces)
{
    auto results = bus->timeout_call(MDB_SERVICE_TIMEOUT, {MDB_SERVICE_NAME,
                                                           MDB_SERVICE_PATH,
                                                           MDB_SERVICE_INTERFACE,
                                                           "GetParentObjects",
                                                           "a{ss}sas",
                                                           {mc::dict(), path, interfaces}});
    return convert_method_result(results);
}

mc::variant get_service_name(mc::dbus::sd_bus* bus, std::string_view sender)
{
    auto results = bus->timeout_call(
        MDB_SERVICE_TIMEOUT,
        {MDB_SERVICE_NAME, MDB_SERVICE_PATH, MDB_SERVICE_INTERFACE, "GetServiceName", "a{ss}s", {mc::dict(), sender}});
    return convert_method_result(results);
}

mc::variant get_service_names(mc::dbus::sd_bus* bus)
{
    auto results = bus->timeout_call(
        MDB_SERVICE_TIMEOUT,
        {MDB_SERVICE_NAME, MDB_SERVICE_PATH, MDB_SERVICE_INTERFACE, "GetServiceNames", "a{ss}", {mc::dict()}});
    return convert_method_result(results);
}

// 验证路径并设置订阅
// 返回：{验证后的路径, 是否验证通过}
static std::pair<std::string, bool> verify_and_setup_subscription(mc::dbus::sd_bus* bus, std::string_view interface,
                                                                  std::string_view filter, bool ignore_case,
                                                                  const std::string& path)
{
    // 先无条件增加新path的订阅引用计数
    setup_signal_watch(bus, path, std::string(interface));

    // 二次确认（需要异常安全，确保订阅引用计数被正确管理）
    std::string verify_path;
    try {
        auto [vp, verify_service] = call_get_path(bus, interface, filter, ignore_case);
        verify_path               = std::move(vp);
    } catch (...) {
        // 二次确认失败，减少刚才增加的订阅引用计数
        decrease_subscription_refcount(path, std::string(interface));
        throw;
    }

    if (path != verify_path) {
        // 验证失败，减少刚才增加的订阅引用计数
        decrease_subscription_refcount(path, std::string(interface));
        return {verify_path, false}; // 返回正确路径，但不缓存
    }

    return {path, true}; // 验证通过，可以缓存
}

// 检查是否需要跳过缓存更新
bool should_skip_cache_update(const cache_entry& old_entry, std::string_view path, std::string_view interface,
                              bool ignore_case)
{
    if (old_entry.path != path || old_entry.interface != interface) {
        return false;
    }
    return old_entry.ignore_case == ignore_case || !old_entry.ignore_case;
}

// 处理旧缓存条目的清理
void handle_old_cache_entry(const cache_entry& old_entry, const std::string& key_str, std::string_view path,
                            std::string_view interface)
{
    remove_from_indices(key_str, old_entry.path, old_entry.interface, old_entry.service_name);

    if (old_entry.path == path && old_entry.interface == interface) {
        t_pending_cleanup.emplace_back(std::string(path), std::string(interface));
    } else {
        t_pending_cleanup.emplace_back(old_entry.path, old_entry.interface);
    }
}

// 创建新的缓存条目
cache_entry create_cache_entry(std::string_view path, std::string_view interface, const std::string& service_name,
                               std::string_view filter, bool ignore_case)
{
    cache_entry entry;
    entry.path         = std::string(path);
    entry.interface    = std::string(interface);
    entry.service_name = service_name;
    entry.filter       = std::string(filter);
    entry.ignore_case  = ignore_case;

    try {
        if (!filter.empty()) {
            auto filter_variant = mc::json::json_decode(filter);
            if (filter_variant.is_dict()) {
                entry.filter_dict = filter_variant.as_dict();
            }
        }
    } catch (const std::exception& e) {
        dlog("Failed to parse filter: ${filter}", ("filter", filter));
    }

    return entry;
}

// 更新缓存和索引（需在持有锁的情况下调用）
static void update_cache_and_indices_locked(std::string_view cache_key, std::string_view path,
                                            std::string_view interface, const std::string& service_name,
                                            std::string_view filter, bool ignore_case)
{
    auto&       cache = get_path_cache();
    std::string key_str(cache_key);

    auto existing_val = cache.get(key_str);
    if (existing_val.has_value()) {
        auto old_entry = existing_val->get();
        if (should_skip_cache_update(old_entry, path, interface, ignore_case)) {
            // 跳过更新：路径和接口相同，缓存条目仍然有效
            // verify_and_setup_subscription 中已经增加了订阅引用计数
            // 但由于缓存条目不变，订阅也不变，所以需要减少刚才增加的引用计数
            // 注意：这里不会影响缓存条目的订阅，因为缓存条目还在使用该订阅
            t_pending_cleanup.emplace_back(std::string(path), std::string(interface));
            return;
        }

        handle_old_cache_entry(old_entry, key_str, path, interface);
    }

    auto entry = create_cache_entry(path, interface, service_name, filter, ignore_case);
    cache.put(key_str, entry);
    add_to_indices(key_str, std::string(path), std::string(interface), service_name);
}

// 尝试从缓存获取路径，如果匹配则返回
std::optional<std::string> try_get_cached_path(const std::string& key_str, bool ignore_case,
                                               mc::algorithm::lru_cache<std::string, cache_entry>& cache)
{
    auto val = cache.get(key_str);
    if (!val.has_value()) {
        return std::nullopt;
    }

    if (ignore_case || !val->get().ignore_case) {
        return val->get().path;
    }
    return std::nullopt;
}

// 等待其他线程完成创建或开始新创建
// 返回值：std::nullopt - 已有缓存，无需创建
//        true - 需要当前线程创建
//        false - 超时，建议放弃缓存直接调用
std::optional<bool> wait_or_start_creation(const std::string& cache_key, const std::string& key_str, bool ignore_case,
                                           std::unique_lock<std::recursive_mutex>& lock, creating_cache_map& creating,
                                           mc::algorithm::lru_cache<std::string, cache_entry>& cache)
{
    constexpr int                       MAX_WAIT_ATTEMPTS = 100;   // 最大等待次数
    constexpr std::chrono::milliseconds WAIT_TIMEOUT(100);         // 每次等待超时时间
    constexpr std::chrono::milliseconds TOTAL_WAIT_TIMEOUT(10000); // 总等待超时时间
    int                                 wait_count = 0;
    auto                                start_time = std::chrono::steady_clock::now();

    while (true) {
        auto cached = try_get_cached_path(key_str, ignore_case, cache);
        if (cached.has_value()) {
            return std::nullopt; // 已有缓存，无需创建
        }

        auto creating_it = creating.find(cache_key);
        if (creating_it == creating.end()) {
            return true; // 没有其他线程在创建，需要当前线程创建
        }

        // 检查等待次数
        if (++wait_count > MAX_WAIT_ATTEMPTS) {
            elog("Cache creation wait timeout: cache_key=${cache_key}, wait_count=${count}",
                 ("cache_key", cache_key)("count", wait_count));
            // 超时后放弃缓存，建议直接调用
            return false;
        }

        // 检查总等待时间
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= TOTAL_WAIT_TIMEOUT) {
            elog("Cache creation total wait timeout: cache_key=${cache_key}, elapsed=${elapsed}ms",
                 ("cache_key", cache_key)("elapsed",
                                          std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()));
            return false;
        }

        auto cv = creating_it->second;
        // 使用 wait_for 避免无限等待，防止死锁
        cv->wait_for(lock, WAIT_TIMEOUT);
    }
}

// 执行路径查询和验证
std::tuple<std::string, std::string, bool> query_and_verify_path(mc::dbus::sd_bus* bus, std::string_view interface,
                                                                 std::string_view filter, bool ignore_case)
{
    auto [path, service_name] = call_get_path(bus, interface, filter, ignore_case);
    if (path.empty()) {
        return {path, service_name, false};
    }

    auto [verified_path, is_verified] = verify_and_setup_subscription(bus, interface, filter, ignore_case, path);

    if (!is_verified) {
        return {verified_path, "", false};
    }

    return {path, service_name, true};
}

// 处理缓存命中或等待创建的情况
// 返回：std::nullopt - 已处理（返回缓存值或超时后直接调用）
//       std::string - 需要当前线程创建缓存
std::optional<mc::variant>
handle_cache_hit_or_wait(const std::string& cache_key, bool ignore_case, std::unique_lock<std::recursive_mutex>& lock,
                         creating_cache_map& creating, mc::algorithm::lru_cache<std::string, cache_entry>& cache,
                         mc::dbus::sd_bus* bus, std::string_view interface, std::string_view filter)
{
    auto result = wait_or_start_creation(cache_key, cache_key, ignore_case, lock, creating, cache);
    if (!result.has_value()) {
        // 已有缓存，直接返回
        auto cached_path = try_get_cached_path(cache_key, ignore_case, cache);
        if (cached_path.has_value()) {
            return mc::variant(cached_path.value());
        }
        // 缓存被清除，需要重新创建，释放锁后直接调用，避免死锁
        lock.unlock();
        return mc::variant(call_get_path(bus, interface, filter, ignore_case).first);
    }

    if (!result.value()) {
        // 超时，放弃缓存，直接调用
        lock.unlock();
        return mc::variant(call_get_path(bus, interface, filter, ignore_case).first);
    }

    return std::nullopt; // 需要当前线程创建缓存
}

// 执行缓存创建逻辑
mc::variant execute_cache_creation(const std::string& cache_key, mc::dbus::sd_bus* bus, std::string_view interface,
                                   std::string_view filter, bool ignore_case,
                                   std::unique_lock<std::recursive_mutex>& lock, creating_cache_map& creating)
{
    ilog("execute_cache_creation: START cache_key=${cache_key}", ("cache_key", cache_key));

    // 注册到 creating map
    auto cv             = std::make_shared<std::condition_variable_any>();
    creating[cache_key] = cv;
    lock.unlock();
    ilog("execute_cache_creation: released cache_mutex");

    // 清理函数：从 creating map 中移除并通知等待线程
    auto cleanup_locked = [&]() {
        auto it = creating.find(cache_key);
        if (it != creating.end() && it->second == cv) {
            creating.erase(it);
        }
        cv->notify_all();
    };

    auto cleanup = [&]() {
        lock.lock();
        cleanup_locked();
        lock.unlock();
    };

    std::string path_result;
    bool        need_cleanup = true;

    try {
        auto [path, service_name, success] = query_and_verify_path(bus, interface, filter, ignore_case);

        if (!success) {
            cleanup();
            return path;
        }

        lock.lock();
        try {
            update_cache_and_indices_locked(cache_key, path, interface, service_name, filter, ignore_case);
            cleanup_locked();
            need_cleanup = false;
            path_result  = path;
        } catch (...) {
            cleanup_locked();
            need_cleanup = false;
            lock.unlock();
            throw;
        }
        lock.unlock();
        process_pending_cleanup();
        return path_result;
    } catch (const std::exception& e) {
        elog("Get path failed: interface=${interface}, filter=${filter}, error=${error}",
             ("interface", interface)("filter", filter)("error", e.what()));
        process_pending_cleanup();
        if (need_cleanup) {
            cleanup();
        }
        throw;
    }
}

mc::variant get_path(mc::dbus::sd_bus* bus, std::string_view interface, std::string_view filter, bool ignore_case,
                     bool enable_cache)
{
    if (bus->is_blocking() || !enable_cache) {
        return call_get_path(bus, interface, filter, ignore_case).first;
    }

    std::string                            cache_key = std::string(interface) + ":" + std::string(filter);
    std::unique_lock<std::recursive_mutex> lock(get_cache_mutex());
    auto&                                  cache    = get_path_cache();
    auto&                                  creating = get_creating_cache_entries();

    // 尝试从缓存获取或等待其他线程创建
    auto cached_result =
        handle_cache_hit_or_wait(cache_key, ignore_case, lock, creating, cache, bus, interface, filter);
    if (cached_result.has_value()) {
        return cached_result.value();
    }

    // 需要当前线程创建缓存
    return execute_cache_creation(cache_key, bus, interface, filter, ignore_case, lock, creating);
}

mc::variant get_interface_owners(mc::dbus::sd_bus* bus, std::string_view interface)
{
    auto results = bus->timeout_call(MDB_SERVICE_TIMEOUT, {MDB_SERVICE_NAME,
                                                           MDB_SERVICE_PATH,
                                                           MDB_SERVICE_INTERFACE,
                                                           "GetInterfaceOwners",
                                                           "a{ss}s",
                                                           {mc::dict(), interface}});
    return convert_method_result(results);
}

mc::variant is_valid_path(mc::dbus::sd_bus* bus, std::string_view path, bool ignore_case)
{
    auto results = bus->timeout_call(MDB_SERVICE_TIMEOUT, {MDB_SERVICE_NAME,
                                                           MDB_SERVICE_PATH,
                                                           MDB_SERVICE_INTERFACE,
                                                           "IsValidPath",
                                                           "a{ss}sb",
                                                           {mc::dict(), path, ignore_case}});
    return convert_method_result(results);
}

mc::variant get_sub_paths_paging(mc::dbus::sd_bus* bus, std::string_view path, int32_t depth,
                                 const mc::variants& interfaces, int32_t skip, int32_t top)
{
    auto results = bus->timeout_call(MDB_SERVICE_TIMEOUT, {MDB_SERVICE_NAME,
                                                           MDB_SERVICE_PATH,
                                                           MDB_SERVICE_INTERFACE,
                                                           "GetSubPathsPaging",
                                                           "a{ss}siasii",
                                                           {mc::dict(), path, depth, interfaces, skip, top}});
    return convert_method_result(results);
}

mc::variant get_classes(mc::dbus::sd_bus* bus, std::string_view service)
{
    auto results = bus->timeout_call(
        MDB_SERVICE_TIMEOUT,
        {MDB_SERVICE_NAME, MDB_SERVICE_PATH, MDB_SERVICE_INTERFACE, "GetClasses", "a{ss}s", {mc::dict(), service}});
    return convert_method_result(results);
}

mc::variant get_object_list(mc::dbus::sd_bus* bus, std::string_view class_name)
{
    auto results = bus->timeout_call(MDB_SERVICE_TIMEOUT, {MDB_SERVICE_NAME,
                                                           MDB_SERVICE_PATH,
                                                           MDB_SERVICE_INTERFACE,
                                                           "GetObjectList",
                                                           "a{ss}s",
                                                           {mc::dict(), class_name}});
    return convert_method_result(results);
}

mc::variant get_object_owner(mc::dbus::sd_bus* bus, std::string_view object_name)
{
    auto results = bus->timeout_call(MDB_SERVICE_TIMEOUT, {MDB_SERVICE_NAME,
                                                           MDB_SERVICE_PATH,
                                                           MDB_SERVICE_INTERFACE,
                                                           "GetObjectOwner",
                                                           "a{ss}s",
                                                           {mc::dict(), object_name}});
    return convert_method_result(results);
}

mc::variant get_matched_objects(mc::dbus::sd_bus* bus, std::string_view object_name, std::string_view interface_pattern)
{
    auto results = bus->timeout_call(MDB_SERVICE_TIMEOUT, {MDB_SERVICE_NAME,
                                                           MDB_SERVICE_PATH,
                                                           MDB_SERVICE_INTERFACE,
                                                           "GetMatchedObjects",
                                                           "a{ss}ss",
                                                           {mc::dict(), object_name, interface_pattern}});
    return convert_method_result(results);
}

mc::variant get_traced_object(mc::dbus::sd_bus* bus)
{
    auto results = bus->timeout_call(
        MDB_SERVICE_TIMEOUT,
        {MDB_SERVICE_NAME, MDB_SERVICE_PATH, MDB_SERVICE_INTERFACE, "GetTracedObject", "a{ss}", {mc::dict()}});
    return convert_method_result(results);
}

void clear_cache()
{
    std::vector<std::string> keys_to_remove;

    {
        std::lock_guard<std::recursive_mutex> lock(get_cache_mutex());
        auto&                                 cache = get_path_cache();

        cache.for_each([&keys_to_remove](const std::string& key, const cache_entry& entry) {
            keys_to_remove.push_back(key);
        });
    }

    try {
        for (const auto& key : keys_to_remove) {
            cleanup_cache_entry(key);
        }
    } catch (const std::exception& e) {
        elog("Clear cache failed: error=${error}", ("error", e.what()));
    }
    // 无论是否有异常，都要处理清理任务，避免订阅引用计数泄漏
    process_pending_cleanup();

    ilog("MDB cache cleared");
}

void set_max_cache_size(size_t max_size)
{
    {
        std::lock_guard<std::recursive_mutex> lock(get_cache_mutex());
        auto&                                 cache = get_path_cache();
        cache.set_max_size(max_size);
        // set_max_size 可能触发多次淘汰，订阅清理任务会累积在 t_pending_cleanup 中
    }

    // 在锁外处理延迟清理队列
    process_pending_cleanup();

    ilog("Set max cache size: ${size}", ("size", max_size));
}

size_t get_max_cache_size()
{
    std::lock_guard<std::recursive_mutex> lock(get_cache_mutex());
    return get_path_cache().max_size();
}

size_t get_cache_size()
{
    std::lock_guard<std::recursive_mutex> lock(get_cache_mutex());
    return get_path_cache().size();
}

} // namespace mc::mdb::service
