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

#include <mc/reflect/reflection.h>

namespace mc::reflect::detail {

std::mutex& reflection_singleton_creation_mutex() noexcept
{
    static std::mutex mutex;
    return mutex;
}

reflection_metadata_ptr& reflection_singleton_instance_with_creator(reflection_singleton_storage& storage,
                                                                    reflection_metadata_ptr* (*creator)())
{
    auto* ptr = storage.load(std::memory_order_acquire);
    if (MC_LIKELY(ptr != nullptr)) {
        return *ptr;
    }

    std::lock_guard<std::mutex> lock(reflection_singleton_creation_mutex());
    ptr = storage.load(std::memory_order_relaxed);
    if (ptr == nullptr) {
        ptr = creator();
        storage.store(ptr, std::memory_order_release);
    }
    return *ptr;
}

reflection_metadata_ptr* reflection_singleton_try_get(reflection_singleton_storage& storage) noexcept
{
    return storage.load(std::memory_order_relaxed);
}

void reflection_singleton_reset(reflection_singleton_storage& storage) noexcept
{
    auto* ptr = storage.load(std::memory_order_relaxed);
    if (ptr == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(reflection_singleton_creation_mutex());
    ptr = storage.load(std::memory_order_relaxed);
    if (ptr != nullptr) {
        storage.store(nullptr, std::memory_order_release);
        delete ptr;
    }
}

registered_reflection_bridge::registered_reflection_bridge(unregister_reflection_t unregister_type) noexcept
    : m_unregister_type(unregister_type)
{}

registered_reflection_bridge::~registered_reflection_bridge()
{
    if (m_unregister_type != nullptr) {
        m_unregister_type();
    }
}

struct_reflection_bridge::struct_reflection_bridge(mc::string_view type_name, type_id_type type_id,
                                                   const struct_metadata& metadata, const struct_reflection_ops& ops,
                                                   unregister_reflection_t unregister_type) noexcept
    : registered_reflection_bridge(unregister_type), m_type_name(type_name), m_type_id(type_id), m_metadata(&metadata),
      m_ops(&ops)
{}

reflected_object_ptr struct_reflection_bridge::create_object()
{
    return m_ops->create_object();
}

mc::string_view struct_reflection_bridge::get_type_name() const
{
    return m_type_name;
}

type_id_type struct_reflection_bridge::get_type_id() const
{
    return m_type_id;
}

std::vector<type_id_type> struct_reflection_bridge::get_base_type_ids() const
{
    return m_metadata->get_base_type_ids();
}

bool struct_reflection_bridge::is_derived_from(type_id_type base_type_id) const
{
    return m_metadata->is_derived_from(base_type_id);
}

const property_type_info* struct_reflection_bridge::get_property_info(mc::string_view name) const
{
    return m_metadata->get_property_info(name);
}

const method_type_info* struct_reflection_bridge::get_method_info(mc::string_view name) const
{
    return m_metadata->get_method_info(name);
}

const base_class_type_info* struct_reflection_bridge::get_base_class_info(mc::string_view name) const
{
    return m_metadata->get_base_class_info(name);
}

const member_info_base* struct_reflection_bridge::get_custom_info(mc::string_view name, size_t reflect_type) const
{
    return m_metadata->get_custom_info(name, reflect_type);
}

const property_type_info* struct_reflection_bridge::get_property_info(mc::quark name) const
{
    return m_metadata->get_property_info(name);
}

const method_type_info* struct_reflection_bridge::get_method_info(mc::quark name) const
{
    return m_metadata->get_method_info(name);
}

const base_class_type_info* struct_reflection_bridge::get_base_class_info(mc::quark name) const
{
    return m_metadata->get_base_class_info(name);
}

const member_info_base* struct_reflection_bridge::get_custom_info(mc::quark name, size_t reflect_type) const
{
    return m_metadata->get_custom_info(name, reflect_type);
}

std::vector<mc::string_view> struct_reflection_bridge::get_property_names() const
{
    return m_metadata->get_property_names();
}

std::vector<mc::string_view> struct_reflection_bridge::get_method_names() const
{
    return m_metadata->get_method_names();
}

const struct_metadata& struct_reflection_bridge::get_metadata_ref() const noexcept
{
    return *m_metadata;
}

} // namespace mc::reflect::detail
