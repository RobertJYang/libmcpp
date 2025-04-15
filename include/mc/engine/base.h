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

#ifndef MC_ENGINE_BASE_H
#define MC_ENGINE_BASE_H

#include <mc/engine/utils.h>
#include <mc/reflect.h>
#include <mc/signal_slot.h>
#include <mc/traits.h>
#include <mc/variant.h>

#include <boost/preprocessor/seq/for_each.hpp>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace mc::engine {
using slot_type = std::function<mc::variant(const mc::variants&)>;

struct interface_base {
    virtual ~interface_base() = default;

    virtual std::string_view    get_interface_name() const                                     = 0;
    virtual mc::connection_type connect(std::string_view signal_name, slot_type slot)          = 0;
    virtual mc::variant         emit(std::string_view signal_name, const mc::variants& args)   = 0;
    virtual mc::variant         invoke(std::string_view method_name, const mc::variants& args) = 0;
    virtual mc::variant         get_property(std::string_view property_name)                   = 0;
    virtual bool set_property(std::string_view property_name, const mc::variant& value)        = 0;
};

class object_base {
public:
    virtual ~object_base() = default;

    virtual const std::string& get_object_name() const                = 0;
    virtual void               set_object_name(std::string_view name) = 0;
    virtual const std::string& get_object_path() const                = 0;
    virtual void               set_object_path(std::string_view path) = 0;

    virtual bool            has_interface(std::string_view interface_name) const = 0;
    virtual interface_base* get_interface(std::string_view interface_name)       = 0;

    virtual mc::variant get_property(std::string_view property_name,
                                     std::string_view interface_name = {}) = 0;
    virtual bool        set_property(std::string_view property_name, const mc::variant& value,
                                     std::string_view interface_name = {}) = 0;

    virtual mc::variant invoke(std::string_view method_name, const mc::variants& args,
                               std::string_view interface_name = {}) = 0;

    virtual mc::connection_type connect(std::string_view signal_name, slot_type slot,
                                        std::string_view interface_name = {}) = 0;
    virtual mc::variant         emit(std::string_view signal_name, const mc::variants& args,
                                     std::string_view interface_name = {})    = 0;
};

// 定义接口名称并在编译期检查其是否符合规范
#define MC_INTERFACE(name)                                                                         \
    static constexpr std::string_view interface_name = name;                                       \
    static_assert(mc::engine::detail::is_valid_interface_name(name),                               \
                  "接口名称必须符合规范: "                                                         \
                  "使用点分隔的单词，每个单词以字母开头且只包含字母、数字和下划线");

#define MC_OBJECT_INTERFACE(r, _, INTERFACE) std::tuple<INTERFACE*>(),

#define MC_OBJECT(PATH_PATTERN, INTERFACES)                                                        \
    static constexpr std::string_view path_pattern = PATH_PATTERN;                                 \
    static constexpr auto             interfaces =                                                 \
        std::tuple_cat(BOOST_PP_SEQ_FOR_EACH(MC_OBJECT_INTERFACE, _, INTERFACES) std::tuple<>());  \
    template <typename Members>                                                                    \
    static constexpr bool check_members(const Members& members) {                                  \
        return mc::engine::detail::check_members(interfaces, members);                             \
    }

} // namespace mc::engine

#endif // MC_ENGINE_BASE_H
