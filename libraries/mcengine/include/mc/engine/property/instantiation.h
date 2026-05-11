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

#ifndef MC_ENGINE_PROPERTY_INSTANTIATION_H
#define MC_ENGINE_PROPERTY_INSTANTIATION_H

/**
 * @file instantiation.h
 * @brief property 模板显式实例化声明
 *
 * 通过显式实例化，将常用的 property<T> 类型在 libmcpp 库中只编译一次，
 * 其他使用 property.h 的库不需要重复展开模板，从而减少编译时间和内存占用。
 *
 * 使用说明：
 * - 此文件由 property.h 自动包含，必须在 property 模板类定义之后
 * - 如果需要添加新的显式实例化类型，请同时修改此文件和 instantiation.cpp
 * - 显式实例化的类型会在 libmcpp 库中编译一次，其他库链接时直接使用
 */

namespace mc::engine {

// ============================================================================
// 显式实例化声明 - 使用默认 Observer (detail::interface_observer)
// 使用 MC_API 导出符号，确保其他库可以链接到这些实例化
// ============================================================================

// 布尔类型
extern template class MC_API property<bool, detail::interface_observer>;

// 整数类型
extern template class MC_API property<int8_t, detail::interface_observer>;
extern template class MC_API property<uint8_t, detail::interface_observer>;
extern template class MC_API property<int16_t, detail::interface_observer>;
extern template class MC_API property<uint16_t, detail::interface_observer>;
extern template class MC_API property<int32_t, detail::interface_observer>;
extern template class MC_API property<uint32_t, detail::interface_observer>;
extern template class MC_API property<int64_t, detail::interface_observer>;
extern template class MC_API property<uint64_t, detail::interface_observer>;

// 浮点类型
extern template class MC_API property<float, detail::interface_observer>;
extern template class MC_API property<double, detail::interface_observer>;

// 字符串类型
extern template class MC_API property<std::string, detail::interface_observer>;

// variant/dict 类型
// 注意：mutable_dict 是 dict 的别名，不需要单独实例化
extern template class MC_API property<mc::variant, detail::interface_observer>;
extern template class MC_API property<mc::dict, detail::interface_observer>;

} // namespace mc::engine

#endif // MC_ENGINE_PROPERTY_INSTANTIATION_H
