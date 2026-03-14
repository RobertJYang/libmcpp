/*
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

#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/fmt/format_arg.h>
#include <mc/fmt/format_spec.h>

namespace mc::fmt::detail {

void throw_format_error(std::string_view fmt_str, const mc::dict& args)
{
    throw mc::format_error(mc::log::message(mc::log::level::error, std::string(fmt_str),
                                            mc::log::context(__FILE__, __FUNCTION__, __LINE__), args.as_mut()));
}

} // namespace mc::fmt::detail