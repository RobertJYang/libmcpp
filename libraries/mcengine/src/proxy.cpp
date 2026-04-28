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

#include <mc/engine/proxy.h>

#include <string_view>

#include <mc/engine/dispatcher.h>
#include <mc/engine/engine.h>
#include <mc/engine/service.h>
#include <mc/engine/std_interface.h>
#include <mc/exception.h>

#include "shm_object_ops.h"
#include "shm_property_sync.h"
#include "shm_service.h"
#include "shm_service_ops.h"

namespace mc::engine {
namespace {

[[noreturn]] void throw_proxy_error(mc::string_view message)
{
    MC_THROW(mc::method_call_exception, "${message}", ("message", message));
}

mc::variant slot_to_variant(const property_slot& slot)
{
    switch (slot.type) {
        case property_type_tag::int64:
            return slot.v_int64;
        case property_type_tag::double_:
            return slot.v_double;
        case property_type_tag::string: {
            auto* blob = slot.v_blob.get();
            if (blob == nullptr) {
                return {};
            }
            auto sv = blob->as_string_view();
            return mc::string(sv.data(), sv.size());
        }
        case property_type_tag::bytes: {
            auto* blob = slot.v_blob.get();
            if (blob == nullptr) {
                return {};
            }
            auto sv = blob->as_string_view();
            return mc::blob(sv.data(), sv.size());
        }
        case property_type_tag::null:
        default:
            return {};
    }
}

bool service_epoch_matches(const resolved_shm_object& resolved, const object_ref& ref,
                           const proxy_policy& policy) noexcept
{
    if (policy.allow_stale_read || !ref.service_epoch.has_value()) {
        return true;
    }
    return resolved.service_epoch.has_value() && *resolved.service_epoch == *ref.service_epoch;
}

// SHM 属性读取结果：miss=slot 不存在，nocache=slot 存在但不可缓存，hit=有值
enum class shm_read_status { miss, nocache, hit };

struct shm_read_result {
    shm_read_status status{shm_read_status::miss};
    mc::variant     value;
};

shm_read_result try_read_shm_property(const resolved_shm_object& resolved, const object_ref& ref,
                                       const proxy_policy& policy, mc::string_view interface_name,
                                       mc::string_view property_name)
{
    auto* object = resolved.object;
    if (object == nullptr || object->properties.get() == nullptr) {
        return {};
    }
    if (!service_epoch_matches(resolved, ref, policy)) {
        return {};
    }
    if (object->abi_version != shm_object_abi_version || !shm_object_check(*object)) {
        return {};
    }

    auto  key  = shm_property_compose_key(std::string_view(interface_name.data(), interface_name.size()),
                                          std::string_view(property_name.data(), property_name.size()));
    auto* slab = object->properties.get();
    int   idx  = property_slab_find(*slab, key);
    if (idx < 0) {
        return {}; // miss
    }

    const auto& slot = slab->slots[idx];
    // SHM 中的 nocache flag 是权威的：proxy 必须走消息，忽略 proxy_route 策略
    if (slot.flags & property_slot_flags::nocache) {
        return {shm_read_status::nocache, {}};
    }

    return {shm_read_status::hit, slot_to_variant(slot)};
}

mc::dict read_shm_properties(const resolved_shm_object& resolved, const object_ref& ref, const proxy_policy& policy,
                             mc::string_view interface_name)
{
    mc::dict result;
    auto*    object = resolved.object;
    if (object == nullptr || object->properties.get() == nullptr) {
        return result;
    }
    if (!service_epoch_matches(resolved, ref, policy)) {
        return result;
    }
    if (object->abi_version != shm_object_abi_version || !shm_object_check(*object)) {
        return result;
    }

    auto* slab = object->properties.get();
    for (std::uint16_t i = 0; i < slab->slot_count; ++i) {
        // 跳过 nocache marker slot
        if (slab->slots[i].flags & property_slot_flags::nocache) {
            continue;
        }
        auto* key = slab->slots[i].key.get();
        if (key == nullptr) {
            continue;
        }
        std::string_view key_view = key->as_string_view();
        std::string_view slot_iface;
        std::string_view slot_name;
        shm_property_decompose_key(key_view, slot_iface, slot_name);
        if (slot_iface != std::string_view(interface_name.data(), interface_name.size())) {
            continue;
        }
        result[mc::string(slot_name.data(), slot_name.size())] = slot_to_variant(slab->slots[i]);
    }
    return result;
}

class default_proxy_shm_resolver : public proxy_shm_resolver {
public:
    resolved_shm_object resolve(const object_ref& ref) override
    {
        auto& table = engine::get_instance().get_object_table();
        for (auto it = table.find<by_path>(ref.path); !it.is_end() && it->get_object_path() == ref.path; ++it) {
            auto* obj = const_cast<abstract_object*>(&*it);
            if (!ref.object_id.has_value() || obj->get_object_id() == *ref.object_id) {
                if (service_matches(*obj, ref)) {
                    return resolve_from_object(*obj);
                }
            }
        }
        return {};
    }

private:
    static bool service_matches(const abstract_object& obj, const object_ref& ref)
    {
        if (ref.service.empty()) {
            return true;
        }
        // 优先看 heap 端 service*（本进程注册的对象都有）
        if (auto* svc = obj.get_service(); svc != nullptr) {
            return svc->name() == ref.service;
        }
        // 跨进程 SHM 重建出来的 abstract_object 没有 service* 指针
        // （父进程的 service 实例不属于子进程，子进程也不会反向回填）。
        // 退化到 SHM POD：从 shm_object 的 shm_service 读 service 名比较。
        auto* sh = obj.get_shm_handle();
        if (sh == nullptr) {
            return false;
        }
        auto* shm_svc = shm_object_service(*sh);
        if (shm_svc == nullptr || shm_svc->abi_version != shm_service_abi_version
            || !shm_service_check(*shm_svc)) {
            return false;
        }
        auto name_view = shm_service_name(*shm_svc);
        std::string_view ref_view(ref.service.data(), ref.service.size());
        return std::string_view(name_view.data(), name_view.size()) == ref_view;
    }

    static resolved_shm_object resolve_from_object(abstract_object& obj)
    {
        resolved_shm_object out;
        out.object = obj.get_shm_handle();
        if (out.object == nullptr) {
            return out;
        }
        auto* svc = shm_object_service(*out.object);
        if (svc != nullptr && svc->abi_version == shm_service_abi_version && shm_service_check(*svc)) {
            out.service_epoch = svc->epoch;
        }
        return out;
    }
};

// 通过 service 发送消息并等待回复
mc::variant call_via_service(const service* svc, const message& request, const proxy_policy& policy)
{
    if (svc == nullptr) {
        throw_proxy_error("proxy 未绑定 service (请通过 service::get_proxy 获取)");
    }

    auto response = svc->send_with_reply(message(request), policy.timeout);
    if (response.header.type == message_type::error) {
        if (auto* payload = response.try_as<error_payload>(); payload != nullptr) {
            MC_THROW(mc::method_call_exception, "远端调用失败: ${name}: ${message}",
                     ("name", payload->name)("message", payload->message));
        }
        MC_THROW(mc::method_call_exception, "远端调用失败: ${name}", ("name", response.header.error_name));
    }
    if (response.header.type != message_type::method_return) {
        throw_proxy_error("远端调用没有返回 method_return");
    }
    auto* payload = response.try_as<method_return_payload>();
    if (payload == nullptr) {
        throw_proxy_error("远端 method_return 缺少 payload");
    }
    return payload->value;
}

// 与 mc/app/app_proto.h 中 message_source_key 保持一致，避免 mcengine → mcapp 依赖
inline constexpr mc::string_view k_message_source_key = "mcapp.source";
inline constexpr mc::string_view k_mq_target_endpoint_id_key = "mc.mq.target.endpoint_id";
inline constexpr mc::string_view k_mq_target_instance_id_key = "mc.mq.target.instance_id";

message build_method_call(const object_ref& ref, mc::string_view interface_name, mc::string_view method_name,
                          const mc::variants& args, mc::string_view signature, const proxy_policy& policy)
{
    message request;
    request.header.type           = message_type::method_call;
    request.header.destination    = ref.service;
    request.header.path           = ref.path;
    request.header.interface_name = mc::string(interface_name);
    request.header.member_name    = mc::string(method_name);
    request.body                  = make_payload<method_call_payload>(signature, args);
    if (ref.endpoint_id.has_value() && ref.instance_id.has_value()) {
        request.header.context[k_mq_target_endpoint_id_key] = static_cast<std::uint64_t>(*ref.endpoint_id);
        request.header.context[k_mq_target_instance_id_key] = static_cast<std::uint64_t>(*ref.instance_id);
    }

    // 根据 message_route 写入 mcapp.source，让 app_proto::on_push 选 MQ/DBus 通道
    switch (policy.message_route) {
        case proxy_message_route::mq:
            request.header.context[k_message_source_key] = mc::string("mq");
            break;
        case proxy_message_route::dbus:
            request.header.context[k_message_source_key] = mc::string("dbus");
            break;
        case proxy_message_route::auto_:
            // 不写入 → app_proto 默认行为：dbus 发送 + 可选 mq 双发
            break;
    }
    return request;
}

} // namespace

mc::shared_ptr<proxy_shm_resolver> make_default_proxy_shm_resolver()
{
    return mc::make_shared<default_proxy_shm_resolver>();
}

// ---------------------------------------------------------------------------
// dynamic API: object_proxy
// ---------------------------------------------------------------------------

object_proxy::object_proxy(const service* svc, object_ref ref, proxy_policy policy,
                           mc::shared_ptr<proxy_shm_resolver> resolver)
    : m_svc(svc), m_ref(std::move(ref)), m_policy(policy), m_resolver(std::move(resolver))
{
    if (!m_resolver && m_policy.route == proxy_route::auto_) {
        m_resolver = make_default_proxy_shm_resolver();
    }
}

const object_ref& object_proxy::ref() const noexcept
{
    return m_ref;
}

const proxy_policy& object_proxy::policy() const noexcept
{
    return m_policy;
}

mc::variant object_proxy::get_property(mc::string_view interface_name, mc::string_view property_name) const
{
    if (m_resolver) {
        auto resolved = m_resolver->resolve(m_ref);
        auto result   = try_read_shm_property(resolved, m_ref, m_policy, interface_name, property_name);

        // SHM 中的 nocache flag 是权威的：忽略 proxy_route 策略，必须走消息
        if (result.status == shm_read_status::nocache) {
            return call_message(std_ifaces::properties, std_ifaces::get,
                                mc::variants{mc::string(interface_name), mc::string(property_name)}, "ss");
        }

        // 命中缓存且 auto_ 策略：直接返回
        if (result.status == shm_read_status::hit && m_policy.route == proxy_route::auto_) {
            return result.value;
        }
    }

    // 缓存未命中或 no_cache 模式：通过消息读取
    return call_message(std_ifaces::properties, std_ifaces::get,
                        mc::variants{mc::string(interface_name), mc::string(property_name)}, "ss");
}

mc::dict object_proxy::get_all_properties(mc::string_view interface_name, proxy_get_all_mode mode) const
{
    if (mode == proxy_get_all_mode::fast_available_only) {
        if (m_resolver) {
            return read_shm_properties(m_resolver->resolve(m_ref), m_ref, m_policy, interface_name);
        }
        return {};
    }

    auto value =
        call_message(std_ifaces::properties, std_ifaces::get_all, mc::variants{mc::string(interface_name)}, "s");
    if (value.is_dict()) {
        return value.as<mc::dict>();
    }
    return {};
}

void object_proxy::set_property(mc::string_view interface_name, mc::string_view property_name,
                                const mc::variant& value) const
{
    (void)call_message(std_ifaces::properties, std_ifaces::set,
                       mc::variants{mc::string(interface_name), mc::string(property_name), value}, "ssv");
}

mc::variant object_proxy::invoke(mc::string_view interface_name, mc::string_view method_name, const mc::variants& args,
                                 mc::string_view signature) const
{
    return call_message(interface_name, method_name, args, signature);
}

mc::variant object_proxy::call_message(mc::string_view interface_name, mc::string_view method_name,
                                       const mc::variants& args, mc::string_view signature) const
{
    auto request = build_method_call(m_ref, interface_name, method_name, args, signature, m_policy);
    return call_via_service(m_svc, request, m_policy);
}

message object_proxy::make_method_call(mc::string_view interface_name, mc::string_view method_name,
                                       const mc::variants& args, mc::string_view signature) const
{
    return build_method_call(m_ref, interface_name, method_name, args, signature, m_policy);
}

// ---------------------------------------------------------------------------
// interface_proxy_context
// ---------------------------------------------------------------------------

void interface_proxy_context::bind(const object_proxy_seed& seed, mc::string_view interface_name)
{
    if (!seed.is_valid()) {
        MC_THROW(mc::method_call_exception, "interface_proxy_context::bind: invalid seed (无 service)");
    }
    m_ref        = seed.ref;
    m_iface_name = mc::string(interface_name);
    m_svc        = seed.svc;
    m_resolver   = seed.resolver;
    m_policy     = seed.policy;
    m_bound      = true;
}

bool interface_proxy_context::is_bound() const noexcept
{
    return m_bound;
}

const object_ref& interface_proxy_context::ref() const noexcept
{
    return m_ref;
}

const proxy_policy& interface_proxy_context::policy() const noexcept
{
    return m_policy;
}

mc::string_view interface_proxy_context::interface_name() const noexcept
{
    return m_iface_name;
}

void interface_proxy_context::ensure_bound() const
{
    if (!m_bound) {
        MC_THROW(mc::method_call_exception, "interface_proxy_context 未绑定 (请通过 service::get_proxy 获取)");
    }
}

mc::variant interface_proxy_context::get_property(mc::string_view property_name) const
{
    ensure_bound();

    if (m_resolver) {
        auto resolved = m_resolver->resolve(m_ref);
        auto result   = try_read_shm_property(resolved, m_ref, m_policy, m_iface_name, property_name);

        // SHM 中的 nocache flag 是权威的：忽略 proxy_route 策略，必须走消息
        if (result.status == shm_read_status::nocache) {
            auto request = build_method_call(m_ref, std_ifaces::properties, std_ifaces::get,
                                             mc::variants{mc::string(m_iface_name), mc::string(property_name)}, "ss",
                                             m_policy);
            return call_via_service(m_svc, request, m_policy);
        }

        // 命中缓存且 auto_ 策略：直接返回
        if (result.status == shm_read_status::hit && m_policy.route == proxy_route::auto_) {
            return result.value;
        }
    }

    // 缓存未命中或 no_cache 模式：通过消息读取
    auto request = build_method_call(m_ref, std_ifaces::properties, std_ifaces::get,
                                     mc::variants{mc::string(m_iface_name), mc::string(property_name)}, "ss", m_policy);
    return call_via_service(m_svc, request, m_policy);
}

void interface_proxy_context::set_property(mc::string_view property_name, const mc::variant& value) const
{
    ensure_bound();

    auto request = build_method_call(m_ref, std_ifaces::properties, std_ifaces::set,
                                     mc::variants{mc::string(m_iface_name), mc::string(property_name), value}, "ssv",
                                     m_policy);
    (void)call_via_service(m_svc, request, m_policy);
}

mc::dict interface_proxy_context::get_all_properties(proxy_get_all_mode mode) const
{
    ensure_bound();

    if (mode == proxy_get_all_mode::fast_available_only) {
        if (m_resolver) {
            return read_shm_properties(m_resolver->resolve(m_ref), m_ref, m_policy, m_iface_name);
        }
        return {};
    }

    auto request = build_method_call(m_ref, std_ifaces::properties, std_ifaces::get_all,
                                     mc::variants{mc::string(m_iface_name)}, "s", m_policy);
    auto value   = call_via_service(m_svc, request, m_policy);
    if (value.is_dict()) {
        return value.as<mc::dict>();
    }
    return {};
}

mc::variant interface_proxy_context::invoke(mc::string_view method_name, const mc::variants& args,
                                            mc::string_view signature) const
{
    ensure_bound();

    auto request = build_method_call(m_ref, m_iface_name, method_name, args, signature, m_policy);
    return call_via_service(m_svc, request, m_policy);
}

// ---------------------------------------------------------------------------
// make_proxy_seed
// ---------------------------------------------------------------------------

namespace {

object_proxy_seed make_seed_from_object(abstract_object& obj, mc::string path, proxy_policy policy)
{
    object_proxy_seed seed;
    seed.policy   = policy;
    seed.resolver = make_default_proxy_shm_resolver();

    seed.ref.path = std::move(path);
    if (auto* svc = obj.get_service(); svc != nullptr) {
        seed.ref.service = mc::string(svc->name());
        // 使用 object 所属的 service 指针
        // const_cast: abstract_object::get_service() 返回 const，但 seed 存储的是 const service*
        seed.svc = svc;
    }
    seed.ref.object_id = obj.get_object_id();

    if (auto* shm = obj.get_shm_handle(); shm != nullptr) {
        if (auto* shm_svc = shm_object_service(*shm);
            shm_svc != nullptr && shm_svc->abi_version == shm_service_abi_version && shm_service_check(*shm_svc)) {
            seed.ref.service_epoch = shm_svc->epoch;
        }
    }
    return seed;
}

} // namespace

object_proxy_seed make_proxy_seed(mc::string_view path, proxy_policy policy)
{
    auto& table = engine::get_instance().get_object_table();
    auto  it    = table.find<by_path>(path);
    if (it.is_end() || it->get_object_path() != path) {
        return {};
    }
    auto* obj = const_cast<abstract_object*>(&*it);
    return make_seed_from_object(*obj, mc::string(path), policy);
}

object_proxy_seed make_proxy_seed(mc::string_view path, mc::string_view service_hint, proxy_policy policy)
{
    auto& table = engine::get_instance().get_object_table();
    for (auto it = table.find<by_path>(path); !it.is_end() && it->get_object_path() == path; ++it) {
        auto* obj = const_cast<abstract_object*>(&*it);
        auto* svc = obj->get_service();
        if (svc != nullptr && svc->name() == service_hint) {
            return make_seed_from_object(*obj, mc::string(path), policy);
        }
    }
    return {};
}

} // namespace mc::engine
