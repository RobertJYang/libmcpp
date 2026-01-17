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

#include <mc/dbus/sd_bus.h>
#include <mc/dbus/message.h>
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
    {"bmc.kepler.Chip.BitIO", {
                                  {"Read", {"BitIORead", "uyu"}},
                                  {"Write", {"BitIOWrite", "uyuay"}},
                              }},
    {"bmc.kepler.Chip.BlockIO", {
                                    {"Read", {"BlockIORead", "uu"}},
                                    {"Write", {"BlockIOWrite", "uay"}},
                                    {"WriteRead", {"BlockIOWriteRead", "ayu"}},
                                    {"ComboWriteRead", {"BlockIOComboWriteRead", "uayuu"}},
                                }},
};

static mc::variant convert_method_result(const mc::variants& arr) {
    if (arr.empty()) {
        return mc::variant();
    }
    if (arr.size() == 1) {
        return arr[0];
    }
    return arr;
}

sd_bus::sd_bus(bool start_now, bool is_blocking) : m_is_blocking(is_blocking),
m_connection(connection::open_session_bus(mc::get_io_context())), m_unique_name(std::string(m_connection.get_unique_name())) {
    if (start_now) {
        m_connection.start();
    }
}

variant sd_bus::dbus_call(mc::milliseconds timeout, std::string_view service_name,
                          std::string_view path, std::string_view interface, std::string_view method,
                          std::string_view signature, const variants& args) {
    auto               msg    = message::new_method_call(service_name, path, interface, method);
    auto               writer = msg.writer();
    signature_iterator it(signature);
    for (auto arg : args) {
        if (it.at_end()) {
            break;
        }
        writer.write_variant(it, arg, 0);
        it.next();
    }
    auto reply = m_connection.send_with_reply(std::move(msg), timeout);
    if (reply.is_valid() && reply.is_method_return()) {
        return convert_method_result(reply.read_args());
    }
    MC_THROW(mc::exception, "dbus call failed, error name: ${error_name}",
             ("error_name", reply.get_error_name()));
}

std::optional<variant> sd_bus::shm_timeout_call(mc::milliseconds timeout, std::string_view service_name,
                                                std::string_view path, std::string_view interface, std::string_view method,
                                                std::string_view signature, const variants& args) {
    auto result = shm_tree::timeout_call_with_sender(timeout, m_unique_name, service_name, path, interface, method, signature, args);
    if (result != std::nullopt) {
        return convert_method_result(result.value());
    }
    return std::nullopt;
}

variant sd_bus::timeout_call_impl(mc::milliseconds timeout, std::string_view service_name,
                                  std::string_view path, std::string_view interface, std::string_view method,
                                  std::string_view signature, const variants& args) {
    if (m_is_blocking) {
        return dbus_call(timeout, service_name, path, interface, method, signature, args);
    }
    auto result = shm_timeout_call(timeout, service_name, path, interface, method, signature, args);
    if (result != std::nullopt) {
        return result.value();
    }
    return dbus_call(timeout, service_name, path, interface, method, signature, args);
}

void sd_bus::set_enable_local_request(bool enable) {
    m_enable_local_request = enable;
}

static variants remove_context_arg(const variants& args) {
    if (args.empty() || !args[0].is_dict()) {
        return args;
    }
    variants args_without_context = args;
    args_without_context.erase(args.begin());
    return args_without_context;
}

std::optional<variant> sd_bus::devmon_chip_call(mc::milliseconds timeout,
                                                std::string_view path, std::string_view interface, std::string_view method,
                                                std::string_view signature, const variants& args) {
    auto interface_it = DEVMON_CHIP_METHODS.find(interface);
    if (interface_it == DEVMON_CHIP_METHODS.end()) {
        return std::nullopt;
    }

    auto method_it = interface_it->second.find(method);
    if (method_it == interface_it->second.end()) {
        return std::nullopt;
    }

    const auto& [target_method, target_signature] = method_it->second;
    return timeout_call_impl(timeout, DEVMON_SERVICE, path, DEVMON_CHIP_INTERFACE,
                             target_method, target_signature, remove_context_arg(args));
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

variant sd_bus::timeout_call(mc::milliseconds timeout, std::string_view service_name,
                             std::string_view path, std::string_view interface, std::string_view method,
                             std::string_view signature, const variants& args) {
    variants               modified_args = add_requestor_to_context(args, m_service_name);
    std::optional<variant> result        = std::nullopt;
    auto                   start         = std::chrono::steady_clock::now();
    if (service_name == DEVMON_SERVICE) {
        result = devmon_chip_call(timeout, path, interface, method, signature, modified_args);
    }
    if (result == std::nullopt) {
        result = timeout_call_impl(timeout, service_name, path, interface, method, signature, modified_args);
    }
    auto end      = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    if (duration >= CALL_TIMEOUT_SECONDS) {
        std::string caller = get_caller_from_context(modified_args);
        wlog("service[${caller}] request timeout: remote service[${service_name}], path[${path}],"
             " interface[${interface}], method[${method}], used time[${duration}s]",
             ("caller", caller)("service_name", service_name)("path", path)("interface", interface)("method", method)("duration", duration));
    }
    return result.value();
}

variant sd_bus::call(std::string_view service_name, std::string_view path, std::string_view interface,
                     std::string_view method, std::string_view signature, const variants& args) {
    return timeout_call(DEFAULT_DBUS_CALL_TIMEOUT, service_name, path, interface, method, signature, args);
}

sd_bus::pcall_result sd_bus::pcall(std::string_view service_name, std::string_view path, std::string_view interface,
                           std::string_view method, std::string_view signature, const variants& args) {
    return timeout_pcall(DEFAULT_DBUS_CALL_TIMEOUT, service_name, path, interface, method, signature, args);
}

sd_bus::pcall_result sd_bus::timeout_pcall(mc::milliseconds timeout, std::string_view service_name,
                                   std::string_view path, std::string_view interface,
                                   std::string_view method, std::string_view signature, const variants& args) {
    try {
        auto result = timeout_call(timeout, service_name, path, interface, method, signature, args);
        return {std::nullopt, std::move(result)};
    } catch (const mc::exception& e) {
        return {e.what(), variant()};
    }
}

} // namespace mc::dbus