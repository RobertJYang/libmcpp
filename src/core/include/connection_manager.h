/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_CORE_INCLUDE_CONNECTION_MANAGER_H
#define MC_CORE_INCLUDE_CONNECTION_MANAGER_H

#include <mc/core/object.h>
#include <mc/signal_slot.h>

#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc::core {

/**
 * @brief 连接管理器，用于管理信号连接
 */
class connection_manager {
public:
    struct connection_info : public mc::noncopyable {
        connection_info(signal_type sig, mc::connection_type conn) : sig(sig), conn(std::move(conn))
        {}

        connection_info(connection_info&& other) noexcept : sig(std::move(other.sig)), conn(std::move(other.conn))
        {}

        connection_info& operator=(connection_info&& other) noexcept
        {
            sig  = std::move(other.sig);
            conn = std::move(other.conn);
            return *this;
        }

        signal_type         sig;
        mc::connection_type conn;
    };

    using connection_map_type        = std::unordered_map<connection_id_type, connection_info>;
    using connection_id_list_type    = std::vector<connection_id_type>;
    using signal_connection_map_type = std::unordered_map<signal_type, connection_id_list_type>;

    /**
     * @brief 添加连接（非线程安全，应由调用者确保同步）
     * @param sig 信号指针
     * @param conn 连接对象
     * @param id 连接ID，如果为0则自动生成
     * @return 连接ID
     */
    connection_id_type add_connection(signal_type sig, mc::connection_type conn,
                                      connection_id_type id = INVALID_CONNECTION_ID);

    /**
     * @brief 移除连接（非线程安全，应由调用者确保同步）
     * @param id 连接ID
     */
    void remove_connection(connection_id_type id);

    /**
     * @brief 移除信号的所有连接（非线程安全，应由调用者确保同步）
     * @param sig 信号指针
     * @return 移除的连接数量
     */
    size_t remove_connections(signal_type sig);

    /**
     * @brief 清空所有连接（非线程安全，应由调用者确保同步）
     */
    void clear();

private:
    connection_id_type new_id();

    connection_map_type        m_connections;
    signal_connection_map_type m_signal_connections;
    connection_id_type         m_next_id{1};
};

} // namespace mc::core

#endif // MC_CORE_INCLUDE_CONNECTION_MANAGER_H