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
#include "shmlock/shmlock_manager.h"
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

static constexpr int      SHM_LOCK_TIMEOUT_DEFAULT_MS = 60000;
static constexpr uint64_t GLOBAL_OBJECT_ID            = 1;
static uint16_t           g_lock_service_id           = 0;

class shm_lock {
public:
    shm_lock()
        : m_shm(shm::shared_memory::get_instance()), m_locked(false) {
        lock();
    }

    ~shm_lock() {
        try {
            unlock();
        } catch (...) {
            // 析构函数中避免抛异常
        }
    }

    void lock() {
        if (!m_locked) {
            m_shm.lock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_locked = false;
            m_shm.unlock();
        }
    }

private:
    shm::shared_memory& m_shm;
    bool                m_locked;
};

class Lock {
public:
    Lock(uint64_t object_id) : m_object_id(object_id), m_lock_mgr(shmlock::ShmLockManager::get_instance()), m_locked(false) {
        if (g_lock_service_id == 0) {
            g_lock_service_id = shmlock::ShmLockManager::allocate_service_id();
        }
        lock();
    }

    ~Lock() {
        try {
            unlock();
        } catch (...) {
            // 析构函数中避免抛异常
        }
    }

    void lock() {
        if (!m_locked) {
            m_lock_handle = m_lock_mgr.acquire_write_lock(m_object_id, g_lock_service_id, SHM_LOCK_TIMEOUT_DEFAULT_MS);
            m_locked      = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_locked = false;
            m_lock_handle.release();
        }
    }

private:
    uint64_t                 m_object_id;
    shmlock::ShmLockManager& m_lock_mgr;
    shmlock::LockHandle      m_lock_handle;
    bool                     m_locked;
};

class SharedLock {
public:
    SharedLock(uint64_t object_id) : m_object_id(object_id), m_lock_mgr(shmlock::ShmLockManager::get_instance()), m_locked(false) {
        if (g_lock_service_id == 0) {
            g_lock_service_id = shmlock::ShmLockManager::allocate_service_id();
        }
        lock();
    }

    ~SharedLock() {
        try {
            unlock();
        } catch (...) {
            // 析构函数中避免抛异常
        }
    }

    void lock() {
        if (!m_locked) {
            m_lock_handle = m_lock_mgr.acquire_read_lock(m_object_id, g_lock_service_id, SHM_LOCK_TIMEOUT_DEFAULT_MS);
            m_locked      = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_locked = false;
            m_lock_handle.release();
        }
    }

private:
    uint64_t                 m_object_id;
    shmlock::ShmLockManager& m_lock_mgr;
    shmlock::LockHandle      m_lock_handle;
    bool                     m_locked;
};

template <typename Fn, typename... Args>
auto shm_global_lock_exec(Fn&& callback, Args&&... args) {
    Lock lock(GLOBAL_OBJECT_ID);
    if constexpr (std::is_void_v<std::invoke_result_t<Fn, Args...>>) {
        std::invoke(std::forward<Fn>(callback), std::forward<Args>(args)...);
        lock.unlock();
    } else {
        auto result = std::invoke(std::forward<Fn>(callback), std::forward<Args>(args)...);
        lock.unlock();
        return result;
    }
}

template <typename Fn, typename... Args>
auto shm_global_lock_shared_exec(Fn&& callback, Args&&... args) {
    SharedLock lock(GLOBAL_OBJECT_ID);
    if constexpr (std::is_void_v<std::invoke_result_t<Fn, Args...>>) {
        std::invoke(std::forward<Fn>(callback), std::forward<Args>(args)...);
        lock.unlock();
    } else {
        auto result = std::invoke(std::forward<Fn>(callback), std::forward<Args>(args)...);
        lock.unlock();
        return result;
    }
}

template <typename Fn, typename... Args>
auto shm_object_lock_exec(uint64_t object_id, Fn&& callback, Args&&... args) {
    Lock lock(object_id);
    if constexpr (std::is_void_v<std::invoke_result_t<Fn, Args...>>) {
        std::invoke(std::forward<Fn>(callback), std::forward<Args>(args)...);
        lock.unlock();
    } else {
        auto result = std::invoke(std::forward<Fn>(callback), std::forward<Args>(args)...);
        lock.unlock();
        return result;
    }
}

template <typename Fn, typename... Args>
auto shm_object_lock_shared_exec(uint64_t object_id, Fn&& callback, Args&&... args) {
    SharedLock lock(object_id);
    if constexpr (std::is_void_v<std::invoke_result_t<Fn, Args...>>) {
        std::invoke(std::forward<Fn>(callback), std::forward<Args>(args)...);
        lock.unlock();
    } else {
        auto result = std::invoke(std::forward<Fn>(callback), std::forward<Args>(args)...);
        lock.unlock();
        return result;
    }
}

using match_cb_t = std::function<void(mc::dbus::message&)>;

class MC_API match_rule {
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

class MC_API match {
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