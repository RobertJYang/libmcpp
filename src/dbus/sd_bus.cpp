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

sd_bus::sd_bus(bool start_now, bool is_blocking)
    : m_is_blocking(is_blocking), m_connection(connection::open_session_bus(mc::get_io_context())),
      m_unique_name(std::string(m_connection.get_unique_name())) {
    if (start_now) {
        m_connection.start();
    }
}

sd_bus::sd_bus(connection conn, bool is_blocking)
    : m_is_blocking(is_blocking), m_connection(std::move(conn)),
      m_unique_name(std::string(m_connection.get_unique_name())) {
}

variants sd_bus::dbus_call(mc::milliseconds timeout, const method_call_params& params) {
    auto               msg    = message::new_method_call(params.service_name, params.path, params.interface, params.method);
    auto               writer = msg.writer();
    signature_iterator it(params.signature);
    for (auto arg : params.args) {
        if (it.at_end()) {
            break;
        }
        writer.write_variant(it, arg, 0);
        it.next();
    }
    auto reply = m_connection.send_with_reply_and_block(std::move(msg), timeout);
    if (reply.is_valid() && reply.is_method_return()) {
        return reply.read_args();
    }
    std::string error_name(reply.get_error_name());
    std::string error_message = reply.get_error_message();
    throw mc::exception(mc::method_call_exception_code, error_name, error_message);
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
    auto mdb_info = shm_tree::get_mdb_info(params);
    if (mdb_info != std::nullopt) {
        return mdb_info;
    }
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
    m_connection.request_name(m_service_name, flags);
    auto& harbor = mc::dbus::harbor::get_instance();
    harbor.set_harbor_name_if_empty("harbor." + m_service_name);
    harbor.register_unique_name(m_unique_name, m_service_name);
    create_shm_tree(harbor.get_harbor_name(), m_service_name, m_unique_name);
    harbor.start();
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
    return m_connection;
}

} // namespace mc::dbus