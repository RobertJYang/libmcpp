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

/**
 * @file message_queue.h
 * @brief 消息队列管理类，负责消息队列的分配和管理
 */
#ifndef MC_INTERPROCESS_MESSAGE_QUEUE_H
#define MC_INTERPROCESS_MESSAGE_QUEUE_H

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include <mc/common.h>

namespace mc {
namespace interprocess {

/**
 * @brief IPC 消息队列模式
 */
enum class queue_mode {
    CREATE_OR_OPEN, // 创建或打开（如果已存在）
    CREATE_ONLY,    // 仅创建（如果存在则失败）
    OPEN_ONLY       // 仅打开（如果不存在则失败）
};

/**
 * @brief 消息队列权限
 */
struct queue_permissions {
    mode_t owner_read   : 1;
    mode_t owner_write  : 1;
    mode_t group_read   : 1;
    mode_t group_write  : 1;
    mode_t others_read  : 1;
    mode_t others_write : 1;

    queue_permissions() : owner_read(1), owner_write(1),
                          group_read(0), group_write(0),
                          others_read(0), others_write(0) {
    }

    // 创建 POSIX 权限位掩码
    mode_t to_mode() const {
        mode_t mode = 0;
        if (owner_read) {
            mode |= 0400;
        }
        if (owner_write) {
            mode |= 0200;
        }
        if (group_read) {
            mode |= 0040;
        }
        if (group_write) {
            mode |= 0020;
        }
        if (others_read) {
            mode |= 0004;
        }
        if (others_write) {
            mode |= 0002;
        }
        return mode;
    }
};

/**
 * @brief 消息队列配置
 */
struct queue_configuration {
    std::string       name;                    // 队列名称（必须以 '/' 开头）
    size_t            max_messages     = 10;   // 最大消息数
    size_t            max_message_size = 4096; // 最大消息大小（字节）
    queue_mode        mode             = queue_mode::CREATE_OR_OPEN;
    queue_permissions permissions;

    queue_configuration() : name("/default_queue") {
        // 默认构造函数不需要做啥
    }

    queue_configuration(const std::string& queue_name) : name(queue_name) {
        if (name.empty() || name[0] != '/') {
            throw std::invalid_argument("Queue name must start with '/'");
        }
    }
};

/**
 * @brief IPC 消息队列类
 */
class MC_API message_queue {
public:
    /**
     * @brief 默认的构造函数
     * @param config 队列配置
     */
    message_queue() = default;

    /**
     * @brief 带有参数的构造函数
     * @param config 队列配置
     */
    message_queue(const queue_configuration& config);

    /**
     * @brief 析构函数 - 自动关闭队列
     */
    ~message_queue();

    // 禁止拷贝
    message_queue(const message_queue&)            = delete;
    message_queue& operator=(const message_queue&) = delete;

    // 允许移动
    message_queue(message_queue&& other) noexcept;
    message_queue& operator=(message_queue&& other) noexcept;

    // 打开消息队列
    void open(const std::string& name, queue_mode mode,
              int max_msgs = 10, int max_msg_size = 1024);

    // 关闭消息队列
    void close();

    // 检查是否打开
    bool is_open() const {
        return mqd_ != -1;
    }

    /**
     * @brief 发送消息
     * @param data 消息数据
     * @param priority 消息优先级（0-32767，值越大优先级越高）
     * @param timeout_ms 超时时间（毫秒），0表示非阻塞，-1表示阻塞
     * @return 是否发送成功
     */
    bool send(const std::vector<uint8_t>& data,
              unsigned int                priority   = 0,
              int                         timeout_ms = -1);

    /**
     * @brief 接收消息
     * @param timeout_ms 超时时间（毫秒），0表示非阻塞，-1表示阻塞
     * @return 接收到的消息，如果失败返回空vector
     */
    std::vector<uint8_t> receive(int timeout_ms = -1);

    /**
     * @brief 接收消息（带优先级）
     * @param[out] received_priority 接收到的消息优先级
     * @param timeout_ms 超时时间（毫秒）
     * @return 接收到的消息
     */
    std::vector<uint8_t> receive_with_priority(unsigned int& received_priority, int timeout_ms = -1);

    /**
     * @brief 获取队列属性
     */
    struct queue_attribute {
        std::string name;
        int         queue_id;
        size_t      max_messages;
        size_t      max_message_size;
        size_t      current_messages;
    };

    queue_attribute get_attributes() const;

private:
    void     open_queue();
    void     close_queue();
    void     throw_if_closed() const;
    timespec calculate_timeout(int timeout_ms) const;

private:
    queue_configuration config_;
    int                 mqd_   = -1;    // 消息队列描述符
    bool                owner_ = false; // 是否是创建者
}; // class message_queue

} // namespace interprocess
} // namespace mc

#endif // MC_INTERPROCESS_MESSAGE_QUEUE_H