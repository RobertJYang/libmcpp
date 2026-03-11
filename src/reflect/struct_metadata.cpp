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

#include <mc/reflect/metadata.h>
#include <mc/sync/mutex_box.h>
#include <mc/sync/shared_mutex.h>

namespace mc::reflect {

struct struct_metadata::impl {
    using members_map_t        = std::unordered_map<std::string_view, member_node<member_info_base>>;
    using offset_members_map_t = std::unordered_map<std::uintptr_t, const member_info_base*>;
    using owner_members_t      = std::vector<const member_info_base*>;

    struct data_t {
        property_list   ordered_properties;   // visit_properties 希望保持顺序
        method_list     ordered_methods;      // visit_methods 希望保持顺序
        base_class_list ordered_base_classes; // visit_base_classes 希望保持顺序
        custom_list     ordered_customs;      // visit_customs 希望保持顺序

        members_map_t        name_to_members;
        offset_members_map_t offset_to_members;

        // 在原始反射元数据不满足要求需要调整的场景，因为传入的元数据是编译期常量，而编译期常量可能会放到只读数据段，
        // 这里克隆一份后再调整更安全，我们用 owner_members 来持有这个克隆数据的所有权。
        owner_members_t owner_members;
    };

    static void add_property_info(data_t& data, const property_type_info* property);
    static void add_method_info(data_t& data, const method_type_info* method);
    static void add_base_class_info(data_t& data, const base_class_type_info* base_class);
    static void add_custom_info(data_t& data, const member_info_base* custom);

    static member_node<member_info_base>& add_member_info(data_t& data, const member_info_base* member);

    static void load_base_class_members(data_t& data, const base_class_type_info* base_class);

    std::string_view name;
    type_id_type     type_id;

    // TODO:: 理论上这里也是不需要锁的，元数据缓存在构建单例的时候一次性创建，之后不会再有变化。
    // 其他读取接口也都不需要加锁，唯一的场景就是动态模块注册的反射元数据，在动态库卸载的时候会被释放，
    // 但这也应该由模块管理器来管理，这里不需要关心。
    // 先加上锁吧，暂时用 unsafe_get_data() 来规避，后续再想办法优化。
    mc::mutex_box<data_t, mc::shared_mutex> m_data;
};

member_node<member_info_base>& struct_metadata::impl::add_member_info(data_t& data, const member_info_base* member)
{
    auto it = data.name_to_members.emplace(member->name, member_node<member_info_base>{member});
    return reinterpret_cast<member_node<member_info_base>&>(it.first->second);
}

void struct_metadata::impl::add_property_info(data_t& data, const property_type_info* property)
{
    if (data.name_to_members.find(property->name) != data.name_to_members.end()) {
        return;
    }

    auto& node = add_member_info(data, property);
    data.ordered_properties.push_front(reinterpret_cast<member_node<property_type_info>&>(node));

    if (property->type() == member_info_type::property) {
        // 只缓存普通属性，计算属性没有 offset
        data.offset_to_members.emplace(property->offset(), property);
    }
}

void struct_metadata::impl::add_method_info(data_t& data, const method_type_info* method)
{
    if (data.name_to_members.find(method->name) != data.name_to_members.end()) {
        return;
    }

    auto& node = add_member_info(data, method);
    data.ordered_methods.push_front(reinterpret_cast<member_node<method_type_info>&>(node));

    auto offset = method->offset();
    if (offset != 0) {
        // 函数地址作为 offset 缓存
        data.offset_to_members.emplace(offset, method);
    }
}

void struct_metadata::impl::add_base_class_info(data_t& data, const base_class_type_info* base_class)
{
    if (data.name_to_members.find(base_class->name) != data.name_to_members.end()) {
        return;
    }

    // 基类信息拷贝一份，因为传入的是编译期常量，由于我们没有找到编译期计算基类偏移
    // 的方法，所以只能运行期拷贝一份数据再设置正确的 base_offset 值
    auto new_base_class = base_class->clone();
    new_base_class->base_offset += base_class->offset(); // 运行时计算基类偏移

    auto& node = add_member_info(data, new_base_class);
    data.ordered_base_classes.push_front(reinterpret_cast<member_node<base_class_type_info>&>(node));
    data.owner_members.push_back(new_base_class); // 保持所有权，析构时释放

    // 将基类的成员信息也拷贝一份到子类中
    load_base_class_members(data, base_class);
}

void struct_metadata::impl::add_custom_info(data_t& data, const member_info_base* custom)
{
    if (data.name_to_members.find(custom->name) != data.name_to_members.end()) {
        return;
    }

    auto& node = add_member_info(data, custom);
    data.ordered_customs.push_front(reinterpret_cast<member_node<member_info_base>&>(node));
}

void struct_metadata::impl::load_base_class_members(data_t& data, const base_class_type_info* base_class)
{
    const struct_metadata& base_class_metadata = base_class->get_metadata();
    std::uintptr_t         offset              = base_class->offset();

    base_class_metadata.visit_properties([&](const property_type_info* property) {
        // 子类属性覆盖基类属性，所以已经存在的属性不需要再添加
        if (data.name_to_members.find(property->name) != data.name_to_members.end()) {
            return visit_status::VS_CONTINUE;
        }

        auto new_property = property->clone();
        new_property->base_offset += offset;
        add_property_info(data, static_cast<const property_type_info*>(new_property));
        data.owner_members.push_back(new_property); // 保持所有权，析构时释放
        return visit_status::VS_CONTINUE;
    });

    // 处理基类方法
    base_class_metadata.visit_methods([&](const method_type_info* method) {
        // 子类方法覆盖基类方法，所以已经存在的属性不需要再添加
        if (data.name_to_members.find(method->name) != data.name_to_members.end()) {
            return visit_status::VS_CONTINUE;
        }

        auto new_method = method->clone();
        new_method->base_offset += offset;
        add_method_info(data, static_cast<const method_type_info*>(new_method));
        data.owner_members.push_back(new_method); // 保持所有权，析构时释放
        return visit_status::VS_CONTINUE;
    });

    // 基类原信息也拷贝一份到子类中，支持精确给定基类名称操作
    base_class_metadata.visit_base_classes([&](const base_class_type_info* base_class) {
        if (data.name_to_members.find(base_class->name) != data.name_to_members.end()) {
            return visit_status::VS_CONTINUE;
        }

        auto new_base_class = base_class->clone();
        new_base_class->base_offset += offset;
        add_base_class_info(data, static_cast<const base_class_type_info*>(new_base_class));
        data.owner_members.push_back(new_base_class); // 保持所有权，析构时释放
        return visit_status::VS_CONTINUE;
    });
}

struct_metadata::struct_metadata(std::string_view name, type_id_type type_id)
    : m_impl(std::make_unique<impl>())
{
    m_impl->name    = name;
    m_impl->type_id = type_id;
}

struct_metadata::~struct_metadata()
{
    auto& data = m_impl->m_data.unsafe_get_data();

    data.ordered_properties.clear();
    data.ordered_methods.clear();
    data.ordered_base_classes.clear();
    data.ordered_customs.clear();

    for (auto& member : data.owner_members) {
        // 成员信息的各个子类都满足编译期常量要求，可以直接释放内存不需要析构，
        // 并且也做不到通过基类指针析构，因为 C++17 不允许编译期常量结构有虚析构函数
        operator delete(const_cast<member_info_base*>(member));
    }
    data.owner_members.clear();

    // member_node<member_info_base> 所有权在这里，最后释放
    data.name_to_members.clear();
    data.offset_to_members.clear();
}

type_id_type struct_metadata::get_type_id() const noexcept
{
    return m_impl->type_id;
}

std::string_view struct_metadata::get_name() const noexcept
{
    return m_impl->name;
}

void struct_metadata::add_property_info(const property_type_info* property)
{
    struct_metadata::impl::add_property_info(m_impl->m_data.unsafe_get_data(), property);
}

void struct_metadata::add_method_info(const method_type_info* method)
{
    struct_metadata::impl::add_method_info(m_impl->m_data.unsafe_get_data(), method);
}

void struct_metadata::add_base_class_info(const base_class_type_info* base_class)
{
    struct_metadata::impl::add_base_class_info(m_impl->m_data.unsafe_get_data(), base_class);
}

void struct_metadata::add_custom_info(const member_info_base* custom)
{
    struct_metadata::impl::add_custom_info(m_impl->m_data.unsafe_get_data(), custom);
}

void struct_metadata::add_members_finish()
{
    // 为了节省内存我们使用了单向链表存储有序数据，这里反转一下链表保证顺序
    auto& data = m_impl->m_data.unsafe_get_data();
    data.ordered_properties.reverse();
    data.ordered_methods.reverse();
    data.ordered_base_classes.reverse();
    data.ordered_customs.reverse();
}

const property_type_info* struct_metadata::get_property_info(std::string_view name) const
{
    auto& data = m_impl->m_data.unsafe_get_data();
    auto  it   = data.name_to_members.find(name);
    if (it == data.name_to_members.end() || !it->second.member->is_property_type()) {
        return nullptr;
    }

    return static_cast<const property_type_info*>(it->second.member);
}

const method_type_info* struct_metadata::get_method_info(std::string_view name) const
{
    auto& data = m_impl->m_data.unsafe_get_data();
    auto  it   = data.name_to_members.find(name);
    if (it == data.name_to_members.end() || it->second.member->type() != member_info_type::method) {
        return nullptr;
    }

    return static_cast<const method_type_info*>(it->second.member);
}

const method_type_info* struct_metadata::get_method_info(size_t offset) const
{
    auto& data = m_impl->m_data.unsafe_get_data();
    auto  it   = data.offset_to_members.find(offset);
    if (it == data.offset_to_members.end() || it->second->type() != member_info_type::method) {
        return nullptr;
    }

    return static_cast<const method_type_info*>(it->second);
}

const base_class_type_info* struct_metadata::get_base_class_info(std::string_view name) const
{
    auto& data = m_impl->m_data.unsafe_get_data();
    auto  it   = data.name_to_members.find(name);
    if (it == data.name_to_members.end() || it->second.member->type() != member_info_type::base_class) {
        return nullptr;
    }

    return static_cast<const base_class_type_info*>(it->second.member);
}

const property_type_info* struct_metadata::get_property_info(size_t offset) const
{
    auto& data = m_impl->m_data.unsafe_get_data();
    auto  it   = data.offset_to_members.find(offset);
    if (it == data.offset_to_members.end() || !it->second->is_property_type()) {
        return nullptr;
    }

    return static_cast<const property_type_info*>(it->second);
}

const member_info_base* struct_metadata::get_custom_info(std::string_view name, size_t reflect_type) const
{
    auto& data = m_impl->m_data.unsafe_get_data();
    auto  it   = data.name_to_members.find(name);
    if (it == data.name_to_members.end() || it->second.member->type() < member_info_type::custom_start) {
        return nullptr;
    }

    if (reflect_type != 0 && it->second.member->type() != reflect_type) {
        return nullptr;
    }

    return it->second.member;
}

void struct_metadata::visit_properties(const property_visitor_t& visitor) const
{
    for (auto& property : get_properties()) {
        if (visitor(property.member) == visit_status::VS_BREAK) {
            break;
        }
    }
}

void struct_metadata::visit_methods(const method_visitor_t& visitor) const
{
    for (auto& method : get_methods()) {
        if (visitor(method.member) == visit_status::VS_BREAK) {
            break;
        }
    }
}

void struct_metadata::visit_base_classes(const base_class_visitor_t& visitor) const
{
    for (auto& base_class : get_base_classes()) {
        if (visitor(base_class.member) == visit_status::VS_BREAK) {
            break;
        }
    }
}

void struct_metadata::visit_customs(const custom_visitor_t& visitor) const
{
    for (auto& custom : get_custom_members()) {
        if (visitor(custom.member) == visit_status::VS_BREAK) {
            break;
        }
    }
}

const property_list& struct_metadata::get_properties() const
{
    return m_impl->m_data.unsafe_get_data().ordered_properties;
}

const method_list& struct_metadata::get_methods() const
{
    return m_impl->m_data.unsafe_get_data().ordered_methods;
}

const base_class_list& struct_metadata::get_base_classes() const
{
    return m_impl->m_data.unsafe_get_data().ordered_base_classes;
}

const member_list& struct_metadata::get_custom_members() const
{
    return m_impl->m_data.unsafe_get_data().ordered_customs;
}

} // namespace mc::reflect