/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_ENGINE_OBJECT_METADATA_H
#define MC_ENGINE_OBJECT_METADATA_H

#include <mc/engine/signal_info.h>
#include <mc/intrusive/list.h>
#include <mc/reflect.h>
#include <mc/small_function.h>
#include <mc/string_view.h>
#include <vector>

namespace mc::engine {

using property_type_info  = mc::reflect::property_type_info;
using interface_type_info = mc::reflect::property_type_info;
using method_type_info    = mc::reflect::method_type_info;
using member_info_base    = mc::reflect::member_info_base;

struct interface_metadata;
class abstract_interface;
class abstract_object;

struct metadata_visitor {
    virtual void handle_interface_begin(const interface_metadata& iface)
    {}
    virtual void handle_interface_end(const interface_metadata& iface)
    {}
    virtual void handle(const property_type_info* info)
    {}
    virtual void handle(const method_type_info* info)
    {}
    virtual void handle(const signal_type_info* info)
    {}
};

template <typename T>
class pointer_list {
public:
    using value_type = T;
    using iterator = typename std::vector<value_type>::iterator;
    using const_iterator = typename std::vector<value_type>::const_iterator;

    pointer_list() = default;
    pointer_list(std::initializer_list<value_type> init) : m_items(init)
    {}

    bool empty() const noexcept
    {
        return m_items.empty();
    }

    std::size_t size() const noexcept
    {
        return m_items.size();
    }

    void reserve(std::size_t count)
    {
        m_items.reserve(count);
    }

    void push_back(value_type value)
    {
        m_items.push_back(value);
    }

    value_type& operator[](std::size_t index)
    {
        return m_items[index];
    }

    const value_type& operator[](std::size_t index) const
    {
        return m_items[index];
    }

    iterator begin() noexcept
    {
        return m_items.begin();
    }

    const_iterator begin() const noexcept
    {
        return m_items.begin();
    }

    iterator end() noexcept
    {
        return m_items.end();
    }

    const_iterator end() const noexcept
    {
        return m_items.end();
    }

private:
    std::vector<value_type> m_items;
};

using struct_metadata_list = pointer_list<const mc::reflect::struct_metadata*>;
using interface_type_info_list = pointer_list<const interface_type_info*>;

class MC_API metadata_list {
public:
    metadata_list() = default;
    metadata_list(mc::string_view class_name, const mc::reflect::struct_metadata** obj_metadata, size_t count);
    ~metadata_list();

    metadata_list(metadata_list&&) noexcept            = default;
    metadata_list& operator=(metadata_list&&) noexcept = default;
    metadata_list(const metadata_list&)                = default;
    metadata_list& operator=(const metadata_list&)     = default;

    mc::string_view get_class_name() const;

    const property_type_info* get_property_info(mc::string_view property_name) const;
    const method_type_info*   get_method_info(mc::string_view method_name) const;
    const signal_type_info*   get_signal_info(mc::string_view signal_name) const;
    const property_type_info* get_property_info(std::uintptr_t offset) const;

    using visit_properties_type = mc::small_function<void(const property_type_info*), 64>;
    using visit_methods_type    = mc::small_function<void(const method_type_info*), 64>;
    using visit_signals_type    = mc::small_function<void(const signal_type_info*), 64>;

    void visit_properties(const visit_properties_type& v) const;
    void visit_methods(const visit_methods_type& v) const;
    void visit_signals(const visit_signals_type& v) const;
    void visit(metadata_visitor& v) const;

    const struct_metadata_list& get_struct_metadata() const;

private:
    struct_metadata_list array; // interface 和其基类的 metadata
    mc::string_view      class_name;
};

struct interface_metadata {
    interface_metadata() = default;
    interface_metadata(const property_type_info* iface, const metadata_list* md) : interface(iface), metadata(md)
    {}

    interface_metadata(const interface_metadata& other)            = default;
    interface_metadata(interface_metadata&& other)                 = default;
    interface_metadata& operator=(const interface_metadata& other) = default;
    interface_metadata& operator=(interface_metadata&& other)      = default;

    const property_type_info* interface{nullptr};
    const metadata_list*      metadata{nullptr};
};

inline abstract_interface* to_interface_ptr(const abstract_object* obj, const property_type_info* info) noexcept
{
    if (info == nullptr) {
        return MC_MEMBER_PTR(abstract_interface*, obj, 0);
    }
    return MC_MEMBER_PTR(abstract_interface*, obj, info->offset());
}

// 表示对象中某个 interface 的属性、方法、信号的元信息
template <typename ReflectItem>
struct interface_item : mc::intrusive::slist_base_hook<> {
    const property_type_info* interface{nullptr}; // interface 在对象中的属性元信息
    const ReflectItem*        item{nullptr};      // interface 具体属性、方法、信号类型的元信息

    interface_item() = default;
    interface_item(const property_type_info* interface, const ReflectItem* item) : interface(interface), item(item)
    {}

    // 辅助函数：给定一个对象指针，计算当前元信息代表的属性、方法、信号在对象中的指针
    template <typename ItemType, typename ObjectType>
    ItemType* to_item_ptr(ObjectType* obj) const noexcept
    {
        if (interface == nullptr) {
            return MC_MEMBER_PTR(ItemType*, obj, item->offset());
        }
        return MC_MEMBER_PTR(ItemType*, obj, item->offset() + interface->offset());
    }
};

// 单项链表，只占一个指针大小，方便保持顺序遍历
using property_list =
    mc::intrusive::slist<interface_item<property_type_info>, mc::intrusive::constant_time_size<false>>;
using method_list = mc::intrusive::slist<interface_item<method_type_info>, mc::intrusive::constant_time_size<false>>;
using signal_list = mc::intrusive::slist<interface_item<signal_type_info>, mc::intrusive::constant_time_size<false>>;

struct MC_API object_metadata {
public:
    object_metadata(mc::string_view class_name, const mc::reflect::struct_metadata& metadata,
                    const metadata_list** interface_metadata = nullptr, size_t count = 0);
    ~object_metadata();

    object_metadata(const object_metadata& other)            = delete;
    object_metadata(object_metadata&& other)                 = default;
    object_metadata& operator=(const object_metadata& other) = delete;
    object_metadata& operator=(object_metadata&& other)      = default;

    const metadata_list&                     get_object_metadata() const;
    const interface_type_info*               get_interface_info(mc::string_view name) const;
    const interface_item<property_type_info> get_property_info(mc::string_view name,
                                                               mc::string_view interface_name) const;
    const interface_item<method_type_info>   get_method_info(mc::string_view name,
                                                             mc::string_view interface_name) const;
    const interface_item<signal_type_info>   get_signal_info(mc::string_view name,
                                                             mc::string_view interface_name) const;

    interface_type_info_list get_interfaces() const;
    const property_list&                    get_propertis() const;
    const method_list&                      get_methods() const;
    const signal_list&                      get_signals() const;

    using visit_properties_type = mc::small_function<void(interface_item<property_type_info>), 64>;
    using visit_methods_type    = mc::small_function<void(interface_item<method_type_info>), 64>;
    using visit_signals_type    = mc::small_function<void(interface_item<signal_type_info>), 64>;
    using visit_interfaces_type = mc::small_function<void(const interface_metadata&), 64>;

    void visit_properties(const visit_properties_type& v) const;
    void visit_methods(const visit_methods_type& v) const;
    void visit_signals(const visit_signals_type& v) const;
    void visit_interfaces(const visit_interfaces_type& v) const;
    void visit(metadata_visitor& v) const;

private:
    struct impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace mc::engine

#endif // MC_ENGINE_OBJECT_H