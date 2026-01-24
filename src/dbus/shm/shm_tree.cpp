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
#include <glib-2.0/glib.h>
#include <mc/dbus/shm/serialize.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/json.h>
#include <mc/variant.h>
#include <type_traits>

#ifndef BUILD_TYPE_DEBUG
#define BUILD_TYPE_DEBUG (0x0b)
#endif

namespace mc::dbus {
static uint32_t g_serial = 0;

using mdb_interface = shm::mdb_interface;

// 前置声明：用于在文件前部的 SHM 快速路径中加对象锁
template <typename LibraryType>
static uint32_t get_object_id(std::string_view path);

namespace {

using shm_tree_handler_t = std::optional<mc::variants> (*)(
    std::string_view service_name, std::string_view path, std::string_view interface,
    std::string_view method, std::string_view signature, const variants& args);

// 前置声明：将 shm::property 转换为 mc::variant（读取时需已持有对应对象的共享锁）
static std::optional<variant> read_property_variant(const shm::property& prop);

std::optional<mc::variants> get_mdb_object_handler([[maybe_unused]] std::string_view service_name,
                                                   [[maybe_unused]] std::string_view path,
                                                   [[maybe_unused]] std::string_view interface,
                                                   [[maybe_unused]] std::string_view method,
                                                   [[maybe_unused]] std::string_view signature,
                                                   const variants&                   args) {
    // 第一个参数为上下文
    if (args.size() < 3) {
        return std::nullopt;
    }

    std::string_view arg_path = args[1].as_string();
    variants         arg_interfaces;
    if (args[2].is_array()) {
        arg_interfaces = args[2].get_array();
    } else {
        return std::nullopt;
    }

    auto result_dict = shm_tree::get_mdb_object(arg_path, arg_interfaces);
    if (!result_dict.has_value()) {
        return std::nullopt;
    }

    variants result;
    result.push_back(variant(result_dict.value()));
    return result;
}

std::optional<mc::variants> get_mdb_sub_paths_handler([[maybe_unused]] std::string_view service_name,
                                                      [[maybe_unused]] std::string_view path,
                                                      [[maybe_unused]] std::string_view interface,
                                                      [[maybe_unused]] std::string_view method,
                                                      [[maybe_unused]] std::string_view signature,
                                                      const variants&                   args) {
    if (args.size() < 4) {
        return std::nullopt;
    }

    std::string_view arg_path  = args[1].as_string();
    uint32_t         arg_depth = args[2].as_uint32();
    variants         arg_interfaces;
    if (args[3].is_array()) {
        arg_interfaces = args[3].get_array();
    } else {
        return std::nullopt;
    }

    uint32_t arg_skip        = args.size() >= 5 ? args[4].as_uint32() : 0;
    uint32_t arg_top         = args.size() >= 6 ? args[5].as_uint32() : 0;
    bool     arg_ignore_case = args.size() >= 7 ? args[6].as_bool() : false;

    auto result_arr = shm_tree::get_mdb_sub_paths(arg_path, arg_depth, arg_interfaces, arg_skip, arg_top,
                                                  arg_ignore_case);
    if (!result_arr.has_value()) {
        return std::nullopt;
    }

    variants result;
    result.push_back(variant(result_arr.value()));
    return result;
}

std::optional<mc::variants> is_valid_mdb_path_handler([[maybe_unused]] std::string_view service_name,
                                                      [[maybe_unused]] std::string_view path,
                                                      [[maybe_unused]] std::string_view interface,
                                                      [[maybe_unused]] std::string_view method,
                                                      [[maybe_unused]] std::string_view signature,
                                                      const variants&                   args) {
    if (args.size() < 3) {
        return std::nullopt;
    }
    std::string_view arg_path        = args[1].as_string();
    bool             arg_ignore_case = args[2].as_bool();

    variants result;
    result.push_back(variant(shm_tree::is_valid_mdb_path(arg_path, arg_ignore_case)));
    return result;
}

// 忽略大小写字符串比较
static bool equals_ignore_case(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) {
            return false;
        }
    }
    return true;
}

// 从属性中读取值并转换为 mc::variant，失败返回 std::nullopt
// 注意：调用方需要保证已持有对应对象的共享锁（shm_object_lock_shared_exec）。
static std::optional<variant> read_property_variant(const shm::property& prop) {
    auto sig = prop.get_signature();
    if (sig.empty()) {
        return std::nullopt;
    }
    auto data_opt = prop.get_data();
    if (!data_opt.has_value()) {
        return std::nullopt;
    }

    std::string_view data = data_opt.value();
    void*            buf  = g_malloc0(data.size());
    if (buf == nullptr) {
        return std::nullopt;
    }
    std::memcpy(buf, data.data(), data.size());
    gvariant_auto_free gv(
        g_variant_new_from_data(G_VARIANT_TYPE(sig.data()), buf, data.size(), false, g_free, buf));
    return gvariant_convert::to_mc_variant(gv.ptr);
}

enum class object_filter_result {
    matched,
    not_matched,
    need_rpc,
};

// 在持有共享内存全局锁的前提下，尝试仅使用共享内存的属性值进行匹配。
// 若存在属性值未落盘（只有属性名/签名但无 data），返回 need_rpc 供上层在释放全局锁后通过 RPC 再查。
static object_filter_result object_match_filter_shm(shm::object& obj, std::string_view iface_name,
                                                    const dict& filter_dict, bool ignore_case) {
    auto it_iface = obj.interfaces().find(iface_name);
    if (it_iface == obj.interfaces().end()) {
        return object_filter_result::not_matched;
    }
    auto iface = it_iface->second;

    bool need_rpc = false;
    for (const auto& e : filter_dict) {
        if (!e.key.is_string()) {
            return object_filter_result::not_matched;
        }
        std::string_view prop_name = e.key.as_string();
        const variant&   expected  = e.value;

        auto prop = iface->find_p(prop_name);
        if (prop == nullptr) {
            return object_filter_result::not_matched;
        }

        auto actual_opt = read_property_variant(*prop);
        if (!actual_opt.has_value()) {
            need_rpc = true;
            continue;
        }
        variant actual = actual_opt.value();

        if (ignore_case && actual.is_string() && expected.is_string()) {
            if (!equals_ignore_case(actual.as_string(), expected.as_string())) {
                return object_filter_result::not_matched;
            }
        } else {
            if (!(actual == expected)) {
                return object_filter_result::not_matched;
            }
        }
    }

    return need_rpc ? object_filter_result::need_rpc : object_filter_result::matched;
}

struct mdb_path_candidate {
    std::string service_name;
    std::string object_path;
};

struct mdb_path_shm_scan_out {
    std::optional<variants>         found;
    std::vector<mdb_path_candidate> need_rpc;
};

static variants make_empty_path_result() {
    variants empty;
    empty.push_back(variant(std::string_view{}));
    empty.push_back(variant(std::string_view{}));
    return empty;
}

// 将 Python 风格的单引号字典转换为 JSON 格式（双引号）
// 例如：{'key':'value'} -> {"key":"value"}
// 简单实现：将单引号替换为双引号，但跳过转义字符后的单引号
static std::string convert_single_quotes_to_json(std::string_view input) {
    std::string result;
    result.reserve(input.size());

    bool escape_next = false;

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];

        if (escape_next) {
            result += c;
            escape_next = false;
            continue;
        }

        if (c == '\\') {
            result += c;
            escape_next = true;
            continue;
        }

        if (c == '\'') {
            // 将单引号替换为双引号
            result += '"';
        } else {
            result += c;
        }
    }

    return result;
}

// RPC 调用相关的常量定义
namespace {
constexpr mc::milliseconds PROP_RPC_TIMEOUT{3000};
constexpr std::string_view MDB_SERVICE_NAME{"bmc.kepler.maca"};
constexpr std::string_view PROPS_IFACE{"bmc.kepler.Object.Properties"};
constexpr std::string_view GET_WITH_CTX{"GetWithContext"};
constexpr std::string_view GET_WITH_CTX_SIG{"a{ss}ss"};
} // namespace

// 构建 GetWithContext RPC 调用的参数
static variants build_get_property_rpc_args(std::string_view interface_name,
                                            std::string_view prop_name) {
    dict     ctx;
    variants rpc_args;
    rpc_args.push_back(variant(ctx));
    rpc_args.push_back(variant(interface_name));
    rpc_args.push_back(variant(prop_name));
    return rpc_args;
}

// 使用 RPC（GetWithContext）获取单个属性值（允许异常抛出，用于 call_shm_get_all_properties）
static std::optional<variant> get_property_via_rpc(std::string_view interface_name,
                                                   std::string_view prop_name,
                                                   std::string_view service_name,
                                                   std::string_view object_path) {
    variants rpc_args = build_get_property_rpc_args(interface_name, prop_name);

    // timeout_call_with_sender 可能抛出异常，这里不捕获，让异常向上传播
    auto reply_opt = shm_tree::timeout_call_with_sender(PROP_RPC_TIMEOUT, MDB_SERVICE_NAME,
                                                        {service_name, object_path, PROPS_IFACE,
                                                        GET_WITH_CTX, GET_WITH_CTX_SIG, rpc_args});

    if (!reply_opt.has_value() || reply_opt->empty()) {
        return std::nullopt;
    }

    return reply_opt->front();
}

// 使用 RPC（GetWithContext）获取单个属性值，捕获异常并返回错误信息（用于 call_shm_get_properties_by_names）
// 返回 pair：第一个是属性值（成功时），第二个是错误信息 variant（仅当 timeout_call_with_sender 抛出异常时）
static std::pair<std::optional<variant>, std::optional<variant>> get_property_via_rpc_with_error(
    std::string_view interface_name, std::string_view prop_name, std::string_view service_name,
    std::string_view object_path) {
    variants rpc_args = build_get_property_rpc_args(interface_name, prop_name);

    std::optional<variants> reply_opt;
    try {
        reply_opt = shm_tree::timeout_call_with_sender(PROP_RPC_TIMEOUT, MDB_SERVICE_NAME,
                                                       {service_name, object_path, PROPS_IFACE,
                                                       GET_WITH_CTX, GET_WITH_CTX_SIG, rpc_args});
    } catch (const std::exception& e) {
        return {std::nullopt, variant(std::string(e.what()))};
    } catch (...) {
        return {std::nullopt, variant(std::string("Unknown exception"))};
    }

    if (!reply_opt.has_value() || reply_opt->empty()) {
        return {std::nullopt, std::nullopt};
    }

    return {reply_opt->front(), std::nullopt};
}

// 使用 RPC（GetWithContext）获取单个属性值并与期望值比较
static bool match_property_via_rpc(std::string_view interface_name, std::string_view prop_name,
                                   const variant& expected, bool ignore_case,
                                   std::string_view service_name, std::string_view object_path) {
    auto actual_opt = get_property_via_rpc(interface_name, prop_name, service_name, object_path);
    if (!actual_opt.has_value()) {
        return false;
    }

    variant actual = actual_opt.value();
    if (ignore_case && actual.is_string() && expected.is_string()) {
        return equals_ignore_case(actual.as_string(), expected.as_string());
    }
    return actual == expected;
}

// 对一个候选对象，使用 RPC 检查是否满足所有过滤条件
static bool match_candidate_via_rpc(const mdb_path_candidate& cand,
                                    std::string_view          interface_name,
                                    const dict&               filter_dict,
                                    bool                      ignore_case) {
    for (const auto& e : filter_dict) {
        if (!e.key.is_string()) {
            return false;
        }
        std::string_view prop_name = e.key.as_string();
        const variant&   expected  = e.value;

        if (!match_property_via_rpc(interface_name, prop_name, expected, ignore_case,
                                    cand.service_name, cand.object_path)) {
            return false;
        }
    }
    return true;
}

// 共享内存扫描阶段：仅用共享内存中的已落盘属性值匹配；若发现缺值则记录 candidate，
// 交给上层在释放全局锁后通过 RPC 再验证。
static mdb_path_shm_scan_out scan_mdb_path_shm(shm::shared_memory& ins, std::string_view iface_name,
                                               const dict& filter_dict, bool ignore_case) {
    mdb_path_shm_scan_out out;
    variants              result;
    auto&                 ins_base   = ins.get_base();
    auto&                 object_map = ins_base.object_map;

    for (const auto& service_pair : object_map) {
        for (const auto& class_pair : service_pair.second) {
            for (const auto& obj_pair : class_pair.second) {
                auto obj_ptr = obj_pair.second;
                if (obj_ptr == nullptr) {
                    continue;
                }
                auto* obj = &*obj_ptr;

                const auto r = object_match_filter_shm(*obj, iface_name, filter_dict, ignore_case);
                if (r == object_filter_result::not_matched) {
                    continue;
                }

                if (r == object_filter_result::matched) {
                    // 找到第一个匹配对象，返回 [path, service_name]
                    result.push_back(variant(std::string(obj->path())));
                    result.push_back(variant(std::string(service_pair.first)));
                    out.found = result;
                    return out;
                }

                // 共享内存缺值，记录 candidate，稍后释放锁后通过 RPC 再查
                out.need_rpc.push_back(
                    mdb_path_candidate{std::string(service_pair.first), std::string(obj->path())});
            }
        }
    }

    return out;
}

std::optional<mc::variants> get_mdb_path_handler([[maybe_unused]] std::string_view service_name,
                                                 [[maybe_unused]] std::string_view path,
                                                 [[maybe_unused]] std::string_view interface,
                                                 [[maybe_unused]] std::string_view method,
                                                 [[maybe_unused]] std::string_view signature,
                                                 const variants&                   args) {
    if (args.size() < 4) {
        return std::nullopt;
    }
    std::string_view arg_interface   = args[1].as_string();
    std::string_view filter_str      = args[2].as_string();
    bool             arg_ignore_case = args[3].as_bool();

    // 直接传递 JSON 字符串，get_mdb_path 内部会处理 JSON decode 和错误处理
    variants path_and_service = shm_tree::get_mdb_path(arg_interface, filter_str, arg_ignore_case);
    return path_and_service;
}

std::optional<mc::variants> get_mdb_interface_owners_handler([[maybe_unused]] std::string_view service_name,
                                                             [[maybe_unused]] std::string_view path,
                                                             [[maybe_unused]] std::string_view interface,
                                                             [[maybe_unused]] std::string_view method,
                                                             [[maybe_unused]] std::string_view signature,
                                                             const variants&                   args) {
    if (args.size() < 2) {
        return std::nullopt;
    }
    std::string_view arg_interface = args[1].as_string();

    variants result;
    result.push_back(variant(shm_tree::get_mdb_interface_owners(arg_interface)));
    return result;
}

std::optional<mc::variants> get_mdb_service_name_handler([[maybe_unused]] std::string_view service_name,
                                                         [[maybe_unused]] std::string_view path,
                                                         [[maybe_unused]] std::string_view interface,
                                                         [[maybe_unused]] std::string_view method,
                                                         [[maybe_unused]] std::string_view signature,
                                                         const variants&                   args) {
    if (args.size() < 2) {
        return std::nullopt;
    }
    std::string_view sender = args[1].as_string();
    variants         result;
    result.push_back(variant(shm_tree::get_mdb_service_name(sender)));
    return result;
}

std::optional<mc::variants> get_mdb_sub_objects_handler([[maybe_unused]] std::string_view service_name,
                                                        [[maybe_unused]] std::string_view path,
                                                        [[maybe_unused]] std::string_view interface,
                                                        [[maybe_unused]] std::string_view method,
                                                        [[maybe_unused]] std::string_view signature,
                                                        const variants&                   args) {
    if (args.size() < 4) {
        return std::nullopt;
    }
    std::string_view arg_path  = args[1].as_string();
    uint32_t         arg_depth = args[2].as_uint32();
    variants         arg_interfaces;
    if (args[3].is_array()) {
        arg_interfaces = args[3].get_array();
    } else {
        return std::nullopt;
    }

    variants result;
    auto     ret = shm_tree::get_mdb_sub_objects(arg_path, arg_depth, arg_interfaces);
    if (!ret.has_value()) {
        return std::nullopt;
    }
    result.push_back(variant(ret.value()));
    return result;
}

std::optional<mc::variants> get_mdb_parent_objects_handler([[maybe_unused]] std::string_view service_name,
                                                           [[maybe_unused]] std::string_view path,
                                                           [[maybe_unused]] std::string_view interface,
                                                           [[maybe_unused]] std::string_view method,
                                                           [[maybe_unused]] std::string_view signature,
                                                           const variants&                   args) {
    if (args.size() < 4) {
        return std::nullopt;
    }
    std::string_view arg_path = args[1].as_string();
    variants         arg_interfaces;
    if (args[2].is_array()) {
        arg_interfaces = args[2].get_array();
    } else {
        return std::nullopt;
    }
    variants result;
    auto     ret = shm_tree::get_mdb_parent_objects(arg_path, arg_interfaces);
    if (!ret.has_value()) {
        return std::nullopt;
    }
    result.push_back(variant(ret.value()));
    return result;
}

std::optional<mc::variants> get_mdb_service_names_handler([[maybe_unused]] std::string_view service_name,
                                                          [[maybe_unused]] std::string_view path,
                                                          [[maybe_unused]] std::string_view interface,
                                                          [[maybe_unused]] std::string_view method,
                                                          [[maybe_unused]] std::string_view signature,
                                                          [[maybe_unused]] const variants&  args) {
    variants result;
    result.push_back(variant(shm_tree::get_mdb_service_names()));
    return result;
}

std::optional<mc::variants> get_mdb_classes_handler([[maybe_unused]] std::string_view service_name,
                                                    [[maybe_unused]] std::string_view path,
                                                    [[maybe_unused]] std::string_view interface,
                                                    [[maybe_unused]] std::string_view method,
                                                    [[maybe_unused]] std::string_view signature,
                                                    [[maybe_unused]] const variants&  args) {
    if (args.size() < 2) {
        return std::nullopt;
    }
    variants         result;
    std::string_view arg_service_name = args[1].as_string();
    result.push_back(variant(shm_tree::get_mdb_classes(arg_service_name)));
    return result;
}

std::optional<mc::variants> get_mdb_object_list_handler([[maybe_unused]] std::string_view service_name,
                                                        [[maybe_unused]] std::string_view path,
                                                        [[maybe_unused]] std::string_view interface,
                                                        [[maybe_unused]] std::string_view method,
                                                        [[maybe_unused]] std::string_view signature,
                                                        [[maybe_unused]] const variants&  args) {
    if (args.size() < 2) {
        return std::nullopt;
    }
    std::string_view arg_class_name = args[1].as_string();
    variants         result;
    result.push_back(variant(shm_tree::get_mdb_object_list(arg_class_name)));
    return result;
}

std::optional<mc::variants> get_mdb_object_owner_handler([[maybe_unused]] std::string_view service_name,
                                                         [[maybe_unused]] std::string_view path,
                                                         [[maybe_unused]] std::string_view interface,
                                                         [[maybe_unused]] std::string_view method,
                                                         [[maybe_unused]] std::string_view signature,
                                                         [[maybe_unused]] const variants&  args) {
    if (args.size() < 2) {
        return std::nullopt;
    }
    std::string_view arg_object_name = args[1].as_string();
    variants         result;
    result.push_back(variant(shm_tree::get_mdb_object_owner(arg_object_name)));
    return result;
}

std::optional<mc::variants> get_mdb_matched_objects_handler([[maybe_unused]] std::string_view service_name,
                                                            [[maybe_unused]] std::string_view path,
                                                            [[maybe_unused]] std::string_view interface,
                                                            [[maybe_unused]] std::string_view method,
                                                            [[maybe_unused]] std::string_view signature,
                                                            [[maybe_unused]] const variants&  args) {
    if (args.size() < 3) {
        return std::nullopt;
    }
    std::string_view arg_service_name  = args[1].as_string();
    std::string_view arg_iface_pattern = args[2].as_string();
    variants         result;
    result.push_back(variant(shm_tree::get_mdb_matched_objects(arg_service_name, arg_iface_pattern)));
    return result;
}

std::optional<mc::variants> call_shm_get_property_handler(std::string_view                  service_name,
                                                          std::string_view                  path,
                                                          [[maybe_unused]] std::string_view interface,
                                                          [[maybe_unused]] std::string_view method,
                                                          [[maybe_unused]] std::string_view signature,
                                                          const variants&                   args) {
    // args[0] 预留给上下文，这里从 args[1]/args[2] 读取 interface/property
    if (args.size() < 3 || !args[1].is_string() || !args[2].is_string()) {
        return std::nullopt;
    }

    std::string_view arg_interface = args[1].as_string();
    std::string_view arg_property  = args[2].as_string();

    auto value_opt = shm_tree::call_shm_get_property(service_name, path, arg_interface, arg_property);
    if (!value_opt.has_value()) {
        return std::nullopt;
    }

    variants result;
    result.push_back(variant(value_opt.value()));
    return result;
}

std::optional<mc::variants> call_shm_get_all_properties_handler(std::string_view                  service_name,
                                                                std::string_view                  path,
                                                                [[maybe_unused]] std::string_view interface,
                                                                [[maybe_unused]] std::string_view method,
                                                                [[maybe_unused]] std::string_view signature,
                                                                const variants&                   args) {
    // args[0] 可能为上下文（对于 GetWithContext），args[1] 为接口名
    // 对于 org.freedesktop.DBus.Properties.GetAll，args[0] 就是接口名
    std::string_view arg_interface;
    if (args.size() >= 1 && args[0].is_string()) {
        arg_interface = args[0].as_string();
    } else if (args.size() >= 2 && args[1].is_string()) {
        arg_interface = args[1].as_string();
    } else {
        return std::nullopt;
    }

    auto props_dict_opt = shm_tree::call_shm_get_all_properties(service_name, path, arg_interface);
    if (!props_dict_opt.has_value()) {
        return std::nullopt;
    }

    variants result;
    result.push_back(variant(props_dict_opt.value()));
    return result;
}

std::optional<mc::variants> call_shm_get_properties_by_names_handler([[maybe_unused]] std::string_view service_name,
                                                                     std::string_view                  path,
                                                                     [[maybe_unused]] std::string_view interface,
                                                                     [[maybe_unused]] std::string_view method,
                                                                     [[maybe_unused]] std::string_view signature,
                                                                     const variants&                   args) {
    // bmc.kepler.Object.Properties.GetPropertiesByNames(ctx, interface, [names...])
    if (args.size() < 3 || !args[1].is_string() || !args[2].is_array()) {
        return std::nullopt;
    }

    std::string_view iface_name = args[1].as_string();
    variants         prop_names = args[2].get_array();

    auto result_opt = shm_tree::call_shm_get_properties_by_names(service_name, path, iface_name, prop_names);
    if (!result_opt.has_value()) {
        return std::nullopt;
    }

    // 直接返回，函数内部已经返回了包含两个 dict 的 variants
    return result_opt;
}

struct call_shm_interface_config {
    std::optional<std::string>                          service;
    std::optional<std::string>                          path;
    std::unordered_map<std::string, shm_tree_handler_t> methods;
};

static const std::unordered_map<std::string_view, call_shm_interface_config>
    g_call_shm_interface_config = {
        {"bmc.kepler.Mdb",
         call_shm_interface_config{
             std::make_optional<std::string>("bmc.kepler.maca"),
             std::make_optional<std::string>("/bmc/kepler/MdbService"),
             {{"GetObject", get_mdb_object_handler},
              {"GetSubPaths", get_mdb_sub_paths_handler},
              {"GetSubPathsPaging", get_mdb_sub_paths_handler},
              {"IsValidPath", is_valid_mdb_path_handler},
              {"GetPath", get_mdb_path_handler},
              {"GetInterfaceOwners", get_mdb_interface_owners_handler},
              {"GetServiceName", get_mdb_service_name_handler},
              {"GetSubObjects", get_mdb_sub_objects_handler},
              {"GetParentObjects", get_mdb_parent_objects_handler},
              {"GetServiceNames", get_mdb_service_names_handler},
              {"GetClasses", get_mdb_classes_handler},
              {"GetObjectList", get_mdb_object_list_handler},
              {"GetObjectOwner", get_mdb_object_owner_handler},
              {"GetMatchedObjects", get_mdb_matched_objects_handler}}}},
        {"org.freedesktop.DBus.Properties",
         call_shm_interface_config{
             std::nullopt,
             std::nullopt,
             {{"Get", call_shm_get_property_handler},
              {"GetAll", call_shm_get_all_properties_handler}}}},
        {"bmc.kepler.Object.Properties",
         call_shm_interface_config{
             std::nullopt,
             std::nullopt,
             {{"GetWithContext", call_shm_get_property_handler},
              {"GetAllWithContext", call_shm_get_all_properties_handler},
              {"GetPropertiesByNames", call_shm_get_properties_by_names_handler}}}}};

static bool has_interface(shm::mdb_object& obj, const variants& interfaces) {
    if (interfaces.empty()) {
        return true;
    }

    for (const auto& iface : interfaces) {
        if (!iface.is_string()) {
            continue;
        }
        std::string_view iface_name = iface.as_string();
        auto             ifaces     = obj.find_interfaces(iface_name);
        if (ifaces == nullptr) {
            return false;
        }
    }

    return true;
}

shm::object_tree* find_tree(std::string_view name) {
    auto& ins      = shm::shared_memory::get_instance();
    auto& name_map = ins.get_object_tree_map(name);
    auto  it       = name_map.find(name);
    if (it == name_map.end()) {
        return nullptr;
    }
    return &*it->second;
}

} // namespace

std::optional<mc::variants> shm_tree::get_mdb_info(const method_call_params& params) {
    auto it_interface = g_call_shm_interface_config.find(params.interface);
    if (it_interface == g_call_shm_interface_config.end()) {
        return std::nullopt;
    }
    const auto& iface_cfg = it_interface->second;

    // 如果配置中限定了 service，则要求精确匹配
    if (iface_cfg.service.has_value() && iface_cfg.service.value() != params.service_name) {
        return std::nullopt;
    }

    // 如果配置中限定了 path，则要求精确匹配
    if (iface_cfg.path.has_value() && iface_cfg.path.value() != params.path) {
        return std::nullopt;
    }

    auto it_method = iface_cfg.methods.find(std::string(params.method));
    if (it_method == iface_cfg.methods.end()) {
        return std::nullopt;
    }

    // 直接通过成员函数指针调用，只需要一次查找
    const auto& handler = it_method->second;
    if (handler == nullptr) {
        return std::nullopt;
    }

    return handler(params.service_name, params.path, params.interface, params.method, params.signature, params.args);
}

variants shm_tree::get_mdb_path(std::string_view interface_name, std::string_view filter_json,
                                bool ignore_case) {
    // 解析 JSON 字符串为 dict
    variant          filter;
    std::string      json_str;
    std::string_view json_to_parse = filter_json;

    // 如果包含单引号，尝试转换为 JSON 格式
    if (filter_json.find('\'') != std::string_view::npos) {
        json_str      = convert_single_quotes_to_json(filter_json);
        json_to_parse = json_str;
    }

    try {
        filter = mc::json::json_decode(json_to_parse);
    } catch (const mc::parse_error_exception&) {
        // JSON 解析失败，返回空结果
        return make_empty_path_result();
    }

    if (!filter.is_dict()) {
        // 不是字典类型，返回空结果
        return make_empty_path_result();
    }

    const dict& filter_dict = filter.get_object();
    return get_mdb_path_impl(interface_name, filter_dict, ignore_case);
}

variants shm_tree::get_mdb_path_impl(std::string_view interface_name, const dict& filter_dict,
                                     bool ignore_case) {
    auto& ins = shm::shared_memory::get_instance();

    // 1) 共享内存扫描阶段：在全局共享锁内仅做“已落盘属性”的匹配；缺值的对象延后处理。
    auto shm_out = shm_global_lock_shared_exec(scan_mdb_path_shm, std::ref(ins), interface_name,
                                               std::cref(filter_dict), ignore_case);
    if (shm_out.found.has_value()) {
        return shm_out.found.value();
    }

    // 2) RPC 回退阶段：释放全局锁后，对缺值对象逐个通过 RPC 查询属性值再匹配。
    // 说明：harbor::get_destination_msg_queue 内部会获取全局共享锁，必须避免锁重入。
    for (const auto& cand : shm_out.need_rpc) {
        if (match_candidate_via_rpc(cand, interface_name, filter_dict, ignore_case)) {
            variants result;
            result.push_back(variant(cand.object_path));
            result.push_back(variant(cand.service_name));
            return result;
        }
    }

    // 未找到匹配对象，返回 ["", ""]
    return make_empty_path_result();
}

std::optional<dict> shm_tree::get_mdb_object(std::string_view path, const variants& interfaces) {
    auto& ins = shm::shared_memory::get_instance();

    return shm_global_lock_shared_exec([&]() -> std::optional<dict> {
        auto obj = ins.find_mdb_object(path, false);
        if (obj == nullptr) {
            return std::nullopt;
        }

        // 返回数据格式为：{"service_name": ["interface1", "interface2"]}
        dict result_dict;
        for (const auto& iface_name_variant : interfaces) {
            if (!iface_name_variant.is_string()) {
                continue;
            }
            std::string_view iface_name = iface_name_variant.as_string();
            auto             ifaces     = obj->find_interfaces(iface_name);
            if (ifaces == nullptr) {
                continue;
            }

            for (const auto& iface : *ifaces) {
                std::string_view service_name = iface.first;

                auto it = result_dict.find(service_name);
                if (it != result_dict.end()) {
                    // 找到 service_name，将 iface_name 添加到对应的数组中
                    variants arr;
                    if (it->value.is_array()) {
                        arr = it->value.get_array();
                    }
                    arr.push_back(variant(iface_name));
                    result_dict[service_name] = variant(arr);
                } else {
                    // 没找到，创建新数组并添加到 result_dict
                    variants arr;
                    arr.push_back(variant(iface_name));
                    result_dict[service_name] = variant(arr);
                }
            }
        }
        return result_dict;
    });
}

std::optional<variants> shm_tree::get_mdb_sub_paths(std::string_view path, uint32_t depth,
                                                    const variants& interfaces, uint32_t skip,
                                                    uint32_t top, bool ignore_case) {
    if (top <= 0) {
        top = std::numeric_limits<uint32_t>::max();
    }

    auto& ins = shm::shared_memory::get_instance();
    return shm_global_lock_shared_exec([&]() -> std::optional<variants> {
        auto&    ins_base = ins.get_base();
        variants result_arr;
        // 先验证路径是否存在
        auto obj = ins.find_mdb_object(path, ignore_case);
        if (obj == nullptr) {
            // 路径不存在，返回空列表
            return result_arr;
        }
        // 路径存在，使用 find_node 遍历子树
        auto node = ins_base.objects.find_node(path, ignore_case);
        if (node == nullptr) {
            // find_node 找不到但 find_mdb_object 找到了，说明路径存在但没有子节点，返回空列表
            return result_arr;
        }
        uint32_t travel_count = 0;
        uint32_t arr_count    = 0;
        node->travel([&](shm::mdb_object& obj, int /*level*/) {
            if (!has_interface(obj, interfaces) || ++travel_count <= skip) {
                return true;
            }

            result_arr.push_back(variant(obj.path()));
            return arr_count < top;
        }, depth, true);

        return result_arr;
    });
}

bool shm_tree::is_valid_mdb_path(std::string_view path, bool ignore_case) {
    auto& ins = shm::shared_memory::get_instance();
    return shm_global_lock_shared_exec([&]() {
        auto obj = ins.find_mdb_object(path, ignore_case);
        return obj != nullptr;
    });
}

variants shm_tree::get_mdb_interface_owners(std::string_view interface_name) {
    variants result_arr;
    auto&    ins = shm::shared_memory::get_instance();

    shm_global_lock_shared_exec([&]() {
        auto f = [&result_arr](mdb_interface& iface) -> bool {
            variants arr;
            arr.push_back(variant(iface.o->get_tree()->unique_name()));
            arr.push_back(variant(iface.o->path()));
            result_arr.push_back(variant(arr));
            return false;
        };
        ins.query_interface_view(interface_name, f);
    });

    return result_arr;
}

std::string_view shm_tree::get_mdb_service_name(std::string_view sender) {
    auto tree = find_tree(sender);
    if (tree == nullptr) {
        return std::string_view();
    }
    return tree->wellknow_name();
}

static void append_iface(dict& service_map, std::string_view service_name, std::string_view iface_name) {
    variants arr;
    auto     it = service_map.find(service_name);
    if (it != service_map.end() && it->value.is_array()) {
        arr = it->value.get_array();
    }
    arr.push_back(variant(iface_name));
    service_map[service_name] = variant(arr);
}

// 收集对象的接口信息到 service_map，格式：{service_name -> [iface1, iface2, ...]}
static void collect_object_interfaces(dict& service_map, shm::mdb_object& obj, const variants& interfaces) {
    if (interfaces.empty()) {
        // 收集所有接口
        for (const auto& iface_pair : obj.interfaces()) {
            std::string_view iface_name = iface_pair.first;
            for (const auto& svc_pair : iface_pair.second) {
                std::string_view service_name = svc_pair.first;
                append_iface(service_map, service_name, iface_name);
            }
        }
    } else {
        // 只收集指定的接口
        for (const auto& iface_v : interfaces) {
            if (!iface_v.is_string()) {
                continue;
            }
            std::string_view iface_name = iface_v.as_string();
            auto             svcs       = obj.find_interfaces(iface_name);
            if (svcs == nullptr) {
                continue;
            }
            for (const auto& svc_pair : *svcs) {
                std::string_view service_name = svc_pair.first;
                append_iface(service_map, service_name, iface_name);
            }
        }
    }
}

// 获取路径的父路径，如 "/a/b" -> "/a"，"/a" -> "/"
static std::string_view get_parent_path(std::string_view path) {
    // 非绝对路径、空路径或根路径都视为没有父路径
    if (path.size() < 2 || path.at(0) != '/') {
        return {};
    }

    // 从后往前查找最后一个 '/'
    auto last_slash = path.rfind('/');
    if (last_slash == 0) {
        // 父路径为根路径 "/"
        return std::string_view(path.data(), 1);
    }

    return path.substr(0, last_slash);
}

struct g_regex_deleter {
    void operator()(GRegex* regex) const {
        if (regex != nullptr) {
            g_regex_unref(regex);
        }
    }
};

// 接口名称匹配：使用 GLib 正则；与原接口一致，不对 '%' 做替换
static std::unique_ptr<GRegex, g_regex_deleter> compile_iface_regex(std::string_view iface_pattern) {
    std::string pattern_str(iface_pattern);
    GError*     error = nullptr;
    auto        regex =
        std::unique_ptr<GRegex, g_regex_deleter>(g_regex_new(pattern_str.c_str(), (GRegexCompileFlags)0,
                                                             (GRegexMatchFlags)0, &error));
    if (regex == nullptr) {
        return nullptr;
    }

    return regex;
}

// 接口名称匹配：iface_regex 为 nullptr 表示全匹配
static bool iface_match(const GRegex* iface_regex, std::string_view iface_name) {
    if (iface_regex == nullptr) {
        return true;
    }
    // 使用 match_full 避免要求字符串以 '\0' 结尾
    return g_regex_match_full(iface_regex, iface_name.data(),
                              static_cast<gint>(iface_name.size()), 0, (GRegexMatchFlags)0,
                              nullptr, nullptr) != FALSE;
}

// 从 shm::object 收集匹配的接口名
static variants collect_matched_ifaces(shm::object& obj, const GRegex* iface_regex) {
    variants matched;
    for (const auto& p : obj.interfaces()) {
        std::string_view iface_sv(p.first.data(), p.first.size());
        if (iface_match(iface_regex, iface_sv)) {
            matched.push_back(variant(iface_sv));
        }
    }
    return matched;
}

// 从 shm::object 获取对象名：优先从 bmc.kepler.Object.Properties 的 ObjectName 属性读取，
// 无法获取时使用完整路径。调用方需已持有 shm_global_lock_shared_exec。
static std::string get_object_name_from_object(shm::object& obj) {
    std::string_view path_sv = obj.path();
    shm::interface*  intf    = nullptr;
    for (const auto& p : obj.interfaces()) {
        if (std::string_view(p.first.data(), p.first.size()) == PROPS_IFACE) {
            intf = p.second ? &*p.second : nullptr;
            break;
        }
    }
    if (intf == nullptr) {
        return std::string(path_sv);
    }
    auto prop = intf->find_p("ObjectName");
    if (!prop) {
        return std::string(path_sv);
    }
    auto name_opt = shm_object_lock_shared_exec(get_object_id<shmlock::ShmLockManager>(path_sv),
                                                [&]() -> std::optional<std::string> {
        auto v = read_property_variant(*prop);
        if (v && v->is_string()) {
            return std::string(v->as_string());
        }
        return std::nullopt;
    });
    if (name_opt) {
        return *name_opt;
    }
    return std::string(path_sv);
}

std::optional<dict> shm_tree::get_mdb_sub_objects(std::string_view path, uint32_t depth,
                                                  const variants& interfaces) {
    auto& ins = shm::shared_memory::get_instance();

    return shm_global_lock_shared_exec([&]() -> std::optional<dict> {
        dict result;
        // 先验证路径是否存在
        auto obj = ins.find_mdb_object(path, false);
        if (obj == nullptr) {
            // 路径不存在，返回空字典
            return result;
        }
        // 路径存在，使用 find_node 遍历子树
        auto& ins_base = ins.get_base();
        auto  node     = ins_base.objects.find_node(path, false);
        if (node == nullptr) {
            // find_node 找不到但 find_mdb_object 找到了，说明路径存在但没有子节点，返回空字典
            return result;
        }

        node->travel(
            [&](shm::mdb_object& obj, int /*level*/) {
            if (!has_interface(obj, interfaces)) {
                return true;
            }

            dict service_map;
            collect_object_interfaces(service_map, obj, interfaces);

            if (!service_map.empty()) {
                result[obj.path()] = variant(service_map);
            }
            return true;
        }, depth, true);

        return result;
    });
}

std::optional<dict> shm_tree::get_mdb_parent_objects(std::string_view path, const variants& interfaces) {
    auto& ins = shm::shared_memory::get_instance();

    return shm_global_lock_shared_exec([&]() -> std::optional<dict> {
        dict result;
        // 计算第一个父路径，如果没有有效父路径直接返回空结果
        auto parent_path = get_parent_path(path);
        if (parent_path.empty()) {
            return result;
        }

        // 从父路径开始向上遍历
        while (!parent_path.empty()) {
            auto obj = ins.find_mdb_object(parent_path, false);
            if (obj != nullptr && has_interface(*obj, interfaces)) {
                dict service_map;
                collect_object_interfaces(service_map, *obj, interfaces);

                if (!service_map.empty()) {
                    result[parent_path] = variant(service_map);
                }
            }

            // 如果已经是根路径，停止遍历
            if (parent_path == "/") {
                break;
            }
            parent_path = get_parent_path(parent_path);
        }

        return result;
    });
}

variants shm_tree::get_mdb_service_names() {
    variants result;
    auto&    ins            = shm::shared_memory::get_instance();
    auto&    wellknow_names = ins.get_wellknow_names();
    for (const auto& [name, value] : wellknow_names) {
        // 剔除前缀是 harbor 的服务名称
        if (name.size() >= 6 && name.substr(0, 6) == "harbor") {
            continue;
        }
        result.push_back(variant(name));
    }
    return result;
}

variants shm_tree::get_mdb_classes(std::string_view service_name) {
    variants result;
    auto&    ins = shm::shared_memory::get_instance();

    return shm_global_lock_shared_exec([&]() {
        auto& ins_base   = ins.get_base();
        auto& object_map = ins_base.object_map;
        if (service_name.empty()) {
            // service_name 为空：返回所有服务的 class_name（可能存在重复，由上层决定是否去重）
            for (const auto& service_map : object_map) {
                for (const auto& class_map : service_map.second) {
                    result.push_back(variant(class_map.first));
                }
            }
            return result;
        }

        auto it = object_map.find(service_name);
        if (it == object_map.end()) {
            return result;
        }

        for (const auto& class_map : it->second) {
            result.push_back(variant(class_map.first));
        }
        return result;
    });
}

variants shm_tree::get_mdb_object_list(std::string_view class_name) {
    variants result;
    auto&    ins = shm::shared_memory::get_instance();

    return shm_global_lock_shared_exec([&]() {
        if (class_name.empty()) {
            return result;
        }

        auto& ins_base   = ins.get_base();
        auto& object_map = ins_base.object_map;

        // object_map: [service_name] -> {[class_name] -> {[obj_name] -> object_ptr}}
        // 遍历所有 service，查找指定 class_name 的所有对象
        for (const auto& service_pair : object_map) {
            for (const auto& class_pair : service_pair.second) {
                const auto class_sv =
                    std::string_view(class_pair.first.c_str(), class_pair.first.size());
                if (class_sv != class_name) {
                    continue;
                }

                for (const auto& obj_pair : class_pair.second) {
                    const auto obj_name_sv =
                        std::string_view(obj_pair.first.c_str(), obj_pair.first.size());
                    const auto obj_ptr = obj_pair.second;
                    if (obj_ptr == nullptr) {
                        continue;
                    }
                    auto* obj  = &*obj_ptr;
                    auto* tree = obj->get_tree();

                    variants service_path_pair;
                    service_path_pair.push_back(
                        variant(tree ? tree->wellknow_name() : std::string_view{}));
                    service_path_pair.push_back(variant(obj->path()));

                    variants service_path_arr;
                    service_path_arr.push_back(variant(service_path_pair));

                    variants entry;
                    entry.push_back(variant(obj_name_sv));
                    entry.push_back(variant(service_path_arr));
                    result.push_back(variant(entry));
                }
            }
        }

        return result;
    });
}

variants shm_tree::get_mdb_object_owner(std::string_view object_name) {
    variants result;
    if (object_name.empty()) {
        return result;
    }

    auto& ins = shm::shared_memory::get_instance();

    return shm_global_lock_shared_exec([&]() {
        auto& ins_base  = ins.get_base();
        auto& obj_names = ins_base.object_names;

        // object_names 结构：{[object_name] = {[key] = object_ptr}}
        // 这里根据 object_name 直接定位到对象列表，再遍历所有 owner
        auto itv = obj_names.find(object_name);
        if (itv == obj_names.end()) {
            return result;
        }

        const auto& object_list = itv->second;
        for (const auto& obj_pair : object_list) {
            auto obj_ptr = obj_pair.second;
            if (obj_ptr == nullptr) {
                continue;
            }

            auto* obj  = &*obj_ptr;
            auto* tree = obj->get_tree();

            variants entry;
            entry.push_back(variant(tree ? tree->wellknow_name() : std::string_view{}));
            entry.push_back(variant(obj->path()));
            result.push_back(variant(entry));
        }

        return result;
    });
}

variants shm_tree::get_mdb_matched_objects(std::string_view service_name, std::string_view iface_pattern) {
    variants result;
    auto&    ins = shm::shared_memory::get_instance();

    // iface_pattern 为空表示匹配所有接口；否则按正则匹配
    const auto iface_regex = iface_pattern.empty() ? nullptr : compile_iface_regex(iface_pattern);
    if (!iface_pattern.empty() && iface_regex == nullptr) {
        return result;
    }

    return shm_global_lock_shared_exec([&]() {
        auto& w  = ins.get_wellknow_names();
        auto  cb = [&](shm::object& obj, int /*level*/) {
            auto matched = collect_matched_ifaces(obj, iface_regex ? iface_regex.get() : nullptr);
            if (matched.empty()) {
                return true;
            }
            std::string obj_name = get_object_name_from_object(obj);
            variants    entry;
            entry.push_back(variant(obj_name));
            entry.push_back(variant(obj.path()));
            entry.push_back(variant(matched));
            result.push_back(variant(entry));
            return true;
        };
        if (!service_name.empty()) {
            auto it = w.find(service_name);
            if (it != w.end()) {
                it->second->travel(cb, 0, true);
            }
        } else {
            for (const auto& p : w) {
                p.second->travel(cb, 0, true);
            }
        }
        return result;
    });
}

shm_tree::shm_tree(std::string_view harbor_name, std::string_view service_name,
                   std::string_view unique_name)
    : m_service_name(service_name), m_unique_name(unique_name),
      m_tree(create_shm_tree(harbor_name, service_name, unique_name)) {
}

void shm_tree::register_object(mc::engine::abstract_object& obj) {
#if defined(ENABLE_CONAN_COMPILE) && ENABLE_CONAN_COMPILE == 1
    auto&           ins     = shm::shared_memory::get_instance();
    auto            path    = obj.get_object_path();
    shm::object&    shm_obj = m_tree->register_object(ins, path);
    shm_obj_visitor visitor(shm_obj, obj);
    obj.get_metadata().visit(visitor);
    // 需要创建上下文，common_properties_interface才能获取到对象信息
    mc::engine::object_call_stack::context object_ctx{obj.get_service(), obj};

    const auto&                    metadata = mc::engine::common_properties_interface::get_instance().get_metadata();
    mc::engine::interface_metadata iface_metadata{nullptr, &metadata};
    visitor.handle_interface_begin(iface_metadata);
    metadata.visit(visitor);
    visitor.handle_interface_end(iface_metadata);

    shm_obj.add_named_object_view(ins, mc::engine::common_properties_name);
    obj.property_update_shm().connect([this, &obj](const mc::variant& value, const auto& prop) {
        auto iface     = prop.get_interface()->get_interface_name();
        auto prop_name = prop.get_name();
        set_property(m_service_name, obj.get_object_path(), iface, prop_name, value);
    });
#endif
}

void shm_tree::unregister_object(std::string_view path) {
    auto& ins = shm::shared_memory::get_instance();
    m_tree->unregister_object(ins, path);
}

static uint32_t set_serial(local_msg& msg) {
    uint32_t serial = msg.get_serial();
    if (serial != 0) {
        return serial;
    }
    if (g_serial == UINT32_MAX) {
        g_serial = 0;
    }
    g_serial++;
    msg.set_serial(g_serial);
    return g_serial;
}

std::optional<mc::variants> shm_tree::timeout_call(mc::milliseconds timeout, const method_call_params& params) {
    return timeout_call_with_sender(timeout, m_unique_name, params);
}

std::optional<mc::variants> shm_tree::timeout_call_with_sender(mc::milliseconds timeout, std::string_view sender,
                                                        const method_call_params& params) {
    local_msg msg(params.service_name, params.path, params.interface, params.method);
    if (!params.signature.empty()) {
        msg.append_args(params.signature, params.args);
    }
    msg.set_local_call(false);
    auto msg_queue = harbor::get_destination_msg_queue(params.service_name);
    if (msg_queue == nullptr) {
        dlog("failed to get message queue of destination service: ${service_name}",
             ("service_name", params.service_name));
        return std::nullopt;
    }
    msg.set_sender(sender);
    uint32_t serial = set_serial(msg);
#if defined(BUILD_TYPE) && defined(BUILD_TYPE_DEBUG) && BUILD_TYPE == BUILD_TYPE_DEBUG
    msg.set_timestamp();
#endif
    auto data = msg.pack();
    try {
        if (!msg_queue->push_back(data, MSG_QUEUE_PUSH_TIMEOUT)) {
            wlog("failed to push to message queue of destination service: ${service_name}",
                 ("service_name", params.service_name));
            return std::nullopt;
        }
    } catch (const std::exception& e) {
        elog("message queue rpc failed: ${error}", ("error", e.what()));
        return std::nullopt;
    }
    auto  promise = mc::make_promise<local_msg>();
    auto  future  = promise.get_future();
    auto& harbor  = mc::dbus::harbor::get_instance();
    if (!harbor.send_shm_msg(sender, serial, promise)) {
        wlog("failed to save shm msg promise");
        return std::nullopt;
    }
    if (future.wait_for(timeout) == mc::futures::future_status::timeout) {
        harbor.remove_shm_msg(sender, serial);
        throw mc::exception(mc::method_call_exception_code, std::string(error_names::no_reply),
                            std::string(error_messages::no_reply));
    }
    auto reply_msg = future.get();
    if (reply_msg.msg_type() == DBUS_MESSAGE_TYPE_ERROR) {
        auto [error_name, error_message] = reply_msg.get_error();
        throw mc::exception(mc::method_call_exception_code, error_name, error_message);
    }
    return reply_msg.read();
}

static shm::shared_ptr<shm::property> find_shm_property(std::string_view service_name, std::string_view path,
                                                        std::string_view interface, std::string_view property) {
    auto& ins  = shm::shared_memory::get_instance();
    auto  tree = ins.get_tree(service_name);
    if (tree == nullptr) {
        MC_THROW(mc::exception, "failed to get tree, service_name: ${service_name}", ("service_name", service_name));
    }
    auto obj = tree->find_object(path);
    if (obj == nullptr) {
        MC_THROW(mc::exception, "failed to find object, path: ${path}", ("path", path));
    }
    auto it = obj->interfaces().find(interface);
    if (it == obj->interfaces().end()) {
        MC_THROW(mc::exception, "failed to find interface: ${interface}", ("interface", interface));
    }
    auto intf = &*it->second;
    auto prop = intf->find_p(property);
    if (prop == nullptr) {
        MC_THROW(mc::exception, "failed to find property: ${property}", ("property", property));
    }
    return prop;
}

template <typename T, typename = void>
struct has_static_allocate_object_id : std::false_type {};

template <typename T>
struct has_static_allocate_object_id<T, std::void_t<decltype(T::allocate_object_id(std::declval<std::string_view>()))>> : std::true_type {};

template <typename LibraryType>
static uint32_t get_object_id(std::string_view path) {
    if constexpr (has_static_allocate_object_id<LibraryType>::value) {
        return LibraryType::allocate_object_id(path);
    }

    return g_str_hash(path.data());
}

static std::optional<variant> get_property_inner(std::string_view service_name, std::string_view path,
                                                 std::string_view interface, std::string_view property) {
    auto prop = find_shm_property(service_name, path, interface, property);
    return shm_object_lock_shared_exec(get_object_id<shmlock::ShmLockManager>(path), [&]() {
        auto data = prop->get_data();
        if (data == std::nullopt) {
            return std::optional<variant>{};
        }
        std::string_view prop_value = data.value();
        size_t           p_data_len = prop_value.size();
        void*            p_data     = g_malloc0(p_data_len);
        if (p_data == nullptr) {
            MC_THROW(mc::exception, "g_malloc0 failed, len: ${len}", ("len", p_data_len));
        }
        std::memcpy(p_data, prop_value.data(), p_data_len);
        std::string_view   signature = prop->get_signature();
        gvariant_auto_free v(
            g_variant_new_from_data(G_VARIANT_TYPE(signature.data()), p_data, p_data_len, false, g_free, p_data));
        return std::make_optional(gvariant_convert::to_mc_variant(v.ptr));
    });
}

variant shm_tree::get_property(std::string_view service_name, std::string_view path, std::string_view interface,
                               std::string_view property) {
    auto res = shm_global_lock_shared_exec([&]() {
        return get_property_inner(service_name, path, interface, property);
    });
    if (res == std::nullopt) {
        MC_THROW(mc::exception, "failed to get value of property: ${property}", ("property", property));
    }
    return res.value();
}

void shm_tree::set_property_inner(shm::shared_ptr<shm::property> prop, const variant& value) {
    auto& ins = shm::shared_memory::get_instance();
    if (value.is_null()) {
        prop->set_data(ins, std::string_view());
        return;
    }
    std::string_view   signature = prop->get_signature();
    gvariant_auto_free v(gvariant_convert::to_gvariant(value, signature.data()));
    auto               data = g_variant_get_data(v.ptr);
    auto               size = g_variant_get_size(v.ptr);
    prop->set_data(ins, std::string_view(static_cast<const char*>(data), size));
}

void shm_tree::set_property(std::string_view service_name, std::string_view path, std::string_view interface,
                            std::string_view property, const variant& value) {
    shm_global_lock_shared_exec([&]() {
        auto prop = find_shm_property(service_name, path, interface, property);
        shm_object_lock_exec(get_object_id<shmlock::ShmLockManager>(path), [&]() {
            set_property_inner(prop, value);
        });
    });
}

std::optional<variant> shm_tree::call_shm_get_property(std::string_view service_name,
                                                       std::string_view path,
                                                       std::string_view interface_name,
                                                       std::string_view property_name) {
    // 1) 共享内存扫描阶段：在全局共享锁内尝试读取属性值
    auto shm_value = shm_global_lock_shared_exec([&]() {
        return get_property_inner(service_name, path, interface_name, property_name);
    });

    // 2) 如果共享内存中命中，直接返回
    if (shm_value.has_value()) {
        return shm_value;
    }

    // 3) RPC 回退阶段：释放全局锁后，通过 RPC 查询属性值
    // 说明：harbor::get_destination_msg_queue 内部会获取全局共享锁，必须避免锁重入。
    return get_property_via_rpc(interface_name, property_name, service_name, path);
}

// 查找接口的辅助函数（不抛异常，返回 optional）
static shm::interface* find_shm_interface(shm::shared_memory& ins, std::string_view service_name,
                                          std::string_view path, std::string_view interface_name) {
    auto tree = ins.get_tree(service_name);
    if (tree == nullptr) {
        return nullptr;
    }

    auto obj = tree->find_object(path);
    if (obj == nullptr) {
        return nullptr;
    }

    auto it = obj->interfaces().find(interface_name);
    if (it == obj->interfaces().end() || !it->second) {
        return nullptr;
    }
    return &*it->second;
}

// 统一的属性扫描结果结构体
struct properties_scan_out {
    dict                     result;            // 已落盘的属性值
    std::vector<std::string> need_rpc_props;    // 需要 RPC 查询的属性名
    bool                     has_error = false; // 是否出现错误（空指针、属性不存在等）
};

// 扫描所有属性的共享内存阶段
static properties_scan_out get_all_shm_properties(shm::shared_memory& ins,
                                                  std::string_view    service_name,
                                                  std::string_view    path,
                                                  std::string_view    interface_name) {
    properties_scan_out out;
    out.has_error = false; // 显式初始化
    auto* intf    = find_shm_interface(ins, service_name, path, interface_name);
    if (intf == nullptr) {
        out.has_error = true;
        return out;
    }

    // 在对象共享锁内读取属性数据（避免每个属性单独加锁）
    shm_object_lock_shared_exec(get_object_id<shmlock::ShmLockManager>(path), [&]() {
        for (const auto& [prop_name, prop_ptr] : intf->properties()) {
            if (!prop_ptr) {
                out.has_error = true;
                return;
            }
            auto value_opt = read_property_variant(*prop_ptr);
            if (!value_opt.has_value()) {
                // 共享内存中存在属性但未落盘 value，标记为需要 RPC
                out.need_rpc_props.push_back(std::string(prop_name));
            } else {
                out.result[std::string_view(prop_name)] = value_opt.value();
            }
        }
    });
    return out;
}

// RPC 回退：对未落盘的属性逐个通过 RPC 查询并填充到结果中
// 说明：harbor::get_destination_msg_queue 内部会获取全局共享锁，必须避免锁重入。
// @param errors 如果非空，则记录 RPC 调用失败的错误信息（仅当 timeout_call_with_sender 抛出异常时）
static void fill_properties_via_rpc(dict& result, const std::vector<std::string>& need_rpc_props,
                                    std::string_view interface_name, std::string_view service_name,
                                    std::string_view path, dict* errors = nullptr) {
    if (errors != nullptr) {
        // 需要记录错误信息，捕获异常
        for (const auto& prop_name : need_rpc_props) {
            auto [value_opt, error_variant] =
                get_property_via_rpc_with_error(interface_name, prop_name, service_name, path);
            if (value_opt.has_value()) {
                result[prop_name] = value_opt.value();
            } else if (error_variant.has_value()) {
                (*errors)[prop_name] = error_variant.value();
            }
        }
    } else {
        // 不需要错误信息，允许异常抛出
        for (const auto& prop_name : need_rpc_props) {
            auto value_opt = get_property_via_rpc(interface_name, prop_name, service_name, path);
            if (value_opt.has_value()) {
                result[prop_name] = value_opt.value();
            }
        }
    }
}

std::optional<dict> shm_tree::call_shm_get_all_properties(std::string_view service_name,
                                                          std::string_view path,
                                                          std::string_view interface_name) {
    // 1) 共享内存扫描阶段：在全局共享锁内收集已落盘的属性值，标记未落盘的属性名
    auto shm_out = shm_global_lock_shared_exec([&]() {
        auto& ins = shm::shared_memory::get_instance();
        return get_all_shm_properties(ins, service_name, path, interface_name);
    });

    // 如果出现错误，视为 SHM 不完整，返回 nullopt（上层可回退到 RPC）
    if (shm_out.has_error) {
        return std::nullopt;
    }

    // 2) RPC 回退阶段：释放全局锁后，对未落盘的属性逐个通过 RPC 查询
    fill_properties_via_rpc(shm_out.result, shm_out.need_rpc_props, interface_name, service_name, path);

    return shm_out.result;
}

// 扫描指定属性名的共享内存阶段
static properties_scan_out get_shm_properties_by_names(shm::shared_memory& ins,
                                                       std::string_view    service_name,
                                                       std::string_view    path,
                                                       std::string_view    interface_name,
                                                       const variants&     prop_names) {
    properties_scan_out out;
    out.has_error = false; // 显式初始化
    auto* intf    = find_shm_interface(ins, service_name, path, interface_name);
    if (intf == nullptr) {
        out.has_error = true;
        return out;
    }

    // 在对象共享锁内读取属性数据（避免每个属性单独加锁）
    shm_object_lock_shared_exec(get_object_id<shmlock::ShmLockManager>(path), [&]() {
        for (const auto& name_v : prop_names) {
            if (!name_v.is_string()) {
                out.has_error = true;
                return;
            }
            std::string_view prop_name = name_v.as_string();

            auto prop_ptr = intf->find_p(prop_name);
            if (!prop_ptr) {
                // 属性不存在，标记错误（与原有行为一致：属性不存在时返回 nullopt）
                out.has_error = true;
                return;
            }

            auto value_opt = read_property_variant(*prop_ptr);
            if (!value_opt.has_value()) {
                // 共享内存中存在属性但未落盘 value，标记为需要 RPC
                out.need_rpc_props.push_back(std::string(prop_name));
            } else {
                out.result[std::string_view(prop_name)] = value_opt.value();
            }
        }
    });
    return out;
}

std::optional<variants> shm_tree::call_shm_get_properties_by_names(
    std::string_view service_name, std::string_view path, std::string_view interface_name,
    const variants& prop_names) {
    // 1) 共享内存扫描阶段：在全局共享锁内收集已落盘的属性值，标记未落盘的属性名
    auto shm_out = shm_global_lock_shared_exec([&]() {
        auto& ins = shm::shared_memory::get_instance();
        return get_shm_properties_by_names(ins, service_name, path, interface_name, prop_names);
    });

    // 如果出现错误（属性不存在、空指针等），返回 nullopt（与原有行为一致）
    if (shm_out.has_error) {
        return std::nullopt;
    }

    // 2) RPC 回退阶段：释放全局锁后，对未落盘的属性逐个通过 RPC 查询，记录成功结果和错误信息
    dict errors;
    fill_properties_via_rpc(shm_out.result, shm_out.need_rpc_props, interface_name, service_name,
                            path, &errors);

    // 返回数组：第一个是成功获取的属性字典，第二个是错误信息字典
    variants result;
    result.push_back(variant(shm_out.result));
    result.push_back(variant(errors));
    return result;
}

void shm_tree::add_shm_match(match_rule& rule, std::string_view harbor_name, uint64_t id) {
    shm_global_lock_exec([this, &rule, harbor_name, id]() {
        auto& ins      = shm::shared_memory::get_instance();
        auto& tree_map = ins.get_object_tree_map(harbor_name);
        auto  it       = tree_map.find(harbor_name);
        if (it == tree_map.end()) {
            return;
        }
        auto shm_slot = ins.get_base()._matchs.add_rule(ins, *rule.rule(), &*it->second);
        m_shm_slots.emplace(id, shm_slot);
    });
}

void shm_tree::add_match(match_rule& rule, mc::dbus::match_cb_t&& cb, uint64_t id) {
    auto& harbor      = harbor::get_instance();
    auto  harbor_name = harbor.get_harbor_name();
    harbor.add_match(rule, std::forward<mc::dbus::match_cb_t>(cb), id);
    add_shm_match(rule, harbor_name, id);
}

void shm_tree::remove_match(uint64_t id) {
    auto it = m_shm_slots.find(id);
    if (it == m_shm_slots.end()) {
        return;
    }
    auto slot = it->second;
    m_shm_slots.erase(it);
    shm_global_lock_exec([slot]() {
        slot();
    });
}
} // namespace mc::dbus