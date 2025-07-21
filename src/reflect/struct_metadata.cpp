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

namespace mc::reflect::detail {

struct struct_metadata::impl {
    using ordered_property_t    = std::vector<const property_type_info*>;
    using property_map_t        = std::unordered_map<std::string_view, const property_type_info*>;
    using method_map_t          = std::unordered_map<std::string_view, const method_type_info*>;
    using base_class_map_t      = std::unordered_map<std::string_view, const base_class_type_info*>;
    using property_offset_map_t = std::unordered_map<size_t, const property_type_info*>;
    using method_offset_map_t   = std::unordered_map<size_t, const method_type_info*>;
    using base_class_members_t  = std::vector<member_info_base*>;

    struct data_t {
        ordered_property_t    ordered_properties; // visit_property 希望保持顺序
        property_map_t        name_to_properties;
        property_offset_map_t offset_to_properties;
        method_map_t          name_to_methods;
        method_offset_map_t   offset_to_methods;
        base_class_map_t      name_to_base_class;
        base_class_members_t  owner_members;
    };

    static void add_property_info(data_t& data, const property_type_info* property);
    static void add_method_info(data_t& data, const method_type_info* method);
    static void add_base_class_info(data_t& data, const base_class_type_info* base_class);
    static void load_base_class_members(data_t& data, const base_class_type_info* base_class);

    std::string_view                        name;
    mc::mutex_box<data_t, mc::shared_mutex> m_data;
};

void struct_metadata::impl::add_property_info(data_t& data, const property_type_info* property) {
    if (data.name_to_properties.find(property->name) != data.name_to_properties.end()) {
        return;
    }

    data.ordered_properties.push_back(property);
    data.name_to_properties[property->name] = property;
    if (property->type() == member_info_type::property) {
        // 只缓存普通属性，计算属性没有 offset
        data.offset_to_properties[property->offset()] = property;
    }
}

void struct_metadata::impl::add_method_info(data_t& data, const method_type_info* method) {
    if (data.name_to_methods.find(method->name) != data.name_to_methods.end()) {
        return;
    }

    data.name_to_methods[method->name]       = method;
    data.offset_to_methods[method->offset()] = method;
}

void struct_metadata::impl::add_base_class_info(data_t& data, const base_class_type_info* base_class) {
    if (data.name_to_base_class.find(base_class->name) != data.name_to_base_class.end()) {
        return;
    }

    // 基类信息拷贝一份，因为传入的是编译期常量，由于我们没有找到编译期计算基类偏移
    // 的方法，所以只能运行期拷贝一份数据再设置正确的 base_offset 值
    auto new_base_class                       = base_class->clone();
    new_base_class->base_offset               = base_class->offset(); // 运行时计算基类偏移
    data.name_to_base_class[base_class->name] = static_cast<const base_class_type_info*>(new_base_class);
    data.owner_members.push_back(new_base_class); // 保持所有权，析构时释放

    // 将基类的成员信息也拷贝一份到子类中
    load_base_class_members(data, base_class);
}

void struct_metadata::impl::load_base_class_members(data_t& data, const base_class_type_info* base_class) {
    const struct_metadata& base_class_metadata = base_class->get_metadata();
    std::uintptr_t         offset              = base_class->offset();

    base_class_metadata.visit_property([&](const property_type_info* property) {
        // 子类属性覆盖基类属性，所以已经存在的属性不需要再添加
        if (data.name_to_properties.find(property->name) != data.name_to_properties.end()) {
            return visit_status::VS_CONTINUE;
        }

        auto new_property = property->clone();
        new_property->base_offset += offset;
        add_property_info(data, static_cast<const property_type_info*>(new_property));
        data.owner_members.push_back(new_property);
        return visit_status::VS_CONTINUE;
    });

    // 处理基类方法
    base_class_metadata.visit_method([&](const method_type_info* method) {
        // 子类方法覆盖基类方法，所以已经存在的属性不需要再添加
        if (data.name_to_methods.find(method->name) != data.name_to_methods.end()) {
            return visit_status::VS_CONTINUE;
        }

        auto new_method = method->clone();
        new_method->base_offset += offset;
        add_method_info(data, static_cast<const method_type_info*>(new_method));
        data.owner_members.push_back(new_method);
        return visit_status::VS_CONTINUE;
    });

    // 基类原信息也拷贝一份到子类中，支持精确给定基类名称操作
    base_class_metadata.visit_base_class([&](const base_class_type_info* base_class) {
        if (data.name_to_base_class.find(base_class->name) != data.name_to_base_class.end()) {
            return visit_status::VS_CONTINUE;
        }

        auto new_base_class = base_class->clone();
        new_base_class->base_offset += offset;
        data.name_to_base_class[base_class->name] = static_cast<const base_class_type_info*>(new_base_class);
        data.owner_members.push_back(new_base_class);
        return visit_status::VS_CONTINUE;
    });
}

struct_metadata::struct_metadata(std::string_view name)
    : m_impl(std::make_unique<impl>()) {
    m_impl->name = name;
}

struct_metadata::~struct_metadata() {
    m_impl->m_data.with_lock([&](auto& data) {
        for (auto* member : data.owner_members) {
            // 成员信息的各个子类都满足编译期常量要求，可以直接释放内存不需要析构
            operator delete(member);
        }
        data.owner_members.clear();
    });
}

std::string_view struct_metadata::get_name() const {
    return m_impl->name;
}

void struct_metadata::add_property_info(const property_type_info* property) {
    m_impl->m_data.with_lock([&](auto& data) {
        struct_metadata::impl::add_property_info(data, property);
    });
}

void struct_metadata::add_method_info(const method_type_info* method) {
    m_impl->m_data.with_lock([&](auto& data) {
        struct_metadata::impl::add_method_info(data, method);
    });
}

void struct_metadata::add_base_class_info(const base_class_type_info* base_class) {
    m_impl->m_data.with_lock([&](auto& data) {
        struct_metadata::impl::add_base_class_info(data, base_class);
    });
}

const property_type_info* struct_metadata::get_property_info(std::string_view name) const {
    auto lock = m_impl->m_data.rlock();
    auto it   = lock->name_to_properties.find(name);
    return it != lock->name_to_properties.end() ? it->second : nullptr;
}

const method_type_info* struct_metadata::get_method_info(std::string_view name) const {
    auto lock = m_impl->m_data.rlock();
    auto it   = lock->name_to_methods.find(name);
    return it != lock->name_to_methods.end() ? it->second : nullptr;
}

const method_type_info* struct_metadata::get_method_info(size_t offset) const {
    auto lock = m_impl->m_data.rlock();
    auto it   = lock->offset_to_methods.find(offset);
    return it != lock->offset_to_methods.end() ? it->second : nullptr;
}

const base_class_type_info* struct_metadata::get_base_class_info(std::string_view name) const {
    auto lock = m_impl->m_data.rlock();
    auto it   = lock->name_to_base_class.find(name);
    return it != lock->name_to_base_class.end() ? it->second : nullptr;
}

const property_type_info* struct_metadata::get_property_info(size_t offset) const {
    auto lock = m_impl->m_data.rlock();
    auto it   = lock->offset_to_properties.find(offset);
    return it != lock->offset_to_properties.end() ? it->second : nullptr;
}

void struct_metadata::visit_property(const property_visitor_t& visitor) const {
    auto lock = m_impl->m_data.rlock();
    for (auto& property : lock->ordered_properties) {
        auto unlock = lock.scoped_unlock();
        if (visitor(property) == visit_status::VS_BREAK) {
            break;
        }
    }
}

void struct_metadata::visit_method(const method_visitor_t& visitor) const {
    auto lock = m_impl->m_data.rlock();
    for (auto& [_, method] : lock->offset_to_methods) {
        auto unlock = lock.scoped_unlock();
        if (visitor(method) == visit_status::VS_BREAK) {
            break;
        }
    }
}

void struct_metadata::visit_base_class(const base_class_visitor_t& visitor) const {
    auto lock = m_impl->m_data.rlock();
    for (auto& [_, base_class] : lock->name_to_base_class) {
        auto unlock = lock.scoped_unlock();
        if (visitor(base_class) == visit_status::VS_BREAK) {
            break;
        }
    }
}

} // namespace mc::reflect::detail