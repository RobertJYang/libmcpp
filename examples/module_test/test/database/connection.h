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

#ifndef MC_TEST_DATABASE_CONNECTION_H
#define MC_TEST_DATABASE_CONNECTION_H

#include "base.h"
#include <map>
#include <string>
#include <vector>

namespace mc::test::database {

/**
 * @brief 数据库连接类
 */
class db_connection {
public:
    MC_REFLECTABLE("DbConnection")

    db_connection()  = default;
    ~db_connection() = default;

    /**
     * @brief 连接到数据库
     */
    bool connect(const std::string& host, const std::string& db_name) {
        m_host    = host;
        m_db_name = db_name;
        m_status  = db_status::CONNECTING;

        // 模拟连接过程
        if (!host.empty() && !db_name.empty()) {
            m_status = db_status::CONNECTED;
            return true;
        }

        m_status = db_status::ERROR;
        return false;
    }

    /**
     * @brief 断开连接
     */
    void disconnect() {
        m_status = db_status::DISCONNECTED;
        m_host.clear();
        m_db_name.clear();
    }

    /**
     * @brief 执行SQL查询
     */
    std::vector<std::string> execute_query(const std::string& sql) {
        if (m_status != db_status::CONNECTED) {
            return {};
        }

        m_last_query = sql;
        // 模拟查询结果
        return {"row1_col1", "row1_col2", "row2_col1", "row2_col2"};
    }

    /**
     * @brief 执行SQL更新
     */
    int execute_update(const std::string& sql) {
        if (m_status != db_status::CONNECTED) {
            return -1;
        }

        m_last_query = sql;
        // 模拟影响的行数
        return 1;
    }

    /**
     * @brief 获取连接状态
     */
    db_status get_status() const {
        return m_status;
    }

    /**
     * @brief 获取主机地址
     */
    const std::string& get_host() const {
        return m_host;
    }

    /**
     * @brief 获取数据库名称
     */
    const std::string& get_database_name() const {
        return m_db_name;
    }

    /**
     * @brief 设置数据库类型
     */
    void set_database_type(db_type type) {
        m_db_type = type;
    }

    /**
     * @brief 获取数据库类型
     */
    db_type get_database_type() const {
        return m_db_type;
    }

    /**
     * @brief 获取最后执行的查询
     */
    const std::string& get_last_query() const {
        return m_last_query;
    }

    /**
     * @brief 测试连接
     */
    bool test_connection() {
        return m_status == db_status::CONNECTED;
    }

private:
    std::string m_host;
    std::string m_db_name;
    db_status   m_status{db_status::DISCONNECTED};
    db_type     m_db_type{db_type::SQLITE};
    std::string m_last_query;
};

} // namespace mc::test::database

#endif // MC_TEST_DATABASE_CONNECTION_H