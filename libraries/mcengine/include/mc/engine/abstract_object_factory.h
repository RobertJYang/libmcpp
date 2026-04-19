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

#ifndef MC_ENGINE_ABSTRACT_OBJECT_FACTORY_H
#define MC_ENGINE_ABSTRACT_OBJECT_FACTORY_H

#include <mc/common.h>
#include <mc/engine/base.h>
#include <mc/memory.h>
#include <mc/string_view.h>

#include <type_traits>

// mc::engine::abstract_object_factory
//
// 从 class_name 重建 abstract_object 的轻量工厂
//
// 设计动机：
//   - reflection_factory 已能从 class_name 创建对象，但其返回 std::shared_ptr<T>
//     (典型实现 std::make_shared)，与 mcengine 使用的侵入式 mc::shared_ptr
//     ownership 模型不兼容（两套引用计数会导致 use-after-free）
//   - 因此 mcengine 维护独立的轻量类工厂：每个 engine::object<T> 在静态初始化期
//     注册一个 () -> mc::shared_ptr<abstract_object> 的工厂函数
//
// 工作流：
//   1. engine::object<T>::metadata() 首次访问时通过 abstract_object_caster_registrar<T>
//      把 (class_name, factory_fn) 注册到全局表
//   2. shm_storage_engine.recover() 通过 try_create_abstract_object(class_name)
//      用工厂创建 abstract_object 实例；property/identity 字段由 caller 回填
//
// 错误语义：
//   - 类未注册（class_name 在表里找不到）→ 返回空 shared_ptr
//   - 工厂内 new 抛异常（OOM 等）→ caster 内部捕获并返回空 shared_ptr
//   - caller 收到空 shared_ptr 时应把 shm_object 标 isolated 并日志
//
// 线程安全：注册和查找都用内部互斥保护

namespace mc::engine {

using abstract_object_factory_fn = mc::shared_ptr<abstract_object> (*)() noexcept;

// 注册一对 (class_name, factory)。重复注册同一 class_name 视为幂等覆盖
// （多 TU 静态初始化期触发常态）。class_name 必须是静态生命周期字符串
// （例如 T::class_name 字面量），表里只存 string_view 不拷贝。
MC_API void register_abstract_object_factory(mc::string_view             class_name,
                                             abstract_object_factory_fn  factory) noexcept;

// 查找 factory；未注册返回 nullptr。
MC_API abstract_object_factory_fn find_abstract_object_factory(mc::string_view class_name) noexcept;

// 高层入口：等价于 find_abstract_object_factory + 调用。
// class_name 不存在 / 工厂返回空 / 任一步失败 → 返回空 shared_ptr。
MC_API mc::shared_ptr<abstract_object> try_create_abstract_object(mc::string_view class_name) noexcept;

// reconstruct 一站式入口：CRC 校验 + factory 查找 + identity 字段绑定 +
// property slab 反向回填。shm_storage_engine.recover() 的标准 reconstruct_fn 实现。
//
// 失败语义（按顺序）：
//   - sh == nullptr                 → 直接返回空（不动 sh）
//   - shm_object_check 不通过        → sh.flags |= isolated，warn，返回空
//   - class_name 未注册到 factory    → sh.flags |= isolated，warn，返回空
//   - factory new 抛异常             → 返回空（factory 内部已捕获）
//   - 成功                           → 返回 abstract_object，identity + property
//                                      已与 sh 对齐；m_shm_handle = sh
//
// 调用方（recover() 主循环）只需对空返回值做 skip，不需要二次 CRC / isolation 处理。
//
// 仅在 USE_SHM=ON 编译时存在该函数；在 OFF 模式下不会被链接（reconstruct_fn 路径
// 整体被 #if 保护）。
MC_API mc::shared_ptr<abstract_object> try_reconstruct_object(shm_object* sh) noexcept;

// 测试辅助：清空 factory 表（reset_for_test 路径用）。生产代码不应调用。
MC_API void clear_abstract_object_factories_for_test() noexcept;

namespace detail {

// 模板辅助：在 engine::object<T> 静态初始化期调用一次。
// T 必须满足 std::is_base_of_v<abstract_object, T>，且必须可默认构造。
template <typename T>
struct abstract_object_caster_registrar {
    abstract_object_caster_registrar() noexcept
    {
        static_assert(std::is_base_of_v<abstract_object, T>,
                      "abstract_object_caster_registrar: T must derive from abstract_object");
        static_assert(std::is_default_constructible_v<T>,
                      "abstract_object_caster_registrar: T must be default-constructible");

        constexpr abstract_object_factory_fn fn = []() noexcept -> mc::shared_ptr<abstract_object> {
            try {
                return mc::shared_ptr<abstract_object>(new T());
            } catch (...) {
                return {};
            }
        };

        register_abstract_object_factory(T::class_name, fn);
    }
};

}  // namespace detail

}  // namespace mc::engine

#endif  // MC_ENGINE_ABSTRACT_OBJECT_FACTORY_H
