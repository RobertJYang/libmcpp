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

#ifndef MC_APP_SERVICE_REGISTRY_H
#define MC_APP_SERVICE_REGISTRY_H

#include <mc/app/service.h>
#include <mc/array.h>
#include <mc/reflect.h>
#include <mc/small_function.h>
#include <mc/string.h>
#include <mc/string_view.h>

#include <type_traits>
#include <string>
#include <unordered_map>
#include <utility>

namespace mc::app {

class service_registry;

using service_creator            = mc::small_function<service_ptr(mc::string), 64>;
using service_property_validator = mc::small_function<bool(const mc::dict&), 64>;

struct service_config_descriptor {
    mc::string version{"v1"};
    mc::dict   default_properties;
    mc::dict   schema;
};

struct service_descriptor {
    mc::string                type_name;
    service_execution_model   execution_model{service_execution_model::inherit};
    bool                      enabled_by_default{false};
    service_config_descriptor config;
};

namespace detail {

template <typename, typename = void>
struct has_config_type : std::false_type {};

template <typename T>
struct has_config_type<T, std::void_t<typename T::config_type>> : std::true_type {};

template <typename T>
inline constexpr bool has_config_type_v = has_config_type<T>::value;

template <typename ServiceType>
service_ptr create_service_instance(mc::string name)
{
    static_assert(std::is_base_of_v<service, ServiceType>, "ServiceType must derive from mc::app::service");
    static_assert(std::is_constructible_v<ServiceType, mc::string> || std::is_constructible_v<ServiceType, std::string>,
                  "ServiceType must be constructible from mc::string or std::string");
    if constexpr (std::is_constructible_v<ServiceType, mc::string>) {
        return mc::make_shared<ServiceType>(std::move(name));
    } else {
        return mc::make_shared<ServiceType>(std::string(name));
    }
}

template <typename ServiceType>
bool validate_service_properties(const mc::dict& properties)
{
    if constexpr (has_config_type_v<ServiceType>) {
        using config_type = typename ServiceType::config_type;
        config_type config{};
        for (const auto& entry : properties) {
            if (!entry.key.is_string()) {
                return false;
            }

            auto* property = mc::reflect::get_property_info<config_type>(entry.key.get_string());
            if (property == nullptr) {
                return false;
            }

            auto signature = property->get_signature();
            auto compatible = [&entry, signature]() {
                if (signature.empty() || signature[0] == 'v') {
                    return true;
                }
                switch (signature[0]) {
                case 's':
                case 'o':
                case 'g':
                    return entry.value.is_string();
                case 'b':
                    return entry.value.is_bool();
                case 'y':
                case 'n':
                case 'q':
                case 'i':
                case 'u':
                case 'x':
                case 't':
                    return entry.value.is_integer();
                case 'd':
                    return entry.value.is_double() || entry.value.is_integer();
                case 'a':
                    return signature.size() > 1 && signature[1] == '{' ? entry.value.is_dict() : entry.value.is_array();
                case '(':
                case 'r':
                    return entry.value.is_dict() || entry.value.is_array();
                default:
                    return true;
                }
            }();

            if (!compatible || !mc::reflect::set_property(config, entry.key.get_string(), entry.value)) {
                return false;
            }
        }
        return true;
    } else {
        MC_UNUSED(properties);
        return true;
    }
}

template <typename ServiceType>
service_config_descriptor make_service_config_descriptor()
{
    service_config_descriptor descriptor;
    if constexpr (has_config_type_v<ServiceType>) {
        using config_type = typename ServiceType::config_type;

        mc::variant defaults;
        mc::to_variant(config_type{}, defaults);
        if (defaults.is_dict()) {
            descriptor.default_properties = defaults.as<mc::dict>();
        }

        mc::reflect::reflector<config_type>::visit_properties([&descriptor](const mc::reflect::property_type_info* property) {
            mc::dict schema_item;
            schema_item["signature"] = mc::string(property->get_signature());
            descriptor.schema[property->name] = std::move(schema_item);
            return mc::reflect::visit_status::VS_CONTINUE;
        });
    }
    return descriptor;
}

template <typename ServiceType>
service_creator make_service_creator()
{
    return service_creator([](mc::string service_name) {
        return create_service_instance<ServiceType>(std::move(service_name));
    });
}

template <typename ServiceType>
service_property_validator make_service_property_validator()
{
    return service_property_validator([](const mc::dict& properties) {
        return validate_service_properties<ServiceType>(properties);
    });
}

template <typename ServiceType>
void register_service_into(service_registry& registry, mc::string_view type_name,
                           service_execution_model execution_model, bool enabled_by_default);

} // namespace detail

class MC_API service_registry {
public:
    void register_descriptor(service_descriptor descriptor, service_creator create,
                             service_property_validator validate_properties = {});

    template <typename ServiceType>
    void register_service(mc::string type_name, service_execution_model execution_model = service_execution_model::inherit,
                          bool enabled_by_default = false)
    {
        register_descriptor(service_descriptor{type_name, execution_model, enabled_by_default,
                                              detail::make_service_config_descriptor<ServiceType>()},
                            detail::make_service_creator<ServiceType>(),
                            detail::make_service_property_validator<ServiceType>());
    }

    bool                      has_service(mc::string_view type_name) const;
    const service_descriptor* find(mc::string_view type_name) const;
    service_ptr               create_service(mc::string_view type_name, mc::string service_name);
    bool                      validate_service_properties(mc::string_view type_name, const mc::dict& properties);
    mc::array<mc::string>     get_service_types() const;
    void                      import_global_services();

private:
    struct service_registration {
        service_descriptor         descriptor;
        service_creator            create;
        service_property_validator validate_properties;
    };

    std::unordered_map<std::string, service_registration> m_descriptors;
};

namespace detail {

using global_service_registrar =
    void (*)(service_registry& registry, mc::string_view type_name, service_execution_model execution_model,
             bool enabled_by_default);

MC_API void register_global_service_descriptor(service_descriptor descriptor, global_service_registrar registrar);
MC_API void import_global_service_descriptors(service_registry& registry);

} // namespace detail

template <typename ServiceType>
void register_global_service(mc::string type_name,
                             service_execution_model execution_model = service_execution_model::inherit,
                             bool enabled_by_default                = false)
{
    detail::register_global_service_descriptor(service_descriptor{
                                                   type_name,
                                                   execution_model,
                                                   enabled_by_default,
                                                   detail::make_service_config_descriptor<ServiceType>(),
                                               },
                                               &detail::register_service_into<ServiceType>);
}

namespace detail {

template <typename ServiceType>
void register_service_into(service_registry& registry, mc::string_view type_name,
                           service_execution_model execution_model, bool enabled_by_default)
{
    registry.register_service<ServiceType>(mc::string(type_name), execution_model, enabled_by_default);
}

} // namespace detail

#define MC_APP_DETAIL_CONCAT_INNER(a, b) a##b
#define MC_APP_DETAIL_CONCAT(a, b) MC_APP_DETAIL_CONCAT_INNER(a, b)
#define MC_APP_REGISTER_GLOBAL_SERVICE(ServiceType, ServiceName)                                              \
    namespace {                                                                                               \
    [[maybe_unused]] const bool MC_APP_DETAIL_CONCAT(mc_app_registered_service_, __COUNTER__) = []() {      \
        ::mc::app::register_global_service<ServiceType>(ServiceName);                                         \
        return true;                                                                                          \
    }();                                                                                                      \
    }

} // namespace mc::app

#endif // MC_APP_SERVICE_REGISTRY_H
