/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 */

#include <mc/engine/metadata.h>
#include <mc/engine/property.h>
#include <mc/engine/std_interface.h>

namespace mc::engine {

properties_interface& properties_interface::get_instance() {
    static properties_interface instance;
    return instance;
}

mc::variant properties_interface::get(std::string_view interface_name,
                                      std::string_view property_name) const {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }
    if (interface_name == common_properties_name) {
        return common_properties_interface::get(property_name);
    }
    return object->get_property(property_name, interface_name, mc::engine::property_options::from_mdb);
}

mc::dict properties_interface::get_all(std::string_view interface_name) const {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }
    if (interface_name == common_properties_name) {
        return common_properties_interface::get_all();
    }
    return object->get_all_properties(interface_name, mc::engine::property_options::from_mdb);
}

void properties_interface::set(std::string_view interface_name, std::string_view property_name,
                               const mc::variant& value) {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return;
    }

    // 通用属性接口不支持修改属性
    if (interface_name == common_properties_name) {
        return;
    }
    object->set_property(property_name, value, interface_name);
}

struct inintrospect_vistor : metadata_visitor {
    void handle_interface_begin(const interface_metadata& iface) override {
        xml_data += "<interface name=\"";
        xml_data += iface.metadata->get_class_name();
        xml_data += "\">";
    }

    void handle_interface_end(const interface_metadata& iface) override {
        xml_data += "</interface>";
    }

    /*
        <property name="Property" type="i" access="readwrite" />
    */
    void handle(const property_type_info* info) override {
        xml_data += "<property name=\"";
        xml_data += info->name;
        xml_data += "\" type=\"";
        xml_data += info->get_signature();
        xml_data += "\" access=\"readwrite\" />";
    }

    /*
        <method name="Method">
          <arg type="i" direction="in"/>
          <arg type="s" direction="in"/>
          <arg type="s" direction="out"/>
        </method>
    */
    void handle(const method_type_info* info) override {
        xml_data += "<method name=\"";
        xml_data += info->name;
        xml_data += "\">";

        auto it      = mc::dbus::signature_iterator(info->get_args_signature());
        auto args_it = it.get_content_iterator();
        for (size_t index = 0; !args_it.at_end(); args_it.next(), ++index) {
            xml_data += "<arg type=\"";
            xml_data += args_it.current_type();
            xml_data += "\" direction=\"in\" />";
        }

        auto return_signature = info->get_result_signature();
        if (!return_signature.empty()) {
            xml_data += "<arg type=\"";
            xml_data += return_signature;
            xml_data += "\" direction=\"out\" />";
        }

        xml_data += "</method>";
    }

    /*
        <signal name="ValueChanged">
            <arg type="i" />
            <arg type="i" />
        </signal>
    */
    void handle(const signal_type_info* info) override {
        xml_data += "<signal name=\"";
        xml_data += info->name;
        xml_data += "\">";

        auto it      = mc::dbus::signature_iterator(info->get_args_signature());
        auto args_it = it.get_content_iterator();
        for (size_t index = 0; !args_it.at_end(); args_it.next(), ++index) {
            xml_data += "<arg type=\"";
            xml_data += args_it.current_type();
            xml_data += "\" />";
        }

        xml_data += "</signal>";
    }

    void handle_children(abstract_object& obj) {
        auto& objs = obj.get_managed_objects();
        auto  path = obj.get_object_path();

        for (auto& obj : objs) {
            auto        obj_path = obj.second->get_object_path();
            std::string obj_name;
            if (mc::string::starts_with(obj_path, path)) {
                // 如果是子路径，跳过前缀只取后半部分
                obj_name = obj_path.substr(path.size() + 1);
            }

            xml_data += "<node name=\"";
            xml_data += obj_name;
            xml_data += "\"/>";
        }
    }

    template <typename Interface>
    void add_standard_interfaces(const Interface& ins) {
        const auto& mt = ins.get_metadata();
        handle_interface_begin({nullptr, &mt});
        mt.visit(*this);
        handle_interface_end({nullptr, &mt});
    }

    std::string xml_data;
};

introspectable_interface& introspectable_interface::get_instance() {
    static introspectable_interface instance;
    return instance;
}

std::string introspectable_interface::introspect() const {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }

    inintrospect_vistor v;
    v.xml_data = "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" "
                 "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\"><node>";
    // 添加通用接口在最前面
    object->get_metadata().visit(v);
    v.add_standard_interfaces(*this);
    v.add_standard_interfaces(properties_interface::get_instance());
    v.add_standard_interfaces(peer_interface::get_instance());
    v.add_standard_interfaces(object_manager_interface::get_instance());
    v.add_standard_interfaces(common_properties_interface::get_instance());

    v.handle_children(*object);
    v.xml_data += "</node>";
    return v.xml_data;
}

peer_interface& peer_interface::get_instance() {
    static peer_interface instance;
    return instance;
}

void peer_interface::ping() const {
}

std::string peer_interface::get_machine_id() const {
    return {};
}

object_manager_interface& object_manager_interface::get_instance() {
    static object_manager_interface instance;
    return instance;
}
struct object_manager_vistor : metadata_visitor {
    object_manager_vistor(abstract_object* object) : m_object(object) {
    }

    void handle_interface_begin(const interface_metadata& iface) override {
        m_current            = {};
        m_interface_metadata = &iface;
    }

    void handle_interface_end(const interface_metadata& iface) override {
        m_interfaces[iface.metadata->get_class_name()] = m_current;
    }

    void handle(const property_type_info* info) override {
        if (m_interface_metadata->metadata->get_class_name() == common_properties_name) {
            mc::variant value     = common_properties_interface::get(info->name);
            m_current[info->name] = value;
        } else {
            // TODO:: 考虑到我们实现了在对象中遮蔽接口属性，这里后续需要先判断是否遮蔽
            const auto* iface     = to_interface_ptr(m_object, m_interface_metadata->interface);
            mc::variant value     = iface->get_property(info->name, mc::engine::property_options::memory);
            m_current[info->name] = value;
        }
    }

    const abstract_object*                       m_object;
    const interface_metadata*                    m_interface_metadata;
    mc::mutable_dict                             m_current;
    std::map<std::string_view, mc::mutable_dict> m_interfaces;
};

object_manager_interface::objects_type
object_manager_interface::get_managed_objects() const {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }

    // TODO:: 这里遍历管理的子对象需要加锁
    auto&        objs = object->get_managed_objects();
    objects_type objects;
    for (auto& [obj_path, obj] : objs) {
        object_manager_vistor v(obj);
        obj->get_metadata().visit(v);
        object_call_stack::context object_ctx{obj->get_service(), *obj};
        common_properties_interface::get_instance().get_metadata().visit(v);
        interfaces_type interfaces;
        for (auto& [name, value] : v.m_interfaces) {
            interfaces[name] = value;
        }
        auto key     = path(std::string(obj_path));
        objects[key] = interfaces;
    }
    return objects;
}

common_properties_interface& common_properties_interface::get_instance() {
    static common_properties_interface instance;
    return instance;
}

mc::variant common_properties_interface::get(std::string_view property_name) {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }
    if (property_name == "ParentPath") {
        return object->get_owner()->get_object_path();
    }
    if (property_name == "ObjectName") {
        return object->get_object_name();
    }
    if (property_name == "ClassName") {
        return object->get_class_name();
    }
    if (property_name == "ObjectIdentifier") {
        return object->get_object_identifier();
    }
    return {};
}

mc::variant common_properties_interface::get_with_context(std::map<std::string, std::string> context, std::string_view interface_name,
                                                          std::string_view property_name) {
    if (interface_name == common_properties_name) {
        return get(property_name);
    }
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }
    return object->get_property(property_name, interface_name);
}

mc::dict common_properties_interface::get_all() {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }
    mc::mutable_dict dict;
    dict["ParentPath"]       = object->get_owner()->get_object_path();
    dict["ObjectName"]       = object->get_object_name();
    dict["ClassName"]        = object->get_class_name();
    dict["ObjectIdentifier"] = object->get_object_identifier();
    return dict;
}

void common_properties_interface::set_with_context(std::map<std::string, std::string> context, std::string_view interface_name,
                                                   std::string_view property_name, const mc::variant& value) {
    if (interface_name == common_properties_name) {
        return;
    }
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        elog("failed to get object from call stack");
        return;
    }
    object->set_property(property_name, value, interface_name);
}

invoke_result standard_interfaces::invoke(abstract_object* object, std::string_view method_name,
                                          const mc::variants& args,
                                          std::string_view    interface_name) {
    // 优化：所有的标准接口都有同样的前缀，前缀不匹配可以快速返回
    if (!mc::string::starts_with(interface_name, common_prefix)) {
        if (interface_name == common_properties_name) {
            return common_properties_interface::get_instance().invoke(method_name, args);
        }
        return {};
    }

    std::string_view name = interface_name.substr(common_prefix.size());
    if (name == properties_name) {
        return properties_interface::get_instance().invoke(method_name, args);
    } else if (name == introspectable_name) {
        return introspectable_interface::get_instance().invoke(method_name, args);
    } else if (name == peer_name) {
        return peer_interface::get_instance().invoke(method_name, args);
    } else if (name == object_manager_name) {
        return object_manager_interface::get_instance().invoke(method_name, args);
    }

    return {};
}

} // namespace mc::engine

MC_REFLECT(mc::engine::properties_interface,
           ((get, "Get"))((get_all, "GetAll"))((set, "Set"))((properties_changed,
                                                              "PropertiesChanged")))
MC_REFLECT(mc::engine::introspectable_interface, ((introspect, "Introspect")))
MC_REFLECT(mc::engine::peer_interface, ((ping, "Ping"))((get_machine_id, "GetMachineId")))
MC_REFLECT(mc::engine::object_manager_interface,
           ((get_managed_objects, "GetManagedObjects"))((interfaces_added, "InterfacesAdded"))(
               (interfaces_removed, "InterfacesRemoved")))
MC_REFLECT(mc::engine::common_properties_interface,
           ((m_parent_path, "ParentPath"))((m_object_name, "ObjectName"))           //
           ((m_class_name, "ClassName"))((m_object_identifier, "ObjectIdentifier")) //
           ((get_with_context, "GetWithContext"))((set_with_context, "SetWithContext")))
