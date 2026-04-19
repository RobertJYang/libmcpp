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

// 从 class_name 重建 abstract_object 的轻量工厂。
// 所有 API noexcept；未注册 / new 失败返回空 shared_ptr。线程安全。

namespace mc::engine {

using abstract_object_factory_fn = mc::shared_ptr<abstract_object> (*)() noexcept;

// class_name 必须是静态生命周期字符串。重复注册幂等覆盖。
MC_API void register_abstract_object_factory(mc::string_view            class_name,
                                             abstract_object_factory_fn factory) noexcept;

MC_API abstract_object_factory_fn find_abstract_object_factory(mc::string_view class_name) noexcept;
MC_API mc::shared_ptr<abstract_object> try_create_abstract_object(mc::string_view class_name) noexcept;

// 仅 reset_for_test 用。
MC_API void clear_abstract_object_factories_for_test() noexcept;

namespace detail {

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
