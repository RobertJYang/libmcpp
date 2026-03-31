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

#ifndef MC_DATABASE_COMMON_H
#define MC_DATABASE_COMMON_H

#include <mc/common.h>
#include <mc/object_base.h>

namespace mc::db {

/**
 * @brief 数据库对象ID类型（与 mc::object_id_type 相同）
 */
using db_object_id_type = mc::object_id_type;

/**
 * @brief 数据库对象指针类型
 */
using db_object_ptr = mc::shared_ptr<mc::object_base>;

// 为了向后兼容，保留旧的类型别名
using object_id_type = db_object_id_type;
using object_base    = mc::object_base;
using object_ptr     = db_object_ptr;

} // namespace mc::db

#endif // MC_DATABASE_COMMON_H
