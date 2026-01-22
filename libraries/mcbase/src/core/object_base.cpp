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

#include <mc/core/object_base.h>

namespace mc::core {

object_base::object_base() {
}

object_base::~object_base() {
}

object_base::object_base(const object_base& other)
    : enable_shared_from_this<object_base>(other), m_object_id(other.m_object_id) {
}

object_base::object_base(object_base&& other)
    : enable_shared_from_this<object_base>(std::forward<object_base>(other)), m_object_id(other.m_object_id) {
    other.m_object_id = 0;
}

object_base& object_base::operator=(object_base&& other) {
    if (this != &other) {
        enable_shared_from_this<object_base>::operator=(std::forward<object_base>(other));
        m_object_id       = other.m_object_id;
        other.m_object_id = 0;
    }
    return *this;
}

object_base& object_base::operator=(const object_base& other) {
    if (this != &other) {
        enable_shared_from_this<object_base>::operator=(other);
        m_object_id = other.m_object_id;
    }
    return *this;
}

object_id_type object_base::get_object_id() const {
    return m_object_id;
}

void object_base::set_object_id(object_id_type id) {
    m_object_id = id;
}

bool object_base::has_valid_id() const {
    return m_object_id != 0;
}

} // namespace mc::core
