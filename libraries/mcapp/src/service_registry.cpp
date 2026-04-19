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

#include <mc/app/service_registry.h>

#include <mutex>

namespace mc::app {
namespace {

struct global_service_registration {
    service_descriptor              descriptor;
    detail::global_service_registrar registrar{nullptr};
};

class global_service_catalog {
public:
    static global_service_catalog& instance()
    {
        static global_service_catalog catalog;
        return catalog;
    }

    void register_descriptor(service_descriptor descriptor, detail::global_service_registrar registrar)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_descriptors.insert_or_assign(std::string(descriptor.type_name),
                                       global_service_registration{std::move(descriptor), registrar});
    }

    void import_into(service_registry& registry) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& entry : m_descriptors) {
            if (entry.second.registrar != nullptr) {
                entry.second.registrar(registry, entry.second.descriptor.type_name, entry.second.descriptor.execution_model,
                                       entry.second.descriptor.enabled_by_default);
            }
        }
    }

private:
    mutable std::mutex                                         m_mutex;
    std::unordered_map<std::string, global_service_registration> m_descriptors;
};

} // namespace

void service_registry::register_descriptor(service_descriptor descriptor, service_creator create,
                                           service_property_validator validate_properties)
{
    m_descriptors.insert_or_assign(
        std::string(descriptor.type_name),
        service_registration{std::move(descriptor), std::move(create), std::move(validate_properties)});
}

bool service_registry::has_service(mc::string_view type_name) const
{
    return m_descriptors.find(std::string(type_name)) != m_descriptors.end();
}

const service_descriptor* service_registry::find(mc::string_view type_name) const
{
    auto it = m_descriptors.find(std::string(type_name));
    if (it == m_descriptors.end()) {
        return nullptr;
    }

    return &it->second.descriptor;
}

service_ptr service_registry::create_service(mc::string_view type_name, mc::string service_name)
{
    auto it = m_descriptors.find(std::string(type_name));
    if (it == m_descriptors.end() || !it->second.create) {
        return nullptr;
    }

    return it->second.create(std::move(service_name));
}

bool service_registry::validate_service_properties(mc::string_view type_name, const mc::dict& properties)
{
    auto it = m_descriptors.find(std::string(type_name));
    if (it == m_descriptors.end() || !it->second.validate_properties) {
        return true;
    }
    return it->second.validate_properties(properties);
}

mc::array<mc::string> service_registry::get_service_types() const
{
    mc::array<mc::string> types;
    for (const auto& entry : m_descriptors) {
        types.push_back(entry.second.descriptor.type_name);
    }
    return types;
}

void service_registry::import_global_services()
{
    detail::import_global_service_descriptors(*this);
}

void detail::register_global_service_descriptor(service_descriptor descriptor, global_service_registrar registrar)
{
    global_service_catalog::instance().register_descriptor(std::move(descriptor), registrar);
}

void detail::import_global_service_descriptors(service_registry& registry)
{
    global_service_catalog::instance().import_into(registry);
}

} // namespace mc::app
