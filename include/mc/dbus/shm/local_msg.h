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
#ifndef MC_DBUS_LOCAL_MSG_H
#define MC_DBUS_LOCAL_MSG_H

#include <dbus/dbus.h>
#include <mc/dbus/message.h>
#include <mc/dbus/shm/serialize.h>
#include <mc/dict.h>
#include <mc/variant.h>

namespace mc::dbus {

/**
 * @brief 检查名称是否为唯一名称
 * @param name [in] 名称
 * @return 是唯一名称返回true，否则返回false
 * @constraint 唯一名称以冒号开头
 */
inline bool is_unique_name(std::string_view name) {
    return !name.empty() && name.at(0) == ':';
}

/**
 * @brief 本地消息对象
 */
class MC_API local_msg {
public:
    /**
     * @brief 默认构造函数
     */
    local_msg() = default;

    /**
     * @brief 构造方法调用消息
     * @param service_name [in] 服务名称
     * @param path [in] 对象路径
     * @param interface [in] 接口名称
     * @param member [in] 方法名称
     */
    local_msg(std::string_view service_name, std::string_view path, std::string_view interface,
              std::string_view member);

    /**
     * @brief 从variants反序列化构造
     * @param v [in] variants数组
     */
    local_msg(const variants& v);

    /**
     * @brief 获取错误信息
     * @return 返回错误名称和错误消息的元组
     */
    std::tuple<std::string, std::string> get_error() const;

    /**
     * @brief 设置为方法返回消息
     */
    void method_return();

    /**
     * @brief 设置错误信息
     * @param error_name [in] 错误名称
     * @param message [in] 错误消息
     */
    void error(std::string_view error_name, std::string_view message);

    /**
     * @brief 设置成员名称
     * @param member [in] 成员名称
     */
    void set_member(std::string_view member);

    /**
     * @brief 获取消息类型
     * @return 返回消息类型
     */
    uint32_t msg_type() const;

    /**
     * @brief 获取对象路径
     * @return 返回对象路径
     */
    std::string_view path() const;

    /**
     * @brief 获取接口名称
     * @return 返回接口名称
     */
    std::string_view interface() const;

    /**
     * @brief 获取成员名称
     * @return 返回成员名称
     */
    std::string_view member() const;

    /**
     * @brief 获取签名
     * @return 返回签名字符串
     */
    std::string_view signature() const;

    /**
     * @brief 追加参数
     * @param signature [in] 参数签名
     * @param args [in] 参数列表
     */
    void append_args(std::string_view signature, const variants& args);

    template <typename... Args>
    void append(std::string_view signature, Args&&... args) {
        m_signature = signature;
        m_args      = variants();
        signature_iterator it(signature);
        auto               func = [this, &it, signature](auto&& arg) {
            signature_iterator item_it(it.current_type());
            MC_ASSERT(!item_it.at_end() && !it.at_end(),
                                    "invalid args size for signature: ${signature}", ("signature", signature));
            m_args.push_back(parse_variant(item_it, std::forward<decltype(arg)>(arg), 0));
            it.next();
        };
        (func(std::forward<Args>(args)), ...);
        // 检查签名是否还有剩余（参数数量少于签名要求）
        MC_ASSERT(it.at_end(), "invalid args size for signature: ${signature}, expected more args", ("signature", signature));
    }

    /**
     * @brief 追加返回参数
     * @param signature [in] 参数签名
     * @param arg [in] 返回值
     */
    void append_return_args(std::string_view signature, const variant& arg);

    /**
     * @brief 读取参数
     * @return 返回参数列表
     */
    const variants& read() const;

    /**
     * @brief 设置发送者
     * @param sender [in] 发送者名称
     */
    void set_sender(std::string_view sender);

    /**
     * @brief 获取发送者
     * @return 返回发送者名称
     */
    std::string_view sender() const;

    /**
     * @brief 设置消息序列号
     * @param serial [in] 序列号
     */
    void set_serial(uint32_t serial);

    /**
     * @brief 获取消息序列号
     * @return 返回序列号
     */
    uint32_t get_serial() const;

    /**
     * @brief 获取回复消息序列号
     * @return 返回回复消息序列号
     */
    uint32_t get_reply_serial() const;

    /**
     * @brief 获取目标名称
     * @return 返回目标名称
     */
    std::string_view destination() const;

    /**
     * @brief 设置是否为本地调用
     * @param v [in] 是否为本地调用
     */
    void set_local_call(bool v);

    /**
     * @brief 检查是否为本地调用
     * @return 是本地调用返回true，否则返回false
     */
    bool is_local_call() const;

    /**
     * @brief 创建DBus消息对象
     * @return 返回DBus消息对象
     */
    message new_dbus_msg() const;

    /**
     * @brief 序列化消息
     * @return 返回序列化后的字符串
     */
    std::string pack() const;

    /**
     * @brief 按签名解析variant
     * @param it [in] 签名迭代器
     * @param v [in] variant对象
     * @param depth [in] 嵌套深度
     * @return 返回解析后的variant
     */
    static variant parse_variant(signature_iterator it, const variant& v, size_t depth);

    /**
     * @brief 按签名解析variant数组
     * @param it [in] 签名迭代器
     * @param v [in] variants数组
     * @param depth [in] 嵌套深度
     * @return 返回解析后的variants数组
     */
    static variants parse_variant_elements(signature_iterator it, const variants& v, size_t depth);

private:
    std::string get_error_message() const;

    bool        m_local_call;
    uint32_t    m_type;
    uint32_t    m_serial;
    uint32_t    m_reply_serial;
    std::string m_service_name;
    std::string m_path;
    std::string m_interface;
    std::string m_member;
    std::string m_error_name;
    std::string m_signature;
    std::string m_sender;
    variants    m_args;
};

using shm_msg_promise  = mc::promise<local_msg>;
using serial_map_type  = std::unordered_map<uint32_t, shm_msg_promise>;
using service_map_type = std::unordered_map<std::string, serial_map_type>;

/**
 * @brief 共享内存待处理消息管理器
 */
class MC_API shm_pending_msgs {
public:
    /**
     * @brief 发送消息
     * @param source_unique_name [in] 源唯一名称
     * @param serial [in] 消息序列号
     * @param promise [in] 消息promise
     * @return 成功返回true，失败返回false
     */
    bool send(std::string_view source_unique_name, uint32_t serial, shm_msg_promise promise);

    /**
     * @brief 回复消息
     * @param destination_unique_name [in] 目标唯一名称
     * @param serial [in] 消息序列号
     * @param msg [in/out] 本地消息对象
     * @return 成功返回true，失败返回false
     */
    bool reply(std::string_view destination_unique_name, uint32_t serial, local_msg& msg);

    /**
     * @brief 移除消息
     * @param unique_name [in] 唯一名称
     * @param serial [in] 消息序列号
     */
    void remove(std::string_view unique_name, uint32_t serial);

    /**
     * @brief 清除指定名称的所有消息
     * @param unique_name [in] 唯一名称
     */
    void clear(std::string_view unique_name);

private:
    std::mutex       m_mutex;
    service_map_type m_pending_msgs;
};

} // namespace mc::dbus

#endif // MC_DBUS_LOCAL_MSG_H