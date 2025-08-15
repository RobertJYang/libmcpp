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

#ifndef MC_TEST_DATABASE_BASE_H
#define MC_TEST_DATABASE_BASE_H

#include <mc/module.h>

MC_MODULE(mc_test_database)

namespace mc::test::database {

/**
 * @brief 数据库连接状态枚举
 */
enum class db_status {
    DISCONNECTED, // 未连接
    CONNECTING,   // 连接中
    CONNECTED,    // 已连接
    ERROR         // 错误状态
};

/**
 * @brief 数据库类型枚举
 */
enum class db_type {
    SQLITE,    // SQLite 数据库
    MYSQL,     // MySQL 数据库
    POSTGRESQL // PostgreSQL 数据库
};

} // namespace mc::test::database

MC_REFLECTABLE("DbStatus", mc::test::database::db_status)
MC_REFLECTABLE("DbType", mc::test::database::db_type)

#endif // MC_TEST_DATABASE_BASE_H