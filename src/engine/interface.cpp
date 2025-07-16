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

interface_impl::~interface_impl() = default;

abstract_object* interface_impl::get_owner() const {
    return m_owner;
}

void interface_impl::set_owner(abstract_object* owner) {
    m_owner = owner;
}

property_changed_signal& interface_impl::property_changed() {
    if (m_property_changed_signal == nullptr) {
        m_property_changed_signal = std::make_unique<property_changed_signal>();
    }

    return *m_property_changed_signal;
}

void interface_impl::notify_property_changed(const mc::variant& value, const property_base& prop) {
    if (m_property_changed_signal) {
        (*m_property_changed_signal)(value, prop);
    }
}

} // namespace mc::engine
