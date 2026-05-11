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
 * @file variant_extension_runtime.cpp
 * @brief erased_extension_node 的非模板运行时调度实现
 *
 * 所有通过 make_composed_extension 创建的 composed extension 均使用
 * erased_extension_node + extension_ops vtable 调度，避免每个 Payload 类型
 * 在每个编译单元展开完整的 composed_extension_node 模板实例。
 */

#include <mc/variant/variant_extension.h>

namespace mc {

// ---------------------------------------------------------------------------
// erased_extension_node
// ---------------------------------------------------------------------------

erased_extension_node::erased_extension_node(void* payload_ptr, const extension_ops* ops) noexcept
    : m_payload(payload_ptr), m_ops(ops)
{}

mc::shared_ptr<variant_extension_base> erased_extension_node::copy() const
{
    return m_ops->copy_node(m_payload);
}

mc::shared_ptr<variant_extension_base> erased_extension_node::deep_copy(detail::copy_context* ctx) const
{
    return m_ops->deep_copy_node(m_payload, ctx);
}

extension_type_info erased_extension_node::get_type_info() const
{
    return m_ops->get_type_info();
}

mc::string_view erased_extension_node::get_type_name() const
{
    if (auto* pt = payload_type()) {
        return pt->name;
    }
    return variant_extension_base::get_type_name();
}

bool erased_extension_node::equals(const variant_extension_base& other) const
{
    if (this == &other) {
        return true;
    }
    return m_ops->equals(m_payload, other);
}

std::size_t erased_extension_node::hash() const
{
    return m_ops->hash_payload(m_payload);
}

const variant_payload_type* erased_extension_node::payload_type() const
{
    return m_ops->get_payload_type();
}

void* erased_extension_node::payload_address()
{
    return m_payload;
}

const void* erased_extension_node::payload_address() const
{
    return m_payload;
}

void* erased_extension_node::upcast_payload_to(const variant_payload_type& target)
{
    return m_ops->upcast(m_payload, target);
}

const void* erased_extension_node::upcast_payload_to(const variant_payload_type& target) const
{
    return m_ops->const_upcast(m_payload, target);
}

} // namespace mc
