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
#include <mc/dbus/message.h>
#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/shm/serialize.h>
#include <mc/log.h>
#include <mc/runtime.h>
#include <memory>
#include <sys/file.h>

namespace mc::dbus {

static constexpr size_t                                       MQ_BUFFER_SIZE     = 1024 * 1024;
static uint32_t                                               g_reply_msg_serial = 0;
static std::unordered_map<std::string, shm::message_queue_t*> g_msg_queues;
static std::mutex                                             g_msg_queues_mutex;

shm::object_tree* create_shm_tree(std::string_view harbor_name, std::string_view service_name,
                                  std::string_view unique_name) {
    MC_ASSERT(!harbor_name.empty(), "harbor name is empty");
    MC_ASSERT(!service_name.empty(), "service name is empty");
    MC_ASSERT(!unique_name.empty(), "unique name is empty");
#if defined(BUILD_TYPE) && defined(BUILD_TYPE_DT) && BUILD_TYPE != BUILD_TYPE_DT
    // 非DT环境下，需要等待framework进程创建好共享内存
    while (!shm::shared_memory::is_exist()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
#endif
    // 首次打开共享内存需要使用文件锁
    FILE *fp = fopen("/dev/shm/init_shm.lock", "r+");
    if (fp == nullptr) {
        elog("failed to open init_shm.lock file, ${error}", ("error", strerror(errno)));
    } else {
        if (flock(fileno(fp), LOCK_EX) < 0) {
            elog("failed to lock init_shm.lock file, ${error}", ("error", strerror(errno)));
        } else {
            shm_lock lock;
            lock.unlock();
        }
        fclose(fp);
    }
    return shm_global_lock_exec([harbor_name, service_name, unique_name]() {
        auto& ins = shm::shared_memory::get_instance();
        ins.set_harbor_name(unique_name, harbor_name);
        auto tree = ins.get_tree(service_name);
        tree->set_unique_name(unique_name);
        tree->set_harbor_name(harbor_name);
        return tree;
    });
}

message_queue::message_queue(shm::message_queue_t& msg_queue)
    : m_msg_queue(msg_queue) {
}

message_queue::~message_queue() {
}

static bool is_dbus_message(const std::string_view& data) {
    if (data.empty()) {
        return false;
    }
    uint8_t head = data.at(0);
    return head == DBUS_LITTLE_ENDIAN || head == DBUS_BIG_ENDIAN;
}

void message_queue::dispatch(int timeout_ms, int max_read_count,
                             std::function<void(message_data&)> handler) {
    if (!m_mutex.try_lock_for(std::chrono::milliseconds(100))) {
        return;
    }
    std::unique_lock lock(m_mutex, std::adopt_lock);
    auto             f = [this](const std::string_view& data, int _) {
        message_data msg_data;
        if (is_dbus_message(data)) {
            mc::dbus::error err;
            DBusMessage*    dbus_msg = dbus_message_demarshal(data.data(), data.size(), &err);
            if (err.is_set()) {
                elog("dbus message marshal failed: ${error}", ("error", err.message));
                return;
            }
            msg_data.ptr  = dbus_msg;
            msg_data.size = -1;
        } else {
            msg_data.ptr = malloc(data.size());
            if (msg_data.ptr == nullptr) {
                return;
            }
            std::memcpy(msg_data.ptr, data.data(), data.size());
            msg_data.size = data.size();
        }
        m_messages.push_back(msg_data);
    };
    m_messages.resize(0);
    if (!m_msg_queue.pop_front(f, timeout_ms, max_read_count, m_read_buf)) {
        return;
    }
    for (auto& msg_data : m_messages) {
        handler(msg_data);
    }
}

harbor::harbor()
    : m_is_running(false) {
}

harbor::~harbor() {
    stop();
}

void harbor::set_harbor_name(std::string_view name) {
    m_harbor_name = std::string(name);
}

void harbor::set_harbor_name_if_empty(std::string_view name) {
    if (m_harbor_name.empty()) {
        m_harbor_name = std::string(name);
    }
}

harbor& harbor::get_instance() {
    return mc::singleton<harbor>::instance_with_creator([]() {
        return new harbor();
    });
}

void harbor::init_message_queue() {
    auto& ins             = shm::shared_memory::get_instance();
    auto& harbor_tree_map = ins.get_object_tree_map(m_harbor_name);
    auto  harbor_it       = harbor_tree_map.find(m_harbor_name);
    if (harbor_it == harbor_tree_map.end()) {
        elog("harbor not found in object tree map: ${harbor_name}", ("harbor_name", m_harbor_name));
        return;
    }
    auto msg_queue = harbor_it->second->get_message_queue();
    if (msg_queue == nullptr) {
        m_mq = new message_queue(harbor_it->second->create_message_queue(ins, MQ_BUFFER_SIZE));
    } else {
        m_mq = new message_queue(*msg_queue);
    }
}

void harbor::register_method_handler(std::string_view service_name, std::string_view unique_name,
                                     method_handler_t handler) {
    std::lock_guard<std::mutex> lock(m_method_handlers_mutex);
    auto                        it = m_method_handlers.emplace(unique_name, handler);
    if (!it.second) {
        elog("failed to register method handler: ${service_name}", ("service_name", service_name));
    }
}

std::string_view harbor::get_harbor_name() const {
    return m_harbor_name;
}

static uint32_t set_serial(local_msg* msg) {
    uint32_t serial = msg->get_serial();
    if (serial != 0) {
        return serial;
    }
    if (g_reply_msg_serial == UINT32_MAX) {
        g_reply_msg_serial = 0;
    }
    g_reply_msg_serial++;
    msg->set_serial(g_reply_msg_serial);
    return g_reply_msg_serial;
}

static shm::object_tree* find_harbor_tree(std::string_view service_name) {
    auto&            ins      = shm::shared_memory::get_instance();
    auto&            tree_map = ins.get_object_tree_map(service_name);
    auto             it       = tree_map.find(service_name);
    std::string_view harbor_name;
    if (it == tree_map.end()) {
        harbor_name = ins.get_harbor_name(service_name);
    } else {
        harbor_name = it->second->harbor_name();
    }
    if (harbor_name.empty()) {
        return nullptr;
    }
    auto& harbor_tree_map = ins.get_object_tree_map(harbor_name);
    auto  harbor_it       = harbor_tree_map.find(harbor_name);
    if (harbor_it == harbor_tree_map.end()) {
        return nullptr;
    }
    return &*harbor_it->second;
}

static bool is_online(shm::object_tree* harbor_tree) {
    return !harbor_tree->unique_name().empty();
}

shm::message_queue_t* harbor::get_destination_msg_queue(std::string_view destination) {
    MC_ASSERT(!destination.empty(), "destination is empty");
    std::string_view harbor_name;
    if (is_unique_name(destination)) {
        harbor_name = shm_global_lock_shared_exec([destination]() {
            auto& ins = shm::shared_memory::get_instance();
            return ins.get_harbor_name(destination);
        });
        if (harbor_name.empty()) {
            return nullptr;
        }
    } else {
        harbor_name = destination;
    }
    auto tree = shm_global_lock_shared_exec(find_harbor_tree, harbor_name);
    if (tree == nullptr) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(g_msg_queues_mutex);
    auto                        it = g_msg_queues.find(std::string(harbor_name));
    if (!is_online(tree)) {
        if (it != g_msg_queues.end()) {
            g_msg_queues.erase(it);
        }
        return nullptr;
    }
    if (it == g_msg_queues.end()) {
        auto msg_queue = tree->get_message_queue();
        if (msg_queue == nullptr) {
            return nullptr;
        }
        auto res = g_msg_queues.emplace(harbor_name, msg_queue);
        if (!res.second) {
            elog("failed to emplace message queue: ${harbor_name}", ("harbor_name", harbor_name));
        }
        return msg_queue;
    }
    return it->second;
}

void harbor::dbus_reply(local_msg* msg) {
    auto dbus_msg = msg->new_dbus_msg();
    dbus_msg.set_member("shm_reply");
    m_connection.send(std::move(dbus_msg));
}

void harbor::invoke_method(local_msg* msg) {
    auto destination = msg->destination();
    if (!is_unique_name(destination)) {
        destination = get_unique_name(destination);
    }
    auto it = m_method_handlers.find(std::string(destination));
    if (it == m_method_handlers.end()) {
        elog("method handler not found: ${destination}", ("destination", destination));
        return;
    }
    auto path      = msg->path();
    auto interface = msg->interface();
    auto member    = msg->member();
    auto args      = msg->read();
    auto handler   = it->second;
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        auto result     = handler(path, interface, member, args);
        auto end_time   = std::chrono::high_resolution_clock::now();
        auto duration   = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        if (duration >= 5000) {
            wlog("method handle time cost: ${duration} ms, path: ${path}, interface: ${interface}, method: ${member}",
                 ("duration", duration)("path", path)("interface", interface)("member", member));
        }
        if (result.first) {
            msg->method_return();
            msg->append_return_args(result.first->get_result_signature(), result.second);
        } else {
            msg->error(error_names::unknown_method, "method not found");
        }
    } catch (const std::exception& e) {
        msg->error(error_names::failed, std::string(e.what()));
    }
    set_serial(msg);
    auto reply_destination = msg->destination();
    auto msg_queue         = get_destination_msg_queue(reply_destination);
    if (msg_queue == nullptr) {
        elog("failed to get message queue: ${destination}", ("destination", reply_destination));
        dbus_reply(msg);
        return;
    }
    if (!msg_queue->push_back(msg->pack(), MSG_QUEUE_PUSH_TIMEOUT)) {
        elog("failed to push message to message queue: ${destination}",
             ("destination", reply_destination));
        dbus_reply(msg);
    }
}

void harbor::process_message(message_data& msg_data) {
    if (msg_data.size < 0) {
        process_dbus_message(reinterpret_cast<DBusMessage*>(msg_data.ptr));
    } else {
        auto unpacked =
            serialize::unpack(std::string_view(static_cast<char*>(msg_data.ptr), msg_data.size));
        free(msg_data.ptr);
        process_local_message(unpacked);
    }
}

void harbor::process_dbus_message(DBusMessage* msg) {
    int msg_type = dbus_message_get_type(msg);
    if (msg_type == DBUS_MESSAGE_TYPE_SIGNAL) {
        // 异步处理 signal 消息，避免阻塞 harbor 线程
        dbus_message_ref(msg); // 增加引用计数，传递到异步任务
        boost::asio::post(mc::get_work_context(), [this, msg]() mutable {
            auto& match = m_connection.get_match();
            match.run_msg(msg);
            dbus_message_unref(msg); // 在异步任务中释放引用
        });
    } else {
        elog("invalid message type ${type} for shared memory queue", ("type", msg_type));
    }
    dbus_message_unref(msg);
}

void harbor::process_local_message(const variants& unpacked) {
    if (unpacked.size() < 2 || !unpacked[0].is_integer() || !unpacked[1].is_string()) {
        // 格式错误的消息无法解析msg_type和destination
        return;
    }
    auto msg      = std::make_unique<local_msg>(unpacked);
    auto msg_type = msg->msg_type();
    bool is_reply;
    if (msg_type == DBUS_MESSAGE_TYPE_METHOD_RETURN || msg_type == DBUS_MESSAGE_TYPE_ERROR) {
        is_reply = true;
    } else if (msg_type == DBUS_MESSAGE_TYPE_METHOD_CALL) {
        is_reply = false;
    } else {
        elog("unknown message type: ${msg_type}", ("msg_type", msg_type));
        return;
    }
    if (!is_reply) {
        boost::asio::post(mc::get_work_context(), [this, msg = std::move(msg)]() mutable {
            invoke_method(msg.get());
        });
        return;
    }
    reply_shm_msg(msg->destination(), msg->get_reply_serial(), *msg);
}

void harbor::start() {
    if (m_harbor_name.empty()) {
        MC_THROW(mc::exception, "harbor name is empty");
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connection.is_connected()) {
        return;
    }
    auto connection = connection::open_session_bus(mc::get_io_context());
    if (!connection.start()) {
        MC_THROW(mc::exception, "failed to start dbus connection");
    }
    if (!connection.request_name(m_harbor_name)) {
        MC_THROW(mc::exception, "failed to request name: ${name}", ("name", m_harbor_name));
    }
    m_unique_name = connection.get_unique_name();
    m_connection  = connection;
    create_shm_tree(m_harbor_name, m_harbor_name, m_unique_name);
    shm_global_lock_exec([this]() {
        init_message_queue();
    });
    if (m_mq == nullptr) {
        MC_THROW(mc::exception, "failed to init message queue");
    }
    m_is_running = true;
    // 创建3个worker线程来处理消息
    for (int i = 0; i < 3; ++i) {
        m_workers.emplace_back(std::make_unique<std::thread>([this]() {
            while (m_is_running) {
                m_mq->dispatch(1000, 1000, [this](message_data& msg_data) {
                    process_message(msg_data);
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }));
    }
}

void harbor::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_is_running = false;
    m_connection.disconnect();
    // 等待所有worker线程结束
    for (auto& worker : m_workers) {
        if (worker && worker->joinable()) {
            worker->join();
        }
    }
    m_workers.clear();
}

void harbor::register_unique_name(std::string unique_name, std::string service_name) {
    std::lock_guard lock(m_unique_name_map_mutex);
    m_unique_name_map.emplace(service_name, unique_name);
}

std::string harbor::get_unique_name(std::string_view service_name) {
    std::lock_guard lock(m_unique_name_map_mutex);
    auto            it = m_unique_name_map.find(std::string(service_name));
    if (it == m_unique_name_map.end()) {
        MC_THROW(mc::system_exception, "failed to get unique name for service ${service_name}",
                 ("service_name", service_name));
    }
    return it->second;
}

bool harbor::send_shm_msg(std::string_view source_name, uint32_t serial,
                          mc::dbus::shm_msg_promise promise) {
    if (!mc::dbus::is_unique_name(source_name)) {
        source_name = get_unique_name(source_name);
    }
    return m_shm_pending_msgs.send(source_name, serial, std::move(promise));
}

bool harbor::reply_shm_msg(std::string_view destination_name, uint32_t serial,
                           mc::dbus::local_msg& msg) {
    if (!mc::dbus::is_unique_name(destination_name)) {
        destination_name = get_unique_name(destination_name);
    }
    return m_shm_pending_msgs.reply(destination_name, serial, msg);
}

void harbor::unregister_service(std::string service_name) {
    std::lock_guard lock(m_unique_name_map_mutex);
    auto            it = m_unique_name_map.find(std::string(service_name));
    if (it != m_unique_name_map.end()) {
        m_shm_pending_msgs.clear(it->second);
        m_unique_name_map.erase(it);
    }
#if defined(BUILD_TYPE) && defined(BUILD_TYPE_DT) && BUILD_TYPE == BUILD_TYPE_DT
    // DT环境下，服务全都注销后停止harbor
    if (m_unique_name_map.empty()) {
        stop();
    }
#endif
}

void harbor::add_rule(mc::dbus::match_rule& rule, mc::dbus::match_cb_t&& cb, uint64_t id) {
    auto& match = m_connection.get_match();
    match.add_rule(rule, std::forward<match_cb_t>(cb), id);
}

void harbor::remove_rule(uint64_t id) {
    auto& match = m_connection.get_match();
    match.remove_rule(id);
}

} // namespace mc::dbus
