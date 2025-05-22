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

#ifndef MC_ENGINE_MACRO_H
#define MC_ENGINE_MACRO_H

namespace mc::engine {

#define MC_INTERFACE(name)                                                                         \
    static constexpr std::string_view interface_name = name;                                       \
    static_assert(mc::engine::detail::is_valid_interface_name(name),                               \
                  "接口名称必须符合规范: "                                                         \
                  "使用点分隔的单词，每个单词以字母开头且只包含字母、数字和下划线");

#define MC_OBJECT_INTERFACE_II(r, _, INTERFACE) std::tuple<INTERFACE*>(),
#define MC_OBJECT_INTERFACE_I(INTERFACES)                                                          \
    BOOST_PP_SEQ_FOR_EACH(MC_OBJECT_INTERFACE_II, _, INTERFACES)

#define MC_OBJECT_INTERFACE(...)                                                                   \
    BOOST_PP_IIF(BOOST_PP_IS_EMPTY(__VA_ARGS__), BOOST_PP_EMPTY, MC_OBJECT_INTERFACE_I)(__VA_ARGS__)

#define MC_OBJECT(OBJECT_TYPE, CLASS_NAME, PATH_PATTERN, ...)                                      \
    using object_type = OBJECT_TYPE;                                                               \
    friend struct mc::reflect::reflector<OBJECT_TYPE>;                                             \
    static constexpr std::string_view path_pattern = PATH_PATTERN;                                 \
    static constexpr std::string_view class_name   = CLASS_NAME;                                   \
    template <typename Members>                                                                    \
    static constexpr bool check_members(const Members& members) {                                  \
        constexpr auto interfaces =                                                                \
            std::tuple_cat(MC_OBJECT_INTERFACE(__VA_ARGS__) std::tuple<>());                       \
        return mc::engine::detail::check_members(interfaces, members);                             \
    }

} // namespace mc::engine

#endif // MC_ENGINE_MACRO_H
