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

#include "base.h"
#include "connection.h"

// 导出数据库状态枚举到模块
MC_MODULE_REFLECT_ENUM(mc_test_database,
                       mc::test::database::db_status,
                       (DISCONNECTED)(CONNECTING)(CONNECTED)(ERROR))

// 导出数据库类型枚举到模块
MC_MODULE_REFLECT_ENUM(mc_test_database,
                       mc::test::database::db_type,
                       (SQLITE)(MYSQL)(POSTGRESQL))

// 导出数据库连接类到模块
MC_MODULE_REFLECT(mc_test_database,
                  mc::test::database::db_connection,
                  ((connect, "connect"))                   // 连接
                  ((disconnect, "disconnect"))             // 断开连接
                  ((execute_query, "executeQuery"))        // 执行查询
                  ((execute_update, "executeUpdate"))      // 执行更新
                  ((get_status, "getStatus"))              // 获取状态
                  ((get_host, "getHost"))                  // 获取主机
                  ((get_database_name, "getDatabaseName")) // 获取数据库名
                  ((set_database_type, "setDatabaseType")) // 设置数据库类型
                  ((get_database_type, "getDatabaseType")) // 获取数据库类型
                  ((get_last_query, "getLastQuery"))       // 获取最后查询
                  ((test_connection, "testConnection")))   // 测试连接

MC_EXPORT_MODULE(mc_test_database)