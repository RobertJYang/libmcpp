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
 * @file typed_variant.cpp
 * @brief 实现 typed_variant 相关的转换函数
 */

#include "../include/mc/variant.h"
#include "../include/mc/dict.h"
#include <cstdint>
#include <stdexcept>

namespace mc {

// typed_variant 实现

typed_variant::typed_variant() : variant() {
}

typed_variant::typed_variant(const variant& other) : variant(other) {
}

typed_variant::typed_variant(variant&& other) noexcept : variant(std::move(other)) {
}

typed_variant::typed_variant(const typed_variant& other) : variant(other) {
}

typed_variant::typed_variant(typed_variant&& other) noexcept : variant(std::move(other)) {
}

typed_variant::typed_variant(type_id type) : variant(type) {
}

typed_variant& typed_variant::operator=(const variant& other) {
    if (this == &other) {
        return *this;
    }

    if (m_type == other.get_type() || m_type == type_id::null_type) {
        variant::operator=(other);
    } else {
        switch (m_type) {
        case type_id::int8_type: {
            m_int64 = other.as_int8();
            break;
        }
        case type_id::uint8_type: {
            m_uint64 = other.as_uint8();
            break;
        }
        case type_id::int16_type: {
            m_int64 = other.as_int16();
            break;
        }
        case type_id::uint16_type: {
            m_uint64 = other.as_uint16();
            break;
        }
        case type_id::int32_type: {
            m_int64 = other.as_int32();
            break;
        }
        case type_id::uint32_type: {
            m_uint64 = other.as_uint32();
            break;
        }
        case type_id::int64_type: {
            m_int64 = other.as_int64();
            break;
        }
        case type_id::uint64_type: {
            m_uint64 = other.as_uint64();
            break;
        }
        case type_id::double_type: {
            m_double = other.as_double();
            break;
        }
        case type_id::bool_type: {
            m_bool = other.as_bool();
            break;
        }
        case type_id::string_type: {
            from_variant(other, *static_cast<std::string*>(m_string_ptr));
            break;
        }
        case type_id::array_type:
            from_variant(other, *static_cast<variants*>(m_array_ptr));
            break;
        case type_id::object_type:
            from_variant(other, *static_cast<dict*>(m_object_ptr));
            break;
        case type_id::blob_type:
            from_variant(other, *static_cast<blob*>(m_blob_ptr));
            break;
        default:
            throw std::runtime_error("未知类型：" + std::string(get_type_name(m_type)));
            break;
        }
    }
    return *this;
}

typed_variant& typed_variant::operator=(variant&& other) {
    if (this != &other) {
        if (get_type() == other.get_type()) {
            variant::operator=(std::move(other));
        } else {
            // 类型不一样那就一定存在转换，相比move会有一定开销
            *this = other;
            other.clear();
        }
    }
    return *this;
}

typed_variant& typed_variant::operator=(const typed_variant& other) {
    if (this != &other) {
        typed_variant::operator=(static_cast<const variant&>(other));
    }
    return *this;
}


typed_variant& typed_variant::operator=(typed_variant&& other) {
    if (this != &other) {
        typed_variant::operator=(std::forward<variant&&>(other));
    }
    return *this;
}

/**
 * @brief 将 typed_variant 转换为 variant
 * @param var 源 typed_variant
 * @param vo 目标 variant
 */
void to_variant(const typed_variant& var, variant& vo) {
    vo = static_cast<const variant&>(var);
}

/**
 * @brief 将 variant 转换为 typed_variant
 * @param var 源 variant
 * @param vo 目标 typed_variant
 */
void from_variant(const variant& var, typed_variant& vo) {
    vo = var;
}

} // namespace mc 