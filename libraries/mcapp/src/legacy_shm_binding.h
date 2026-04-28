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

#ifndef MC_APP_LEGACY_SHM_BINDING_H
#define MC_APP_LEGACY_SHM_BINDING_H

// 旧 dbus/shm 兼容粘合层。
//
// 当顶层 use_old_shm=true（同时 use_shm=false，二者互斥）时，mcapp::service
// 会在 bring_up_dbus_transport() 中实例化 legacy_shm::binding，把 service 的
// 对象表挂载到旧 harbor / shm_tree 之上：
//   - 启动 harbor（进程级 singleton），注册 unique_name → service_name 映射
//   - 注册 method handler：harbor 收到的跨进程调用会转换为 mc::engine::message
//     方法调用，走 mc::engine::dispatch 派发到目标对象，再把 method_return 的
//     value 解出回填给 harbor
//   - 订阅 service.get_object_table().on_object_added/on_object_removed，
//     在共享内存对象树中同步注册/反注册对象
//
// 整个粘合层与 mcengine 保持纯解耦：mcengine 完全不感知 dbus / shm / harbor，
// 仅通过对象表的 mc::signal 与 mc::engine::dispatch 暴露生命周期与消息钩子。
// 粘合层失效或 use_old_shm=false 时，对 mcengine 零侵入。
//
// 这里复用 mcdbus 的 MCDBUS_USE_OLD_SHM 宏（由 mcdbus_dep 透传），保证 mcapp
// 与 mcdbus 对"是否走旧 shm"的判断永远一致。

#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM

#include <mc/signal/connection.h>
#include <mc/string.h>

#include <memory>
#include <string>

namespace mc {
class object_base;
} // namespace mc

namespace mc::dbus {
class connection;
class shm_tree;
} // namespace mc::dbus

namespace mc::engine {
class abstract_object;
class service;
} // namespace mc::engine

namespace mc::app::legacy_shm {

// 单 service 的旧 shm 兼容粘合实例。生命周期由 mcapp::service 管理。
// install/uninstall 幂等。harbor 是进程级 singleton，多个 service 实例共享
// 同一 harbor，按需懒启动。
class binding {
public:
    binding(mc::engine::service& svc, mc::dbus::connection& conn);
    ~binding();

    binding(const binding&)            = delete;
    binding& operator=(const binding&) = delete;
    binding(binding&&)                 = delete;
    binding& operator=(binding&&)      = delete;

    // service_name 与 unique_name 都非空时尝试 install；返回是否成功挂载。
    bool install();
    void uninstall();

    bool installed() const noexcept
    {
        return m_installed;
    }

private:
    void on_object_added_signal(mc::object_base& obj);
    void on_object_removed_signal(mc::object_base& obj);

    void register_to_shm(mc::engine::abstract_object& obj);
    void unregister_from_shm(mc::string_view path);

    mc::engine::service&                m_service;
    mc::dbus::connection&               m_connection;
    std::string                         m_service_name;
    std::string                         m_unique_name;
    std::unique_ptr<mc::dbus::shm_tree> m_shm_tree;
    bool                                m_installed{false};
    mc::scoped_connection               m_added_conn;
    mc::scoped_connection               m_removed_conn;
};

} // namespace mc::app::legacy_shm

#endif // MCDBUS_USE_OLD_SHM

#endif // MC_APP_LEGACY_SHM_BINDING_H
