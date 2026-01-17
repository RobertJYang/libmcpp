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

#ifndef MC_LUACLIB_DFT_VARIANT_CONVERT_H
#define MC_LUACLIB_DFT_VARIANT_CONVERT_H

/**
 * @file variant_convert.h
 * @brief variant 与 Lua 互转的性能测试接口
 */

// 前向声明 Lua 状态机
struct lua_State;

// Lua C 模块导出函数声明（C 链接）
extern "C" {

/**
 * @brief 测试 variant/variants 与 Lua 之间的双向转换性能
 *
 * 根据参数数量自动选择转换方式：
 * - 1个参数：使用 lua_to_variant / variant_to_lua，返回单个值
 * - 多个参数：使用 lua_to_variants / variants_to_lua，返回值数组
 *
 * Lua 使用示例：
 * ```lua
 * -- 单个参数（返回单个值）
 * local lua_to_variant_us, variant_to_lua_us, converted_value =
 *     convert_between_variant_lua({a = 1, b = 2})
 * print("总耗时:", lua_to_variant_us + variant_to_lua_us, "μs")
 * assert(converted_value.a == 1)
 *
 * -- 多个参数（返回数组）
 * local lua_to_variants_us, variants_to_lua_us, converted_values =
 *     convert_between_variant_lua(1, "hello", {x = 10})
 * print("总耗时:", lua_to_variants_us + variants_to_lua_us, "μs")
 * assert(converted_values[1] == 1 and converted_values[2] == "hello")
 *
 * -- 循环测试收集耗时数据
 * local times = {}
 * for i = 1, 1000 do
 *     local t1, t2, _ = convert_between_variant_lua({a = 1, b = 2})
 *     table.insert(times, t1 + t2)
 * end
 * -- 计算平均值、中位数等统计信息
 * ```
 *
 * @param L Lua 状态机
 * @return 返回值数量（3个）
 *   - lua 转换耗时（微秒）
 *   - 回转换耗时（微秒）
 *   - 转换后的值（单个值或数组，取决于参数数量）
 */
int convert_between_variant_lua(::lua_State* L);

} // extern "C"

#endif // MC_LUACLIB_DFT_VARIANT_CONVERT_H
