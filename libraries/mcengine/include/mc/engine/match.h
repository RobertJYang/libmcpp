/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_ENGINE_MATCH_H
#define MC_ENGINE_MATCH_H

#include <mc/engine/match/condition_filter.h>
#include <mc/engine/match/endpoint_sweep.h>
#include <mc/engine/match/filter.h>
#include <mc/engine/match/path_pattern.h>
#include <mc/engine/match/rule.h>
#include <mc/engine/match/table.h>
#include <mc/engine/match/types.h>

namespace mc::engine {

using match::matches;
using match::condition_filter;
using match::condition_filter_backend;
using match::filter_backend;
using match::filter_backend_ptr;
using match::filter_backend_registry;
using match::filter_compiled;
using match::filter_compiled_ptr;
using match::filter_spec;
using match::make_condition_filter;
using match::match_callback;
using match::match_id;
using match::match_rule;
using match::no_filter;
using match::register_condition_filter_backend;

} // namespace mc::engine

#endif // MC_ENGINE_MATCH_H
