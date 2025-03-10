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

#ifndef MC_SIGNAL_SLOT_H
#define MC_SIGNAL_SLOT_H

#include <algorithm>
#include <boost/signals2.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace mc {

/**
 * @brief 信号类模板，用于实现信号槽机制
 *
 * 信号类提供以下功能：
 * - 信号连接与断开
 * - 信号触发
 * - 自动断开连接
 *
 * @tparam Args 信号参数类型列表
 */
template <typename... Args>
class signal {
public:
    using slot_type = std::function<void(Args...)>;
    using connection_type = boost::signals2::connection;

    /**
     * @brief 构造函数
     */
    signal() = default;

    /**
     * @brief 析构函数
     */
    ~signal() = default;

    /**
     * @brief 连接信号到槽函数
     * @param slot 槽函数
     * @return 连接对象，可用于手动断开连接
     */
    connection_type connect(const slot_type& slot) {
        return signal_.connect(slot);
    }

    /**
     * @brief 断开所有连接
     */
    void disconnect_all() {
        signal_.disconnect_all_slots();
    }

    /**
     * @brief 触发信号
     * @param args 信号参数
     */
    void emit(Args... args) {
        signal_(args...);
    }

    /**
     * @brief 重载函数调用运算符，用于触发信号
     * @param args 信号参数
     */
    void operator()(Args... args) {
        emit(args...);
    }

    /**
     * @brief 检查信号是否有连接的槽
     * @return 如果有连接的槽则返回true，否则返回false
     */
    bool empty() const {
        return signal_.empty();
    }

    // 为了保持与原有API兼容，提供connection类型别名
    using connection = connection_type;

private:
    boost::signals2::signal<void(Args...)> signal_;
};

/**
 * @brief 连接管理器，用于自动管理信号连接的生命周期
 */
class connection_manager {
public:
    /**
     * @brief 构造函数
     */
    connection_manager() = default;

    /**
     * @brief 析构函数，会自动断开所有连接
     */
    ~connection_manager() {
        disconnect_all();
    }

    /**
     * @brief 添加连接
     * @param connection 要管理的连接对象
     */
    template <typename... Args>
    void add(const boost::signals2::connection& connection) {
        connections_.push_back(connection);
    }

    /**
     * @brief 断开所有连接
     */
    void disconnect_all() {
        for (auto& connection : connections_) {
            connection.disconnect();
        }
        connections_.clear();
    }

private:
    std::vector<boost::signals2::connection> connections_;
};

} // namespace mc

#endif // MC_SIGNAL_SLOT_H