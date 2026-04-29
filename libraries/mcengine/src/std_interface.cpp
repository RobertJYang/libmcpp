/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 */

#include <mc/engine/context.h>
#include <mc/engine/errors/std_errors.h>
#include <mc/engine/metadata.h>
#include <mc/engine/property.h>
#include <mc/engine/service.h>
#include <mc/engine/std_interface.h>
#include <mc/filesystem.h>
#include <mc/im/key_buffer.h>
#include <mc/quark.h>
#include <mc/reflect/signature.h>
#include <mc/string.h>

#include <set>

namespace mc::engine {

namespace std_ifaces {

const mc::string properties          = mc::string::from_quark("org.freedesktop.DBus.Properties"_q);
const mc::string introspectable      = mc::string::from_quark("org.freedesktop.DBus.Introspectable"_q);
const mc::string peer                = mc::string::from_quark("org.freedesktop.DBus.Peer"_q);
const mc::string object_manager      = mc::string::from_quark("org.freedesktop.DBus.ObjectManager"_q);
const mc::string get                 = mc::string::from_quark("Get"_q);
const mc::string get_all             = mc::string::from_quark("GetAll"_q);
const mc::string set                 = mc::string::from_quark("Set"_q);
const mc::string ping                = mc::string::from_quark("Ping"_q);
const mc::string get_machine_id      = mc::string::from_quark("GetMachineId"_q);
const mc::string introspect          = mc::string::from_quark("Introspect"_q);
const mc::string get_managed_objects = mc::string::from_quark("GetManagedObjects"_q);

} // namespace std_ifaces

namespace {

// try_invoke 内压栈，供标准接口通过 object_call_stack 取当前对象（不经 obj.invoke）。
class scoped_object_context {
public:
    scoped_object_context(service* svc, abstract_object& obj) : m_ctx(svc, obj)
    {}

private:
    object_call_stack::context m_ctx;
};

bool object_has_interface(abstract_object& object, mc::string_view interface_name)
{
    if (standard_interfaces::is_standard_interface(interface_name)) {
        return true;
    }
    return object.has_interface(interface_name);
}

std::optional<standard_interfaces::invoke_hit> invoke_standard_interface(abstract_object&    object,
                                                                         abstract_interface& target,
                                                                         mc::string_view     method_name,
                                                                         const mc::variants& args)
{
    auto* method = target.get_method_info(method_name);
    if (method == nullptr) {
        MC_REPLY_ERROR_AND_THROW(errors::unknown_method, ("method", method_name));
    }

    scoped_object_context obj_ctx(object.get_service(), object);

    // 标准接口错误需要当前调用上下文。
    context               call_ctx(*object.get_service(), object);
    detail::variants_call call_info{args, target.get_interface_name(), method_name};
    call_info.path = object.get_object_path();
    call_ctx.set_call_info(call_info);
    context_stack::context ctx_guard(object.get_service(), call_ctx);

    return standard_interfaces::invoke_hit{target.invoke(method_name, args), method->get_result_signature()};
}

} // namespace

bool standard_interfaces::is_standard_interface(mc::string_view interface_name) noexcept
{
    return interface_name == properties_interface_name || interface_name == introspectable_interface_name ||
           interface_name == peer_interface_name || interface_name == object_manager_interface_name;
}

//===--------------------------------------------------------------------------------===//
// org.freedesktop.DBus.Properties
//===--------------------------------------------------------------------------------===//

properties_interface& properties_interface::get_instance()
{
    static properties_interface instance;
    return instance;
}

mc::variant properties_interface::get(mc::string_view interface_name, mc::string_view property_name)
{
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }
    if (!object_has_interface(*object, interface_name)) {
        MC_REPLY_ERROR_AND_THROW(errors::unknown_interface, ("interface", interface_name));
    }
    if (!object->has_property(property_name, interface_name)) {
        MC_REPLY_ERROR_AND_THROW(errors::unknown_property, ("property", property_name));
    }
    return object->get_property(property_name, interface_name, mc::engine::property_options::from_mdb);
}

mc::dict properties_interface::get_all(mc::string_view interface_name)
{
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }
    if (!object_has_interface(*object, interface_name)) {
        MC_REPLY_ERROR_AND_THROW(errors::unknown_interface, ("interface", interface_name));
    }
    return object->get_all_properties(interface_name, mc::engine::property_options::from_mdb);
}

void properties_interface::set(mc::string_view interface_name, mc::string_view property_name, const mc::variant& value)
{
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return;
    }
    if (!object_has_interface(*object, interface_name)) {
        MC_REPLY_ERROR_AND_THROW(errors::unknown_interface, ("interface", interface_name));
    }
    if (!object->has_property(property_name, interface_name)) {
        MC_REPLY_ERROR_AND_THROW(errors::unknown_property, ("property", property_name));
    }
    auto info = object->get_metadata().get_property_info(property_name, interface_name);
    if (info.item != nullptr && !info.item->is_writable()) {
        MC_REPLY_ERROR_AND_THROW(errors::property_read_only, ("property", property_name));
    }
    object->set_property(property_name, value, interface_name);
}

//===--------------------------------------------------------------------------------===//
// org.freedesktop.DBus.Introspectable
//===--------------------------------------------------------------------------------===//

namespace {

struct introspect_visitor : metadata_visitor {
    void handle_interface_begin(const interface_metadata& iface) override
    {
        mc::string_view name = iface.metadata != nullptr ? iface.metadata->get_class_name() : mc::string_view{};
        xml_data += "<interface name=\"";
        xml_data.append(name);
        xml_data += "\">";
    }

    void handle_interface_end(const interface_metadata& iface) override
    {
        (void)iface;
        xml_data += "</interface>";
    }

    void handle(const property_type_info* info) override
    {
        xml_data += "<property name=\"";
        xml_data.append(info->name);
        xml_data += "\" type=\"";
        xml_data.append(info->get_signature());
        xml_data += "\" access=\"";
        xml_data += info->is_writable() ? "readwrite" : "read";
        xml_data += "\" />";
    }

    void handle(const method_type_info* info) override
    {
        xml_data += "<method name=\"";
        xml_data.append(info->name);
        xml_data += "\">";

        auto                            args_sig = info->get_args_signature();
        mc::reflect::signature_iterator it(args_sig);
        auto                            args_it = it.get_content_iterator();
        while (!args_it.at_end()) {
            xml_data += "<arg type=\"";
            xml_data.append(args_it.current_type());
            xml_data += "\" direction=\"in\" />";
            args_it.next();
        }

        auto result_sig = info->get_result_signature();
        if (!result_sig.empty()) {
            xml_data += "<arg type=\"";
            xml_data.append(result_sig);
            xml_data += "\" direction=\"out\" />";
        }

        xml_data += "</method>";
    }

    void handle(const signal_type_info* info) override
    {
        xml_data += "<signal name=\"";
        xml_data.append(info->name);
        xml_data += "\">";

        auto                            args_sig = info->get_args_signature();
        mc::reflect::signature_iterator it(args_sig);
        auto                            args_it = it.get_content_iterator();
        while (!args_it.at_end()) {
            xml_data += "<arg type=\"";
            xml_data.append(args_it.current_type());
            xml_data += "\" />";
            args_it.next();
        }

        xml_data += "</signal>";
    }

    void handle_children(abstract_object& obj)
    {
        auto& objs      = obj.get_managed_objects();
        auto  self_path = obj.get_object_path();

        for (auto& [key, child] : objs) {
            (void)key;
            auto            child_path = child->get_object_path();
            mc::string_view rel        = child_path;
            if (mc::string::starts_with(child_path, self_path) && child_path.size() > self_path.size() &&
                child_path[self_path.size()] == '/') {
                rel =
                    mc::string_view(child_path.data() + self_path.size() + 1, child_path.size() - self_path.size() - 1);
            }
            auto            slash_pos     = rel.find('/');
            mc::string_view first_segment = (slash_pos != mc::string_view::npos) ? rel.substr(0, slash_pos) : rel;
            xml_data += "<node name=\"";
            xml_data.append(first_segment);
            xml_data += "\"></node>";
        }
    }

    template <typename Interface>
    void add_standard_interface(const Interface& ins)
    {
        const auto& mt = ins.get_metadata();
        handle_interface_begin({nullptr, &mt});
        mt.visit(*this);
        handle_interface_end({nullptr, &mt});
    }

    mc::string xml_data;
};

} // namespace

introspectable_interface& introspectable_interface::get_instance()
{
    static introspectable_interface instance;
    return instance;
}

mc::string introspectable_interface::introspect()
{
    auto* object = object_call_stack::top_value();
    // 无对象上下文时由 try_invoke(svc, nullptr, ...) 经 introspect_intermediate_node 处理中间路径，
    // 此处不向客户端暴露「部分内容」。
    if (object == nullptr) {
        return {};
    }

    introspect_visitor v;
    v.xml_data = "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" "
                 "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\"><node>";
    // 先列对象自身的接口
    object->get_metadata().visit(v);
    // 再追加 4 个标准接口
    v.add_standard_interface(*this);
    v.add_standard_interface(properties_interface::get_instance());
    v.add_standard_interface(peer_interface::get_instance());
    v.add_standard_interface(object_manager_interface::get_instance());
    // 列出子对象节点
    v.handle_children(*object);
    v.xml_data += "</node>";
    return v.xml_data;
}

namespace {

// 收集对象表中所有以 path/ 为前缀的对象、其相对 path 的下一段 segment 集合（去重）。
// 实现：在 by_path 唯一索引（radix）上从 lower_bound(path/) 开始顺序遍历，
// 直到首个不再匹配 path/ 前缀的项；O(log n + k) 量级，k 为命中数。
std::set<mc::string> collect_child_segments(service_object_table& table, mc::string_view path)
{
    std::set<mc::string> segments;
    mc::string           prefix = path == "/" ? mc::string("/") : mc::string(path) + "/";
    table.with_index<by_path>([&](auto& idx) {
        for (auto it = idx.lower_bound(prefix); !it.is_end(); ++it) {
            mc::string_view k = it.key();
            if (!mc::im::has_prefix(k, mc::string_view(prefix))) {
                break;
            }
            mc::string_view rest      = k.substr(prefix.size());
            auto            slash_pos = rest.find('/');
            mc::string_view seg       = (slash_pos == mc::string_view::npos) ? rest : rest.substr(0, slash_pos);
            if (!seg.empty()) {
                segments.emplace(seg);
            }
        }
    });
    return segments;
}

} // namespace

mc::string introspectable_interface::introspect_intermediate_node(const service& svc, mc::string_view path)
{
    auto& table    = const_cast<service&>(svc).get_object_table();
    auto  segments = collect_child_segments(table, path);
    if (segments.empty()) {
        return {};
    }

    mc::string xml;
    xml += "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" "
           "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n";
    xml += "<node>\n";
    for (const auto& seg : segments) {
        xml += "  <node name=\"";
        xml.append(seg);
        xml += "\"/>\n";
    }
    xml += "</node>\n";
    return xml;
}

//===--------------------------------------------------------------------------------===//
// org.freedesktop.DBus.Peer
//===--------------------------------------------------------------------------------===//

peer_interface& peer_interface::get_instance()
{
    static peer_interface instance;
    return instance;
}

void peer_interface::ping()
{}

mc::string peer_interface::get_machine_id()
{
    // 读取 /etc/machine-id（标准 systemd/dbus 约定），结果缓存。
    static mc::string s_machine_id;
    static bool       s_initialized = false;
    if (!s_initialized) {
        s_initialized = true;
        if (auto content = mc::filesystem::read_file("/etc/machine-id")) {
            // 去除尾部换行符
            auto sv = content->view();
            while (!sv.empty() && (sv.back() == '\n' || sv.back() == '\r')) {
                sv = sv.substr(0, sv.size() - 1);
            }
            s_machine_id = mc::string(sv);
        }
    }
    return s_machine_id;
}

//===--------------------------------------------------------------------------------===//
// org.freedesktop.DBus.ObjectManager
//===--------------------------------------------------------------------------------===//

object_manager_interface& object_manager_interface::get_instance()
{
    static object_manager_interface instance;
    return instance;
}

namespace {

struct object_manager_visitor : metadata_visitor {
    object_manager_visitor(abstract_object* object) : m_object(object)
    {}

    void handle_interface_begin(const interface_metadata& iface) override
    {
        m_current            = {};
        m_interface_metadata = &iface;
    }

    void handle_interface_end(const interface_metadata& iface) override
    {
        auto name = iface.metadata != nullptr ? iface.metadata->get_class_name() : mc::string_view{};
        m_interfaces[mc::string(name)] = m_current;
    }

    void handle(const property_type_info* info) override
    {
        if (m_interface_metadata == nullptr || m_interface_metadata->interface == nullptr) {
            return;
        }
        const auto* iface = to_interface_ptr(m_object, m_interface_metadata->interface);
        if (iface == nullptr) {
            return;
        }
        mc::variant value                 = iface->get_property(info->name, mc::engine::property_options::memory);
        m_current[mc::string(info->name)] = value;
    }

    abstract_object*                          m_object;
    const interface_metadata*                 m_interface_metadata{nullptr};
    mc::dict                                  m_current;
    object_manager_interface::interfaces_type m_interfaces;
};

} // namespace

object_manager_interface::objects_type object_manager_interface::get_managed_objects()
{
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }

    objects_type result;
    for (auto& [obj_path, child] : object->get_managed_objects()) {
        (void)obj_path;
        if (child == nullptr) {
            continue;
        }
        object_manager_visitor v(child);
        child->get_metadata().visit(v);
        mc::engine::path key{mc::string(child->get_object_path())};
        result[key] = v.m_interfaces;
    }
    return result;
}

//===--------------------------------------------------------------------------------===//
// standard_interfaces::try_invoke
//===--------------------------------------------------------------------------------===//

std::optional<standard_interfaces::invoke_hit>
standard_interfaces::try_invoke(const service& svc, abstract_object* object, mc::string_view path,
                                mc::string_view method_name, const mc::variants& args, mc::string_view interface_name)
{
    // 无对象上下文：仅 Introspectable.Introspect 的中间节点语义有意义，
    // 其余标准接口一律返回 nullopt（dispatcher 据此回 unknown_object）。
    if (object == nullptr) {
        if (interface_name != introspectable_interface_name || method_name != std_ifaces::introspect) {
            return std::nullopt;
        }
        auto xml = introspectable_interface::get_instance().introspect_intermediate_node(svc, path);
        if (xml.empty()) {
            return std::nullopt;
        }
        return invoke_hit{mc::variant(std::move(xml)), "s"};
    }

    abstract_interface* target = nullptr;
    if (interface_name == std_ifaces::properties) {
        target = &properties_interface::get_instance();
    } else if (interface_name == std_ifaces::introspectable) {
        target = &introspectable_interface::get_instance();
    } else if (interface_name == std_ifaces::peer) {
        target = &peer_interface::get_instance();
    } else if (interface_name == std_ifaces::object_manager) {
        target = &object_manager_interface::get_instance();
    } else {
        return std::nullopt;
    }
    (void)path;

    return invoke_standard_interface(*object, *target, method_name, args);
}

} // namespace mc::engine

MC_REFLECT(mc::engine::properties_interface, ((get, "Get"))((get_all, "GetAll"))((set, "Set"))(properties_changed))
MC_REFLECT(mc::engine::introspectable_interface, ((introspect, "Introspect")))
MC_REFLECT(mc::engine::peer_interface, ((ping, "Ping"))((get_machine_id, "GetMachineId")))
MC_REFLECT(mc::engine::object_manager_interface,
           ((get_managed_objects, "GetManagedObjects"))(interfaces_added)(interfaces_removed))
