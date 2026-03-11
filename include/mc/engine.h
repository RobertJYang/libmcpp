/**
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
#ifndef MC_ENGINE_H
#define MC_ENGINE_H

#include <mc/engine/engine.h>
#include <mc/engine/errors/std_errors.h>
#include <mc/engine/interface.h>
#include <mc/engine/macro.h>
#include <mc/engine/object.h>
#include <mc/engine/service.h>
#include <mc/engine/std_interface.h>

namespace mc::engine {

inline engine& get_engine()
{
    return mc::engine::engine::get_instance();
}

template <typename Table>
Table& get_table(std::string_view table_name)
{
    return mc::engine::engine::get_instance().get_table<Table>(table_name);
}

inline object_table& get_object_table()
{
    return get_engine().get_object_table();
}

} // namespace mc::engine

namespace mc {

using engine::get_engine;
using engine::get_object_table;
using engine::get_table;

} // namespace mc

#endif // MC_ENGINE_H
