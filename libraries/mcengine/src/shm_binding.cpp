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

#include <mc/engine/internal/shm_binding.h>

#include <mc/engine/base.h>
#include <mc/engine/object.h>
#include <mc/engine/service.h>

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
#include <unistd.h>

#include <string>
#include <unordered_set>
#include <vector>

#include <mc/engine/abstract_object_factory.h>
#include <mc/engine/shm_object.h>

#include "shm_object_ops.h"
#include "shm_property_sync.h"
#include "shm_runtime_provider.h"
#include "shm_service.h"
#include "shm_service_ops.h"
#include <mc/exception.h>
#include <mc/log/log.h>
#endif

namespace mc::engine::shm_binding {

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM

// service_state 是 binding 内部状态：
//   - m_shm_service：本进程持有的 shm_service POD 指针（attach 后写入 / detach 后清零）
//   - m_isolated_path_hints：attach_and_recover 期间扫到的 isolated 候选 path，
//     gc_isolated 据此走 O(K) 直查路径；回收后清空。
struct service_state {
    shm_service*             m_shm_service{nullptr};
    std::vector<std::string> m_isolated_path_hints;
};

namespace {

// 全局 well-known shm_service_map name：所有 mcengine 进程共享一份，
// 作为 service attach 入口表。
constexpr mc::string_view k_service_map_name = "mcengine.service_map";

shm_service_map _open_service_map()
{
    auto& rt  = shm_runtime_provider::instance();
    auto  opt = rt.get_or_create_map<shm_byte_string,
                                     shm_ptr<shm_service>,
                                     shm_byte_string_less>(k_service_map_name);
    MC_ASSERT_THROW(static_cast<bool>(opt), mc::invalid_arg_exception,
                    "shm_binding: 无法 lazy 创建 shm_service_map (name=${n})",
                    ("n", std::string(k_service_map_name)));
    return std::move(*opt);
}

// reconstruct_fn：CRC 校验 + factory 查找 + identity 字段绑定 + property slab 反向回填。
// 任一步失败 → sh.flags |= isolated；recover() 主循环按空返回值跳过。
mc::shared_ptr<abstract_object> _reconstruct_object(shm_object* sh) noexcept
{
    if (sh == nullptr) {
        return {};
    }
    if (!shm_object_check(*sh)) {
        sh->flags |= shm_object_flags::isolated;
        wlog("shm_object CRC 校验失败 → 隔离 object_id=${id}", ("id", sh->object_id));
        return {};
    }

    auto cn = shm_object_class_name(*sh);
    if (cn.empty()) {
        sh->flags |= shm_object_flags::isolated;
        wlog("shm_object class_name 为空 → 隔离 object_id=${id}", ("id", sh->object_id));
        return {};
    }

    auto obj = try_create_abstract_object(cn);
    if (!obj) {
        sh->flags |= shm_object_flags::isolated;
        wlog("class 未注册 → 隔离 class_name=${c} object_id=${id}",
             ("c", std::string(cn))("id", sh->object_id));
        return {};
    }

    try {
        obj->set_shm_handle(sh);
        obj->set_object_id(sh->object_id);
        obj->set_object_name(std::string(shm_object_name(*sh)));
        obj->set_object_path(std::string(shm_object_path(*sh)));
        obj->set_position(std::string(shm_object_position(*sh)));
        shm_load_properties_into(*obj, *sh);
    } catch (const std::exception& e) {
        sh->flags |= shm_object_flags::isolated;
        wlog("reconstruct 异常 → 隔离 class_name=${c} object_id=${id} err=${e}",
             ("c", std::string(cn))("id", sh->object_id)("e", e.what()));
        return {};
    } catch (...) {
        sh->flags |= shm_object_flags::isolated;
        wlog("reconstruct 未知异常 → 隔离 class_name=${c} object_id=${id}",
             ("c", std::string(cn))("id", sh->object_id));
        return {};
    }

    return obj;
}

// 整表 clear 的批量回收钩子：shm_storage_engine.clear() 解除 map 引用前
// 先把 ShmRecord 自身（含 byte_string / property slab / child slab）释放，
// 否则会泄漏在 user_arena。单对象 unregister 走 release_object_handle，
// 不经此路径。
void _destroy_shm_record(shm_object* sh) noexcept
{
    if (sh == nullptr) {
        return;
    }
    auto& rt    = shm_runtime_provider::instance();
    auto  arena = rt.user_arena();
    shm_object_destroy(arena, sh);
}

void _recover_shm_record(shm_object* sh) noexcept
{
    if (sh == nullptr) {
        return;
    }
    auto& rt    = shm_runtime_provider::instance();
    auto  arena = rt.user_arena();
    shm_object_journal_recover(arena, *sh);
}

void _install_common_engine_hooks(service_object_table& tbl)
{
    auto& eng = tbl.engine();
    eng.set_shm_handle_extractor(
        [](const abstract_object& o) noexcept { return o.get_shm_handle(); });
    eng.set_recover_hook(&_recover_shm_record);
    eng.set_destroy_hook(&_destroy_shm_record);
    eng.set_reconstruct(&_reconstruct_object);
}

// global object_table 走另一组 hook（无 destroy_hook，因为它不持有
// ShmRecord 所有权——所有权在 service_object_table 那边）。
void _install_global_engine_hooks(object_table& tbl)
{
    auto& eng = tbl.engine();
    eng.set_shm_handle_extractor(
        [](const abstract_object& o) noexcept { return o.get_shm_handle(); });
    eng.set_recover_hook(&_recover_shm_record);
    eng.set_reconstruct(&_reconstruct_object);
}

}  // namespace

// ---------- service_state 生命周期 ----------

service_state* create_service_state()
{
    return new service_state();
}

void destroy_service_state(service_state* state) noexcept
{
    delete state;
}

// ---------- 写路径 ----------

void ensure_object_handle(abstract_object& obj, service_state* state) noexcept
{
    shm_service* svc_pod = (state != nullptr) ? state->m_shm_service : nullptr;
    if (auto* existing = obj.get_shm_handle(); existing != nullptr) {
        if (svc_pod != nullptr && shm_object_service(*existing) != svc_pod) {
            shm_object_set_service(*existing, svc_pod);
        }
        return;
    }
    try {
        auto& rt    = shm_runtime_provider::instance();
        auto  arena = rt.user_arena();
        auto* sh    = shm_object_create(arena,
                                        obj.get_object_id(),
                                        std::string_view(obj.get_class_name().data(),
                                                         obj.get_class_name().size()),
                                        std::string_view(obj.get_object_name().data(),
                                                         obj.get_object_name().size()),
                                        std::string_view(obj.get_object_path().data(),
                                                         obj.get_object_path().size()),
                                        std::string_view(obj.get_position().data(),
                                                         obj.get_position().size()));
        MC_ASSERT_THROW(sh != nullptr, mc::invalid_arg_exception,
                        "shm_binding.ensure_object_handle: shm_object_create returned null");
        if (svc_pod != nullptr) {
            shm_object_set_service(*sh, svc_pod);
        }
        obj.set_shm_handle(sh);
    } catch (...) {
        // 分配失败不向上抛：shm_handle 缺失会让后续 sync 路径全 silent skip，
        // 业务读路径仍走 heap，只是失去崩溃恢复能力
    }
}

void release_object_handle(abstract_object& obj) noexcept
{
    auto* sh = obj.get_shm_handle();
    if (sh == nullptr) {
        return;
    }
    auto& rt    = shm_runtime_provider::instance();
    auto  arena = rt.user_arena();
    // 先把 sh.service 置 null：万一别的进程刚好通过全局 object_table 持有这个
    // shm_object 指针在反查 service，至少能拿到 well-defined null。
    shm_object_set_service(*sh, nullptr);
    shm_object_destroy(arena, sh);
    obj.set_shm_handle(nullptr);
}

void sync_owner(abstract_object& self,
                abstract_object* old_owner,
                abstract_object* new_owner) noexcept
{
    auto* sh = self.get_shm_handle();
    if (sh == nullptr) {
        return;
    }
    auto* old_sh = (old_owner != nullptr) ? old_owner->get_shm_handle() : nullptr;
    auto* new_sh = (new_owner != nullptr) ? new_owner->get_shm_handle() : nullptr;
    if (old_sh == new_sh) {
        return;
    }
    if (old_sh != nullptr) {
        (void)shm_object_remove_child(*old_sh, sh);
    }
    shm_object_set_parent(*sh, new_sh);
    if (new_sh != nullptr && shm_runtime_provider::has_instance()) {
        try {
            auto& rt    = shm_runtime_provider::instance();
            auto  arena = rt.user_arena();
            (void)shm_object_add_child(arena, *new_sh, sh);
        } catch (...) {
            // arena 不可用：parent 字段已写好，children 端最坏缺一条反向边，
            // 由 _restore_owner_relations 的 path-find 兜底
        }
    }
}

void sync_position(abstract_object& self, std::string_view position) noexcept
{
    auto* sh = self.get_shm_handle();
    if (sh == nullptr || !shm_runtime_provider::has_instance()) {
        return;
    }
    try {
        auto& rt    = shm_runtime_provider::instance();
        auto  arena = rt.user_arena();
        (void)shm_object_set_position(arena, *sh, position);
    } catch (...) {
        // SHM 同步失败不向上抛
    }
}

void notify_after_register() noexcept
{
    shm_runtime_provider::check_arena_usage();
}

// ---------- service 生命周期 ----------

void attach_and_recover(service_state*        state,
                        mc::string_view       svc_name,
                        service_object_table& table)
{
    MC_ASSERT_THROW(state != nullptr, mc::invalid_arg_exception,
                    "shm_binding.attach_and_recover: state is null");

    auto& rt    = shm_runtime_provider::instance();
    auto  arena = rt.user_arena();
    auto  smap  = _open_service_map();

    auto current_pid = static_cast<std::uint32_t>(::getpid());
    state->m_shm_service =
        shm_service_attach(arena, smap, svc_name, current_pid);
    MC_ASSERT_THROW(state->m_shm_service != nullptr, mc::invalid_arg_exception,
                    "shm_binding.attach_and_recover: shm_service_attach 失败 name=${n} "
                    "（SHM 分配失败 或 同名 service 被另一活进程占用）",
                    ("n", std::string(svc_name)));

    table.engine().recover();

    // recover 后扫一遍 idx0：
    //   - 收集 isolated path 候选，供 gc_isolated 走 O(K) 直查
    //   - 强制刷新 shm_object.service，覆盖 takeover/detached 残留的旧指针
    state->m_isolated_path_hints.clear();
    auto& eng = table.engine();
    auto& m0  = eng.index_map(0U);
    for (auto it = m0.cbegin(); it != m0.cend(); ++it) {
        auto        kv = *it;
        shm_object* sh = (kv.value != nullptr) ? kv.value->get() : nullptr;
        if (sh == nullptr) {
            continue;
        }
        if ((sh->flags & shm_object_flags::isolated) != 0U) {
            auto sv = kv.key->as_string_view();
            state->m_isolated_path_hints.emplace_back(sv.data(), sv.size());
            continue;
        }
        if (shm_object_service(*sh) != state->m_shm_service) {
            shm_object_set_service(*sh, state->m_shm_service);
        }
    }
}

void detach(service_state* state) noexcept
{
    if (state == nullptr || state->m_shm_service == nullptr) {
        return;
    }
    shm_service_set_state(*state->m_shm_service, shm_service_state::detached);
    shm_service_set_pid(*state->m_shm_service, 0U);
    state->m_shm_service = nullptr;
    state->m_isolated_path_hints.clear();
}

std::size_t gc_isolated(service_state* state, service_object_table& table) noexcept
{
    auto& eng = table.engine();
    auto& m0  = eng.index_map(0U);

    std::unordered_set<shm_object*> targets;
    if (state != nullptr && !state->m_isolated_path_hints.empty()) {
        for (const auto& path : state->m_isolated_path_hints) {
            auto mp = m0.find(mc::string_view(path));
            if (!mp) {
                continue;
            }
            shm_object* sh = (mp.value != nullptr) ? mp.value->get() : nullptr;
            if (sh != nullptr && (sh->flags & shm_object_flags::isolated) != 0U) {
                targets.insert(sh);
            }
        }
        state->m_isolated_path_hints.clear();
    } else {
        for (auto it = m0.cbegin(); it != m0.cend(); ++it) {
            auto        kv = *it;
            shm_object* sh = (kv.value != nullptr) ? kv.value->get() : nullptr;
            if (sh != nullptr && (sh->flags & shm_object_flags::isolated) != 0U) {
                targets.insert(sh);
            }
        }
    }
    if (targets.empty()) {
        return 0U;
    }

    // 反扫每个 idx 找出指向 target 的 key 再 erase。
    // 不按 path / class_name+pos 反推：idx1 用的是上层 table 的复合 key 编码，
    // 这一层拿不到；O(N) 反扫在单 service 规模下足够。
    for (std::size_t i = 0; i < eng.index_count; ++i) {
        auto&                    m = eng.index_map(i);
        std::vector<std::string> keys_to_erase;
        for (auto it = m.cbegin(); it != m.cend(); ++it) {
            auto        kv = *it;
            shm_object* sh = (kv.value != nullptr) ? kv.value->get() : nullptr;
            if (sh != nullptr && targets.count(sh) != 0U) {
                auto sv = kv.key->as_string_view();
                keys_to_erase.emplace_back(sv.data(), sv.size());
            }
        }
        for (const auto& k : keys_to_erase) {
            (void)m.erase(mc::string_view(k));
        }
    }

    auto& rt    = shm_runtime_provider::instance();
    auto  arena = rt.user_arena();
    for (auto* sh : targets) {
        shm_object_destroy(arena, sh);
    }
    return targets.size();
}

// ---------- table 构造 ----------

std::shared_ptr<service_object_table> create_service_object_table(mc::string_view name)
{
    auto&          rt   = shm_runtime_provider::instance();
    std::string    base = std::string(name) + ".object_table";
    engine_alloc_t alloc(rt, base);
    auto           tbl = std::make_shared<service_object_table>(name, alloc);
    _install_common_engine_hooks(*tbl);
    return tbl;
}

std::shared_ptr<object_table> create_global_object_table()
{
    auto&          rt = shm_runtime_provider::instance();
    engine_alloc_t alloc(rt, "object_table");
    auto           tbl = std::make_shared<object_table>(alloc);
    _install_global_engine_hooks(*tbl);
    // 已存在的 SHM map（其他进程已注册过对象）需要 recover：把 SHM PODs
    // 重建为本进程 heap 端 abstract_object 实例。空 map 时 recover 是 no-op。
    tbl->engine().recover();
    return tbl;
}

// ---------- 测试 ----------

void reset_runtime_for_test() noexcept
{
    shm_runtime_provider::reset_for_test();
}

#else  // MCENGINE_USE_SHM = OFF

// OFF 模式：仅需提供 table 构造的非 inline 实现（这两个函数返回类型涉及完整定义，
// 不便在 header 内联）。其余 API 由 header 全部 inline no-op。

std::shared_ptr<service_object_table> create_service_object_table(mc::string_view name)
{
    return std::make_shared<service_object_table>(name);
}

std::shared_ptr<object_table> create_global_object_table()
{
    return std::make_shared<object_table>();
}

#endif

}  // namespace mc::engine::shm_binding
