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

/**
 * @file query.h
 * @brief 数据库表高级查询功能
 */
#ifndef MC_DATABASE_QUERY_H
#define MC_DATABASE_QUERY_H

#include <mc/database/query/builder.h>
#include <mc/database/query/condition.h>
#include <mc/database/query/field_accessor.h>
#include <mc/database/query/metadata.h>
#include <mc/database/query/planner.h>
#include <mc/database/query/table_query.h>

namespace mc::database {

// 为方便使用，引入query命名空间中的关键类型
using query::compare_op;
using query::index_metadata;
using query::index_type;
using query::make_field_accessor;
using query::make_table_query;
using query::query_builder;
using query::table_query;

} // namespace mc::database

#endif // MC_DATABASE_QUERY_H