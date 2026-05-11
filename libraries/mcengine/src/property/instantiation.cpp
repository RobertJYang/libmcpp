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
 * @file instantiation.cpp
 * @brief property 模板显式实例化定义
 *
 * 此文件包含常用 property<T> 类型的显式实例化定义，
 * 确保这些模板类在 libmcpp 库中只编译一次。
 */

#include <mc/engine/property.h>

namespace mc::engine {

// ============================================================================
// 显式实例化定义 - 使用默认 Observer (detail::interface_observer)
// 注意：MC_API 只需要在头文件的 extern template 声明中，定义处不需要
// ============================================================================

// 布尔类型
template class property<bool, detail::interface_observer>;

// 整数类型
template class property<int8_t, detail::interface_observer>;
template class property<uint8_t, detail::interface_observer>;
template class property<int16_t, detail::interface_observer>;
template class property<uint16_t, detail::interface_observer>;
template class property<int32_t, detail::interface_observer>;
template class property<uint32_t, detail::interface_observer>;
template class property<int64_t, detail::interface_observer>;
template class property<uint64_t, detail::interface_observer>;

// 浮点类型
template class property<float, detail::interface_observer>;
template class property<double, detail::interface_observer>;

// 字符串类型
template class property<std::string, detail::interface_observer>;

// variant/dict 类型
// 注意：mutable_dict 是 dict 的别名，不需要单独实例化
template class property<mc::variant, detail::interface_observer>;
template class property<mc::dict, detail::interface_observer>;

} // namespace mc::engine
