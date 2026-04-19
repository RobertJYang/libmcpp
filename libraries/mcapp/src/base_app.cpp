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

#include <mc/app/base_app.h>

#include <mc/exception.h>

namespace mc::app {
namespace {

base_app* g_current_app = nullptr;

} // namespace

base_app* base_app::get() noexcept
{
    return g_current_app;
}

base_app& base_app::instance()
{
    MC_ASSERT(g_current_app != nullptr, "mcapp 当前实例不存在");
    return *g_current_app;
}

void base_app::reset_for_test() noexcept
{
    g_current_app = nullptr;
}

base_app::base_app()
{
    if (g_current_app != nullptr) {
        MC_THROW(mc::invalid_op_exception, "mcapp 只允许一个当前实例");
    }
    g_current_app = this;
}

base_app::~base_app()
{
    if (g_current_app == this) {
        g_current_app = nullptr;
    }
}

} // namespace mc::app
