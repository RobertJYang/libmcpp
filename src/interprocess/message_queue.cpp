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

#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <iostream>

#include "mc/interprocess/message_queue.h"

namespace mc {
namespace interprocess {
message_queue::message_queue(const queue_configuration& config) 
    : config_(config), mqd_(-1), owner_(false) {
    
    // 验证配置
    if (config_.max_messages == 0 || config_.max_message_size == 0) {
        throw std::invalid_argument("Invalid queue configuration");
    }
    
    open_queue();
}

message_queue::~message_queue() {
    close_queue();
}

message_queue::message_queue(message_queue&& other) noexcept
    : config_(std::move(other.config_)),
      mqd_(other.mqd_),
      owner_(other.owner_) {
    other.mqd_ = -1;
    other.owner_ = false;
}

message_queue& message_queue::operator=(message_queue&& other) noexcept {
    if (this != &other) {
        close_queue();
        config_ = std::move(other.config_);
        mqd_ = other.mqd_;
        owner_ = other.owner_;
        other.mqd_ = -1;
        other.owner_ = false;
    }
    return *this;
}

void message_queue::open_queue() {
    // 设置打开标志
    int flags = O_RDWR;
    
    switch (config_.mode) {
        case queue_mode::CREATE_OR_OPEN:
            flags |= O_CREAT;
            owner_ = true;
            break;
        case queue_mode::CREATE_ONLY:
            flags |= O_CREAT | O_EXCL;
            owner_ = true;
            break;
        case queue_mode::OPEN_ONLY:
            // 不设置 O_CREAT
            owner_ = false;
            break;
    }
    
    // 设置队列属性
    struct mq_attr attr;
    std::memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg = static_cast<long>(config_.max_messages);
    attr.mq_msgsize = static_cast<long>(config_.max_message_size);
    
    // 打开/创建消息队列
    mqd_ = mq_open(config_.name.c_str(), flags, 
                   config_.permissions.to_mode(), &attr);
    
    if (mqd_ == -1) {
        std::string error = "Failed to open message queue '";
        error += config_.name + "': " + std::strerror(errno);
        throw std::system_error(errno, std::system_category(), error);
    }
}

void message_queue::close_queue() {
    if (mqd_ != -1) {
        mq_close(mqd_);
        
        // 如果是创建者，则删除队列
        if (owner_) {
            mq_unlink(config_.name.c_str());
        }
        
        mqd_ = -1;
        owner_ = false;
    }
}

bool message_queue::send(const std::vector<uint8_t>& data, 
                        unsigned int priority,
                        int timeout_ms) {
    
    if (!is_valid()) {
        return false;
    }
    
    // 允许发送空消息（某些场景需要空消息作为信号）
    // POSIX mq_send 允许发送长度为0的消息
    
    if (data.size() > config_.max_message_size) {
        throw std::runtime_error("Message size exceeds maximum allowed");
    }
    
    // 限制优先级范围
    if (priority > 32767) {
        priority = 32767;
    }
    
    // 设置超时
    timespec timeout = calculate_timeout(timeout_ms);
    
    int result;
    // 对于空消息，传递 nullptr 和 size=0
    const char* msg_ptr = data.empty() ? nullptr : reinterpret_cast<const char*>(data.data());
    
    if (timeout_ms >= 0) {
        result = mq_timedsend(mqd_, msg_ptr, data.size(), priority, &timeout);
    } else {
        result = mq_send(mqd_, msg_ptr, data.size(), priority);
    }
    
    if (result == -1) {
        if (errno == EAGAIN || errno == ETIMEDOUT) {
            return false;  // 超时或队列满
        }
        throw std::system_error(errno, std::system_category(), 
                               "Failed to send message");
    }
    
    return true;
}

std::vector<uint8_t> message_queue::receive(int timeout_ms) {
    unsigned int priority;
    return receive_with_priority(priority, timeout_ms);
}

std::vector<uint8_t> message_queue::receive_with_priority(
    unsigned int& received_priority, int timeout_ms) {
    
    std::vector<uint8_t> buffer(config_.max_message_size);
    
    if (!is_valid()) {
        return {};
    }
    
    // 设置超时
    timespec timeout = calculate_timeout(timeout_ms);
    
    ssize_t bytes_received;
    if (timeout_ms >= 0) {
        bytes_received = mq_timedreceive(mqd_,
                                        reinterpret_cast<char*>(buffer.data()),
                                        buffer.size(),
                                        &received_priority,
                                        &timeout);
    } else {
        bytes_received = mq_receive(mqd_,
                                   reinterpret_cast<char*>(buffer.data()),
                                   buffer.size(),
                                   &received_priority);
    }
    
    if (bytes_received == -1) {
        if (errno == EAGAIN || errno == ETIMEDOUT) {
            return {};  // 超时或队列空
        }
        throw std::system_error(errno, std::system_category(),
                               "Failed to receive message");
    }
    
    // 调整缓冲区大小为实际接收到的数据大小
    buffer.resize(bytes_received);
    return buffer;
}

message_queue::Attributes message_queue::get_attributes() const {
    Attributes attr;
    
    if (!is_valid()) {
        return attr;
    }
    
    struct mq_attr mqattr;
    if (mq_getattr(mqd_, &mqattr) == -1) {
        throw std::system_error(errno, std::system_category(),
                               "Failed to get queue attributes");
    }
    
    attr.max_messages = static_cast<size_t>(mqattr.mq_maxmsg);
    attr.max_message_size = static_cast<size_t>(mqattr.mq_msgsize);
    attr.current_messages = static_cast<size_t>(mqattr.mq_curmsgs);
    
    return attr;
}

void message_queue::clear() {
    if (!is_valid()) {
        return;
    }
    
    // 非阻塞接收所有消息 - 使用立即超时
    unsigned int priority;
    std::vector<uint8_t> buffer(config_.max_message_size);
    
    // 设置立即超时（非阻塞）
    timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    // timeout 设置为当前时间，表示不等待，立即超时
    
    while (true) {
        ssize_t bytes = mq_timedreceive(mqd_,
                                       reinterpret_cast<char*>(buffer.data()),
                                       buffer.size(),
                                       &priority,
                                       &timeout);
        
        if (bytes == -1) {
            break;
        }
        
        // 每次循环更新超时时间为当前时间，确保非阻塞
        clock_gettime(CLOCK_REALTIME, &timeout);
    }
}

timespec message_queue::calculate_timeout(int timeout_ms) const {
    timespec timeout;
    
    if (timeout_ms == 0) {
        // 非阻塞 - 立即超时
        clock_gettime(CLOCK_REALTIME, &timeout);
    } else if (timeout_ms > 0) {
        // 必须使用 CLOCK_REALTIME 的绝对时间
        // mq_timedreceive/mq_timedsend 要求使用 CLOCK_REALTIME
        clock_gettime(CLOCK_REALTIME, &timeout);
        
        // 加上超时偏移量
        timeout.tv_sec += timeout_ms / 1000;
        timeout.tv_nsec += (timeout_ms % 1000) * 1000000;
        
        // 处理纳秒溢出
        if (timeout.tv_nsec >= 1000000000) {
            timeout.tv_sec += timeout.tv_nsec / 1000000000;
            timeout.tv_nsec = timeout.tv_nsec % 1000000000;
        }
    } else {
        // 阻塞模式，返回当前时间（会被忽略）
        clock_gettime(CLOCK_REALTIME, &timeout);
    }
    
    return timeout;
}

} // namespace interprocess
} // namespace mc 