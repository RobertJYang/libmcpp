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
#ifndef MC_DATABASE_QUERY_QUERY_H
#define MC_DATABASE_QUERY_QUERY_H

#include <mc/db/query/builder.h>
#include <mc/db/query/condition.h>
#include <mc/db/query/metadata.h>
#include <mc/db/query/planner.h>
#include <mc/db/query/table_query.h>

namespace mc::db {
using query::compare_op;
using query::condition;
using query::field;
using query::logical_op;
using query::query_builder;
using query::query_plan;
using query::query_plan_type;
using query::query_range;
using query::table_query;
using query::to_condition;

namespace conditions = query::conditions;

using namespace query::conditions;
} // namespace mc::db

#endif // MC_DATABASE_QUERY_QUERY_H