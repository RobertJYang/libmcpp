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
#include <mc/engine/interface.h>
#include <mc/engine/macro.h>
#include <mc/engine/object.h>
#include <mc/engine/service.h>
#include <mc/engine/std_interface.h>

namespace mc::engine {

inline engine& get_engine() {
    return mc::engine::engine::get_instance();
}

inline strand_type make_strand() {
    return strand_type(get_engine().get_io_context().get_executor());
}

inline strand_type make_strand(io_context_type& io_context) {
    return strand_type(io_context.get_executor());
}

inline boost::asio::io_context& get_io_context() {
    return get_engine().get_io_context();
}

template <typename Table>
Table& get_table(std::string_view table_name) {
    return mc::engine::engine::get_instance().get_table<Table>(table_name);
}

inline object_table& get_object_table() {
    return get_engine().get_object_table();
}

} // namespace mc::engine

namespace mc {
using engine::io_context_type;
using engine::strand_type;

using engine::get_engine;
using engine::get_io_context;
using engine::get_object_table;
using engine::get_table;
using engine::make_strand;

} // namespace mc

#endif // MC_ENGINE_H
