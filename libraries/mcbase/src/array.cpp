/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
 * @file array.cpp
 * @brief 实现 array.h 中声明的辅助方法
 */
#include <mc/array.h>
#include <mc/exception.h>

namespace mc {
namespace detail {

/**
 * @brief 抛出索引越界异常
 *
 * @param msg 异常消息
 */
[[noreturn]] void throw_array_out_of_range(mc::string_view msg)
{
    MC_THROW(mc::out_of_range_exception, "${msg}", ("msg", msg));
}

} // namespace detail
} // namespace mc
