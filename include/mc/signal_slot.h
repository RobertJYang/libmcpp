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
 * @tparam Signature 信号函数签名
 */
template <typename Signature>
class signal : public boost::signals2::signal<Signature> {
public:
    using base_type = boost::signals2::signal<Signature>;
    using connection_type = boost::signals2::connection;
    using slot_type = typename base_type::slot_type;

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
        return base_type::connect(slot);
    }

    /**
     * @brief 断开所有连接
     */
    void disconnect_all() {
        base_type::disconnect_all_slots();
    }

    /**
     * @brief 检查信号是否有连接的槽
     * @return 如果有连接的槽则返回true，否则返回false
     */
    bool empty() const {
        return base_type::empty();
    }
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