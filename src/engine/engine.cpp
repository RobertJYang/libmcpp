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

#include <mc/engine/engine.h>

namespace mc::engine {

engine::~engine() {
}

engine::engine() {
    // 初始化对象引擎
}

void engine::init() {
    // 初始化对象引擎
}

void engine::run() {
    // 运行对象引擎
}

void engine::stop() {
    // 停止对象引擎
}

mc::db::database& engine::get_database() {
    return m_database;
}

} // namespace mc::engine
