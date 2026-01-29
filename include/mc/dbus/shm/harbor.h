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
#include <mc/dbus/match.h>
#include <mc/dbus/shm/local_msg.h>
#include <mc/engine.h>
#include <mc/variant.h>

namespace mc::dbus {
constexpr int MSG_QUEUE_PUSH_TIMEOUT = 100;

using reply_msg_map_t      = std::unordered_map<std::string, std::unordered_map<uint32_t, local_msg*>>;
using invoke_result        = std::pair<const mc::reflect::method_type_info*, mc::engine::invoke_result>;
using method_handler_t     = std::function<invoke_result(
    std::string_view, std::string_view, std::string_view, const mc::variants&)>;
using method_handler_map_t = std::unordered_map<std::string, method_handler_t>;

/**
 * @brief 消息数据
 */
struct message_data {
    void* ptr;  /**< 数据指针 */
    int   size; /**< 数据大小 */
};

/**
 * @brief 创建共享内存对象树
 * @param harbor_name [in] 港口名称
 * @param service_name [in] 服务名称
 * @param unique_name [in] 唯一名称
 * @return 返回共享内存对象树指针
 */
MC_API shm::object_tree* create_shm_tree(std::string_view harbor_name, std::string_view service_name,
                                         std::string_view unique_name);

/**
 * @brief 消息队列
 */
class message_queue {
public:
    /**
     * @brief 构造函数
     * @param msg_queue [in/out] 共享内存消息队列引用
     */
    message_queue(shm::message_queue_t& msg_queue);

    /**
     * @brief 析构函数
     */
    ~message_queue();

    /**
     * @brief 分发消息
     * @param timeout_ms [in] 超时时间（毫秒）
     * @param max_read_count [in] 最大读取数量
     * @param handler [in] 消息处理回调函数
     */
    void dispatch(int timeout_ms, int max_read_count, std::function<void(message_data&)> handler);

private:
    shm::message_queue_t&     m_msg_queue;
    std::vector<message_data> m_messages;
    std::timed_mutex          m_mutex;
    std::string               m_read_buf;
};

/**
 * @brief 共享内存通信港口
 */
class MC_API harbor {
public:
    /**
     * @brief 构造函数
     */
    harbor();

    /**
     * @brief 析构函数
     */
    ~harbor();

    /**
     * @brief 获取港口单例
     * @return 返回港口单例引用
     */
    static harbor& get_instance();

    /**
     * @brief 获取目标消息队列
     * @param destination [in] 目标名称
     * @return 返回消息队列指针
     */
    static shm::message_queue_t* get_destination_msg_queue(std::string_view destination);

    /**
     * @brief 设置港口名称
     * @param name [in] 港口名称
     */
    void set_harbor_name(std::string_view name);

    /**
     * @brief 如果港口名称为空则设置
     * @param name [in] 港口名称
     */
    void set_harbor_name_if_empty(std::string_view name);

    /**
     * @brief 获取港口名称
     * @return 返回港口名称
     */
    std::string_view get_harbor_name() const;

    /**
     * @brief 启动港口
     */
    void start();

    /**
     * @brief 停止港口
     */
    void stop();

    /**
     * @brief 注册方法处理器
     * @param service_name [in] 服务名称
     * @param unique_name [in] 唯一名称
     * @param handler [in] 方法处理器
     */
    void register_method_handler(std::string_view service_name, std::string_view unique_name,
                                 method_handler_t handler);

    /**
     * @brief 发送共享内存消息
     * @param source_name [in] 源名称
     * @param serial [in] 消息序列号
     * @param promise [in] 消息promise
     * @return 成功返回true，失败返回false
     */
    bool send_shm_msg(std::string_view source_name, uint32_t serial,
                      mc::dbus::shm_msg_promise promise);

    /**
     * @brief 回复共享内存消息
     * @param destination_name [in] 目标名称
     * @param serial [in] 消息序列号
     * @param msg [in/out] 本地消息对象
     * @return 成功返回true，失败返回false
     */
    bool reply_shm_msg(std::string_view destination_name, uint32_t serial,
                       mc::dbus::local_msg& msg);

    /**
     * @brief 移除共享内存消息
     * @param source_name [in] 源名称
     * @param serial [in] 消息序列号
     */
    void remove_shm_msg(std::string_view source_name, uint32_t serial);

    /**
     * @brief 注册唯一名称
     * @param unique_name [in] 唯一名称
     * @param service_name [in] 服务名称
     */
    void register_unique_name(std::string unique_name, std::string service_name);

    /**
     * @brief 获取唯一名称
     * @param service_name [in] 服务名称
     * @return 返回唯一名称
     */
    std::string get_unique_name(std::string_view service_name);

    /**
     * @brief 注销服务
     * @param service_name [in] 服务名称
     */
    void unregister_service(std::string service_name);

    /**
     * @brief 添加匹配规则
     * @param rule [in] 匹配规则
     * @param cb [in] 回调函数
     * @param id [in] 规则ID
     */
    void add_rule(mc::dbus::match_rule& rule, mc::dbus::match_cb_t&& cb, uint64_t id);

    /**
     * @brief 移除匹配规则
     * @param id [in] 规则ID
     */
    void remove_rule(uint64_t id);

    /**
     * @brief 添加匹配规则并订阅信号
     * @param rule [in] 匹配规则
     * @param cb [in] 回调函数
     * @return 返回分配的规则ID
     */
    uint64_t add_match(mc::dbus::match_rule& rule, mc::dbus::match_cb_t&& cb);

    /**
     * @brief 移除匹配规则并取消订阅信号
     * @param id [in] 规则ID
     */
    void remove_match(uint64_t id);

private:
    void init_message_queue();
    void process_message(message_data& msg_data);
    void process_dbus_message(DBusMessage* msg);
    void process_local_message(const variants& unpacked);
    void invoke_method(local_msg* msg);
    void dbus_reply(local_msg* msg);

    std::string                                  m_harbor_name;
    std::string                                  m_unique_name;
    connection                                   m_connection;
    bool                                         m_is_running;
    message_queue*                               m_mq;
    std::mutex                                   m_mutex;
    method_handler_map_t                         m_method_handlers;
    std::mutex                                   m_method_handlers_mutex;
    mc::dbus::shm_pending_msgs                   m_shm_pending_msgs;
    std::unordered_map<std::string, std::string> m_unique_name_map;
    std::mutex                                   m_unique_name_map_mutex;
    std::vector<std::unique_ptr<std::thread>>    m_workers;
    std::unordered_map<int, std::string>         m_match_map;
    std::unordered_map<std::string, int>         m_match_count;
};

} // namespace mc::dbus

#endif // MC_DBUS_HARBOR_H