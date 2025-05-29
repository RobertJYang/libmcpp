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

#ifndef MC_DBUS_HARBOR_H
#define MC_DBUS_HARBOR_H

#include <dbus/dbus.h>
#include <mc/dbus/connection.h>
#include <mc/dbus/shm/local_msg.h>
#include <mc/engine.h>
#include <mc/variant.h>

#define BUILD_TYPE_DT (0x0a)

#if defined(ENABLE_DBUS_SHARED_MEMORY) && ENABLE_DBUS_SHARED_MEMORY == 1
#include <dbus/shm_tree/object.h>
#include <dbus/shm_tree/shared_memory.h>
#include <dbus/shm_tree/shared_memory_base.h>
#include <dbus/shm_tree/tree.h>
#elif defined(BUILD_TYPE) && defined(BUILD_TYPE_DT) && BUILD_TYPE == BUILD_TYPE_DT
// DT环境下如果禁用共享内存，使用打桩的定义
#include <mc/dbus/shm/mock_shm.h>
#endif

namespace mc::dbus {
constexpr int MSG_QUEUE_PUSH_TIMEOUT = 100;

using reply_msg_map_t  = std::unordered_map<std::string, std::unordered_map<uint32_t, local_msg*>>;
using method_handler_t = std::function<mc::engine::invoke_result(
    std::string_view, std::string_view, std::string_view, const mc::variants&)>;
using method_handler_map_t = std::unordered_map<std::string, method_handler_t>;

struct message_data {
    void* ptr;
    int   size;
};

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

shm::object_tree* create_shm_tree(std::string_view harbor_name, std::string_view service_name,
                                  std::string_view unique_name);

class message_queue {
public:
    message_queue(shm::message_queue_t& msg_queue);
    ~message_queue();
    void dispatch(int timeout_ms, int max_read_count, std::function<void(message_data&)> handler);

private:
    shm::message_queue_t&     m_msg_queue;
    std::vector<message_data> m_messages;
    std::timed_mutex          m_mutex;
    std::string               m_read_buf;
};

class harbor {
public:
    harbor();
    ~harbor();
    static harbor&   get_instance();
    void             set_harbor_name(std::string_view name);
    void             set_harbor_name_if_empty(std::string_view name);
    std::string_view get_harbor_name() const;
    void             start();
    void             stop();
    void        register_method_handler(std::string_view service_name, std::string_view unique_name,
                                        method_handler_t handler);
    bool        send_shm_msg(std::string_view source_name, uint32_t serial,
                             mc::dbus::shm_msg_promise promise);
    bool        reply_shm_msg(std::string_view destination_name, uint32_t serial,
                              mc::dbus::local_msg& msg);
    void        register_unique_name(std::string unique_name, std::string service_name);
    std::string get_unique_name(std::string_view service_name);
    void        unregister_service(std::string service_name);
    static shm::message_queue_t* get_destination_msg_queue(std::string_view destination);

private:
    void init_message_queue();
    void process_message(message_data& msg_data);
    void process_dbus_message(DBusMessage* msg);
    void process_local_message(const variants& unpacked);
    void invoke_method(local_msg* msg);
    void dbus_reply(local_msg* msg);

    std::string                                  m_harbor_name;
    std::string                                  m_unique_name;
    strand_type                                  m_strand;
    connection                                   m_connection;
    bool                                         m_is_running;
    message_queue*                               m_mq;
    std::mutex                                   m_mutex;
    method_handler_map_t                         m_method_handlers;
    std::mutex                                   m_method_handlers_mutex;
    mc::dbus::shm_pending_msgs                   m_shm_pending_msgs;
    std::unordered_map<std::string, std::string> m_unique_name_map;
    std::mutex                                   m_unique_name_map_mutex;
    std::unique_ptr<std::thread>                 m_worker;
};

} // namespace mc::dbus

#endif // MC_DBUS_HARBOR_H