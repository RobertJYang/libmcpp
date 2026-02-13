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

#include <mc/dbus/bus_mode_impl.h>
#include <mc/dbus/message.h>
#include <mc/dbus/sd_bus.h>
#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/log.h>
#include <mc/runtime.h>

namespace mc::dbus {
constexpr mc::milliseconds DEFAULT_DBUS_CALL_TIMEOUT = mc::minutes(10);
constexpr std::string_view DEVMON_SERVICE            = "bmc.kepler.devmon";
constexpr std::string_view DEVMON_CHIP_INTERFACE     = "bmc.dev.Chip";
constexpr int64_t          CALL_TIMEOUT_SECONDS      = 30;

using method_mapping_t = std::map<std::string_view, std::pair<std::string_view, std::string_view>>;

const std::map<std::string_view, method_mapping_t> DEVMON_CHIP_METHODS = {
    {"bmc.kepler.Chip.BitIO",
     {
         {"Read", {"BitIORead", "uyu"}},
         {"Write", {"BitIOWrite", "uyuay"}},
     }},
    {"bmc.kepler.Chip.BlockIO",
     {
         {"Read", {"BlockIORead", "uu"}},
         {"Write", {"BlockIOWrite", "uay"}},
         {"WriteRead", {"BlockIOWriteRead", "ayu"}},
         {"ComboWriteRead", {"BlockIOComboWriteRead", "uayuu"}},
     }},
};

// 创建对应模式的实现
static std::unique_ptr<bus_mode_impl> create_bus_impl(bool is_blocking) {
    if (is_blocking) {
        return std::make_unique<blocking_bus_impl>();
    }
    return std::make_unique<nonblocking_bus_impl>();
}

sd_bus::sd_bus(bool start_now, bool is_blocking)
    : m_is_blocking(is_blocking), m_bus(create_bus_impl(is_blocking)) {
    // 创建连接并初始化到 bus_impl
    auto conn = connection::open_session_bus(mc::get_io_context());
    m_bus->init_connection(std::move(conn));
    m_unique_name = std::string(m_bus->get_unique_name());

    if (start_now) {
        m_bus->get_connection().start();
    }
}

sd_bus::sd_bus(connection conn, bool is_blocking)
    : m_is_blocking(is_blocking), m_bus(create_bus_impl(is_blocking)) {
    // 使用提供的连接初始化到 bus_impl
    m_bus->init_connection(std::move(conn));
    m_unique_name = std::string(m_bus->get_unique_name());
}

sd_bus::~sd_bus() = default;

sd_bus::sd_bus(sd_bus&& other) noexcept
    : m_is_blocking(other.m_is_blocking),
      m_enable_local_request(other.m_enable_local_request),
      m_bus(std::move(other.m_bus)),
      m_unique_name(std::move(other.m_unique_name)),
      m_service_name(std::move(other.m_service_name)) {
}

sd_bus& sd_bus::operator=(sd_bus&& other) noexcept {
    if (this != &other) {
        m_is_blocking          = other.m_is_blocking;
        m_enable_local_request = other.m_enable_local_request;
        m_bus                  = std::move(other.m_bus);
        m_unique_name          = std::move(other.m_unique_name);
        m_service_name         = std::move(other.m_service_name);
    }
    return *this;
}

variants sd_bus::dbus_call(mc::milliseconds timeout, const method_call_params& params) {
    return m_bus->timeout_call(timeout.count(), std::string(params.service_name), std::string(params.path),
                               std::string(params.interface), std::string(params.method),
                               std::string(params.signature), variants(params.args));
}

std::optional<variants> sd_bus::shm_timeout_call(mc::milliseconds timeout, const method_call_params& params) {
    auto result = shm_tree::timeout_call_with_sender(timeout, m_unique_name, params);
    if (result != std::nullopt) {
        return result.value();
    }
    return std::nullopt;
}

variants sd_bus::timeout_call_impl(mc::milliseconds timeout, const method_call_params& params) {
    if (m_is_blocking || m_service_name.empty()) {
        return dbus_call(timeout, params);
    }
    auto result = shm_timeout_call(timeout, params);
    if (result != std::nullopt) {
        return result.value();
    }
    return dbus_call(timeout, params);
}

void sd_bus::set_enable_local_request(bool enable) {
    m_enable_local_request = enable;
}

static variants remove_context_arg(const variants& args) {
    if (args.empty()) {
        return args;
    }
    bool is_context = args[0].is_dict() || (args[0].is_array() && args[0].as_array().empty());
    if (!is_context) {
        return args;
    }
    variants args_without_context = args;
    args_without_context.erase(args.begin());
    return args_without_context;
}

std::optional<variants> sd_bus::reroute_call(mc::milliseconds timeout, const method_call_params& params) {
    if (params.service_name != DEVMON_SERVICE) {
        return std::nullopt;
    }
    auto interface_it = DEVMON_CHIP_METHODS.find(params.interface);
    if (interface_it == DEVMON_CHIP_METHODS.end()) {
        return std::nullopt;
    }

    auto method_it = interface_it->second.find(params.method);
    if (method_it == interface_it->second.end()) {
        return std::nullopt;
    }

    const auto& [target_method, target_signature] = method_it->second;
    method_call_params target_params{DEVMON_SERVICE, params.path, DEVMON_CHIP_INTERFACE,
                                     target_method, target_signature, remove_context_arg(params.args)};
    return timeout_call_impl(timeout, target_params);
}

void sd_bus::request_name(std::string_view service_name, uint32_t flags) {
    m_service_name = std::string(service_name);
    // 委托给 m_bus 处理请求名称和模式特定的逻辑
    m_bus->request_name(m_service_name, flags);
}

static variants add_requestor_to_context(const variants& args, std::string_view service_name) {
    if (service_name.empty() || args.empty() || !args[0].is_dict()) {
        return args;
    }
    auto context = args[0].as_dict();
    if (context.find("Requestor") != context.end()) {
        return args;
    }
    context["Requestor"]   = service_name;
    variants modified_args = args;
    modified_args[0]       = std::move(context);
    return modified_args;
}

static std::string get_caller_from_context(const variants& args) {
    if (args.empty() || !args[0].is_dict()) {
        return "unknown";
    }
    auto context = args[0].as_dict();
    if (context.contains("Requestor") && context["Requestor"].is_string()) {
        return context["Requestor"].as_string();
    }
    if (context.contains("Interface") && context["Interface"].is_string()) {
        return context["Interface"].as_string();
    }
    return "unknown";
}

variants sd_bus::timeout_call(mc::milliseconds timeout, const method_call_params& params) {
    method_call_params modified_params{
        .service_name = params.service_name,
        .path         = params.path,
        .interface    = params.interface,
        .method       = params.method,
        .signature    = params.signature,
        .args         = add_requestor_to_context(params.args, m_service_name),
    };
    auto                    start  = std::chrono::steady_clock::now();
    std::optional<variants> result = reroute_call(timeout, modified_params);
    if (!result.has_value()) {
        result = timeout_call_impl(timeout, modified_params);
    }
    auto        time_cost = std::chrono::steady_clock::now() - start;
    auto        duration  = std::chrono::duration_cast<std::chrono::seconds>(time_cost).count();
    std::string caller    = get_caller_from_context(modified_params.args);
    if (duration >= CALL_TIMEOUT_SECONDS) {
        wlog("service[${caller}] request timeout: remote service[${service_name}], path[${path}],"
             " interface[${interface}], method[${method}], used time[${duration}s]",
             ("caller", caller)("service_name", modified_params.service_name)("path", modified_params.path)(
                 "interface", modified_params.interface)("method", modified_params.method)("duration", duration));
    } else {
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_cost).count();
        dlog("service[${caller}] request successful: remote service[${service_name}], path[${path}],"
             " interface[${interface}], method[${method}], used time[${duration_ms}ms]",
             ("caller", caller)("service_name", modified_params.service_name)("path", modified_params.path)(
                 "interface", modified_params.interface)("method", modified_params.method)("duration_ms", duration_ms));
    }
    return result.value();
}

variants sd_bus::call(const method_call_params& params) {
    return timeout_call(DEFAULT_DBUS_CALL_TIMEOUT, params);
}

sd_bus::pcall_result sd_bus::pcall(const method_call_params& params) {
    return timeout_pcall(DEFAULT_DBUS_CALL_TIMEOUT, params);
}

sd_bus::pcall_result sd_bus::timeout_pcall(mc::milliseconds timeout, const method_call_params& params) {
    try {
        auto result = timeout_call(timeout, params);
        return {std::nullopt, std::move(result)};
    } catch (const mc::exception& e) {
        return {e.what(), variants{}};
    }
}

connection& sd_bus::get_connection() {
    return m_bus->get_connection();
}

uint64_t sd_bus::add_match(match_rule& rule, match_cb_t&& cb) {
    return m_bus->add_match(rule, std::forward<match_cb_t>(cb));
}

void sd_bus::remove_match(uint64_t id) {
    m_bus->remove_match(id);
}

void sd_bus::register_object(mc::shared_ptr<dynamic_object> object) {
    std::string path(object->get_object_path());
    if (m_objects.find(path) != m_objects.end()) {
        return;
    }
    m_bus->get_connection().register_path(path, [](auto& msg) {
        MC_UNUSED(msg);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    });
    m_objects.emplace(path, object);
#if defined(ENABLE_CONAN_COMPILE)
    auto* shm_tree = m_bus->get_shm_tree();
    if (!shm_tree) {
        return;
    }
    shm::object_tree* tree = shm_tree->get_tree();
    if (!tree) {
        return;
    }
    auto&        ins     = shm::shared_memory::get_instance();
    shm::object& shm_obj = tree->register_object(ins, path);
    for (auto& [name, interface] : object->get_interfaces()) {
        shm::interface& shm_intf = shm_obj.register_interface(ins, false, name);
        for (auto& [prop_name, prop] : interface->get_properties()) {
            shm::shared_ptr<shm::property> shm_prop = shm_intf.add_p(ins, prop_name, prop.signature);
            shm_prop->set_flags(prop.flags);
            shm_tree::set_property_inner(shm_prop, prop.value);
            prop.shm_prop = shm_prop;
        }
    }
#endif
}

void sd_bus::unregister_object(std::string_view path) {
    m_bus->get_connection().unregister_path(path);
    m_objects.erase(std::string(path));
}

} // namespace mc::dbus
