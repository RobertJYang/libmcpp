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

#ifndef MC_ENGINE_INTERNAL_SHM_BINDING_H
#define MC_ENGINE_INTERNAL_SHM_BINDING_H

#include <cstddef>
#include <memory>
#include <string_view>

#include <mc/common.h>
#include <mc/string_view.h>

namespace mc::engine {

class abstract_object;
class service;
class service_object_table;
class object_table;

} // namespace mc::engine

// 业务侧 SHM 副作用收敛入口。state 为 null 的所有函数均 no-op。
// USE_SHM=OFF 时本头退化为 inline 空实现。

namespace mc::engine::shm_binding {

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM

struct service_state; // pImpl

MC_API service_state* create_service_state();
MC_API void           destroy_service_state(service_state* state) noexcept;

MC_API void ensure_object_handle(abstract_object& obj, service_state* state) noexcept;
MC_API void release_object_handle(abstract_object& obj) noexcept;
MC_API void sync_owner(abstract_object& self, abstract_object* old_owner,
                       abstract_object* new_owner) noexcept;
MC_API void sync_position(abstract_object& self, std::string_view position) noexcept;
MC_API void notify_after_register() noexcept;

// 失败抛 mc::invalid_arg_exception。
MC_API void attach_and_recover(service_state* state, mc::string_view svc_name,
                               service_object_table& table);
MC_API void detach(service_state* state) noexcept;
MC_API std::size_t gc_isolated(service_state* state, service_object_table& table) noexcept;

MC_API std::shared_ptr<service_object_table> create_service_object_table(mc::string_view name);
MC_API std::shared_ptr<object_table>         create_global_object_table();

MC_API void reset_runtime_for_test() noexcept;

#else // MCENGINE_USE_SHM = OFF

struct service_state {};

inline service_state* create_service_state()
{
    return nullptr;
}
inline void destroy_service_state(service_state*) noexcept
{}

inline void ensure_object_handle(abstract_object&, service_state*) noexcept
{}
inline void release_object_handle(abstract_object&) noexcept
{}
inline void sync_owner(abstract_object&, abstract_object*, abstract_object*) noexcept
{}
inline void sync_position(abstract_object&, std::string_view) noexcept
{}
inline void notify_after_register() noexcept
{}

inline void attach_and_recover(service_state*, mc::string_view, service_object_table&)
{}
inline void detach(service_state*) noexcept
{}
inline std::size_t gc_isolated(service_state*, service_object_table&) noexcept
{
    return 0U;
}

std::shared_ptr<service_object_table> create_service_object_table(mc::string_view name);
std::shared_ptr<object_table>         create_global_object_table();

inline void reset_runtime_for_test() noexcept
{}

#endif

} // namespace mc::engine::shm_binding

#endif // MC_ENGINE_INTERNAL_SHM_BINDING_H
