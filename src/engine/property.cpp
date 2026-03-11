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

#include <mc/engine/property.h>

namespace mc::engine {

void detail::interface_observer::notify(const mc::variant& value, const property_base& prop)
{
    if (!m_interface) {
        return;
    }

    m_interface->notify_property_changed(value, prop);
    auto object = m_interface->get_owner();
    if (!object) {
        return;
    }

    object->notify_property_changed(value, prop);
}

void detail::interface_observer::notify_update_shm(const mc::variant& value, const property_base& prop)
{
    if (!m_interface) {
        return;
    }
    auto object = m_interface->get_owner();
    if (object) {
        object->notify_property_update_shm(value, prop);
    }
}

} // namespace mc::engine

namespace mc {
void from_variant(const mc::variant& var, mc::engine::ref_object*& ptr)
{
    if (var.is_extension()) {
        auto ext_ptr = var.as_extension();
        if (ext_ptr) {
            ptr = dynamic_cast<mc::engine::ref_object*>(ext_ptr.get());
            if (ptr) {
                return;
            }
        }
    }
    ptr = nullptr;
}
} // namespace mc