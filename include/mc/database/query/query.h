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

#include <mc/database/query/builder.h>
#include <mc/database/query/condition.h>
#include <mc/database/query/metadata.h>
#include <mc/database/query/planner.h>
#include <mc/database/query/proto_query.h>
#include <mc/database/query/table_query.h>

namespace mc::db {
// 导出便于使用的类型别名
using query::compare_op;
using query::condition;
using query::logical_op;
using query::query_builder;
using query::query_plan;
using query::query_plan_type;
using query::query_range;
using query::table_query;

// 导出 query_builder
using query::query_builder;

// 导出条件相关定义
using query::compare_op;
using query::condition;
using query::conditions::and_cond;
using query::conditions::or_cond;

// 导出 Proto 查询 DSL
namespace proto_query = query::dsl;
using query::between;
using query::contains;
using query::field;
using query::in;
using query::like;
using query::to_condition;

// 导出条件工厂函数
using namespace query::conditions;

// 导出Proto查询DSL相关函数和类型
using query::between;
using query::contains;
using query::field;
using query::in;
using query::like;
using query::to_condition;
} // namespace mc::db

#endif // MC_DATABASE_QUERY_QUERY_H