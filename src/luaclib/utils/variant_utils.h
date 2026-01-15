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

/**
 * @file variant_utils.h
 * @brief variant 和 variants 与 Lua 堆栈之间的转换工具函数
 */
#ifndef MC_LUACLIB_UTILS_VARIANT_UTILS_H
#define MC_LUACLIB_UTILS_VARIANT_UTILS_H

#include <mc/common.h>
#include <mc/variant.h>
#include <mc/variant/variants.h>

// Lua 前向声明
struct lua_State;

namespace mc {
namespace lua {

/**
 * @brief 将 variant 推入 Lua 堆栈
 * @param L Lua 状态机
 * @param v variant 值
 * @return 推入堆栈的元素数量（通常为 1）
 * @throw mc::exception 如果转换失败
 */
MC_API int variant_to_lua(::lua_State* L, const variant& v);

/**
 * @brief 将 variants 作为多个返回值推入 Lua 堆栈
 * @param L Lua 状态机
 * @param vs variants 值
 * @return 推入堆栈的元素数量（等于 variants 的大小）
 * @throw mc::exception 如果转换失败
 * @note 如果需要将 variants 作为单个 table 推入，请使用 variant_to_lua(L, vs)
 */
MC_API int variants_to_lua(::lua_State* L, const variants& vs);

/**
 * @brief 从 Lua 堆栈读取一个值并转换为 variant
 * @param L Lua 状态机
 * @param index 堆栈索引（可以是负数，表示从栈顶开始计数）
 * @return variant 值
 * @throw mc::exception 如果转换失败或索引无效
 */
MC_API variant lua_to_variant(::lua_State* L, int index);

/**
 * @brief 从 Lua 堆栈读取多个值并转换为 variants
 * @param L Lua 状态机
 * @param start_index 起始堆栈索引
 * @param count 要读取的元素数量（如果为 -1，则读取到栈顶）
 * @return variants 值
 * @throw mc::exception 如果转换失败或索引无效
 */
MC_API variants lua_to_variants(::lua_State* L, int start_index, int count);

/**
 * @brief 从 Lua 堆栈读取从指定索引到栈顶的所有值并转换为 variants
 * @param L Lua 状态机
 * @param start_index 起始堆栈索引
 * @return variants 值
 * @throw mc::exception 如果转换失败或索引无效
 */
MC_API variants lua_to_variants(::lua_State* L, int start_index);

} // namespace lua
} // namespace mc

#endif // MC_LUACLIB_UTILS_VARIANT_UTILS_H
