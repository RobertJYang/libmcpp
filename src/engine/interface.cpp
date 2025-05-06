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

#include <mc/engine/interface.h>

namespace mc::engine {

interface_impl::interface_impl() {
}

interface_impl::~interface_impl() {
}

void interface_impl::set_object(abstract_object* obj) {
    m_object = obj;
}

abstract_object* interface_impl::get_object() const {
    return m_object;
}

timer interface_impl::new_timer(mc::milliseconds                interval,
                                std::function<void(error_code)> callback) {
    MC_ASSERT(m_object, "current interface is not bound to object, cannot create timer");

    return m_object->new_timer(interval, callback);
}

} // namespace mc::engine