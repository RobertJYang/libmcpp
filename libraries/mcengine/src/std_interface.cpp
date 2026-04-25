/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 */

#include <mc/engine/errors/std_errors.h>
#include <mc/engine/metadata.h>
#include <mc/engine/property.h>
#include <mc/engine/std_interface.h>
#include <mc/reflect/signature.h>
#include <mc/string.h>

namespace mc::engine {

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
        xml_data += "\" access=\"readwrite\" />";
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
            xml_data += "<node name=\"";
            xml_data.append(rel);
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
    // 标准 dbus-daemon 会返回 /etc/machine-id，mcapp 这里先返回空字符串（业务侧
    // 对该方法无依赖）。TODO: 如需要可读取 /etc/machine-id 或 boot-id 并缓存。
    return {};
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

std::optional<standard_interfaces::invoke_hit> standard_interfaces::try_invoke(abstract_object& object,
                                                                               mc::string_view method_name,
                                                                               const mc::variants& args,
                                                                               mc::string_view interface_name)
{
    // 快速过滤：标准 DBus 接口必然以 "org.freedesktop.DBus." 开头
    if (!mc::string::starts_with(interface_name, common_prefix)) {
        return std::nullopt;
    }

    mc::string_view suffix =
        mc::string_view(interface_name.data() + common_prefix.size(), interface_name.size() - common_prefix.size());

    abstract_interface* target = nullptr;
    if (suffix == properties_name) {
        target = &properties_interface::get_instance();
    } else if (suffix == introspectable_name) {
        target = &introspectable_interface::get_instance();
    } else if (suffix == peer_name) {
        target = &peer_interface::get_instance();
    } else if (suffix == object_manager_name) {
        target = &object_manager_interface::get_instance();
    } else {
        return std::nullopt;
    }

    // 命中已知标准接口，但方法不存在时返回 unknown_method 错误
    auto* method = target->get_method_info(method_name);
    if (method == nullptr) {
        MC_REPLY_ERROR_AND_THROW(errors::unknown_method, ("method", method_name));
    }

    // 压入调用栈：让 Peer/Properties/Introspectable/ObjectManager 通过
    // object_call_stack::top_value() 拿到当前对象
    scoped_object_context ctx(object.get_service(), object);
    return invoke_hit{target->invoke(method_name, args), method->get_result_signature()};
}

} // namespace mc::engine

MC_REFLECT(mc::engine::properties_interface, ((get, "Get"))((get_all, "GetAll"))((set, "Set"))(properties_changed))

MC_REFLECT(mc::engine::introspectable_interface, ((introspect, "Introspect")))

MC_REFLECT(mc::engine::peer_interface, ((ping, "Ping"))((get_machine_id, "GetMachineId")))

MC_REFLECT(mc::engine::object_manager_interface,
           ((get_managed_objects, "GetManagedObjects"))(interfaces_added)(interfaces_removed))
