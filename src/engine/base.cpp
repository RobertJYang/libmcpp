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
#include <mc/engine/base.h>

namespace mc::engine {

object_wrap::object_wrap(mc::engine::object_base* object) : m_object(object) {
    m_object->ref();
}

object_wrap::~object_wrap() {
    free();
}

void object_wrap::free() {
    if (m_object) {
        m_object->unref();
        m_object = nullptr;
    }
}

object_wrap::object_wrap(const object_wrap& other)
    : mc::db::object<object_wrap>(), m_object(other.m_object) {
    if (m_object) {
        m_object->ref();
    }
}

object_wrap& object_wrap::operator=(const object_wrap& other) {
    if (this != &other && m_object != other.m_object) {
        free();
        m_object = other.m_object;
        if (m_object) {
            m_object->ref();
        }
    }

    return *this;
}

object_wrap::object_wrap(object_wrap&& other) noexcept {
    m_object       = other.m_object;
    other.m_object = nullptr;
}

object_wrap& object_wrap::operator=(object_wrap&& other) noexcept {
    if (this != &other) {
        free();

        m_object       = other.m_object;
        other.m_object = nullptr;
    }

    return *this;
}

const std::string& object_wrap::get_path() const {
    return m_object->get_object_path();
}

} // namespace mc::engine