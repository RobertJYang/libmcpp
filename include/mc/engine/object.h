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

#ifndef MC_ENGINE_OBJECT_H
#define MC_ENGINE_OBJECT_H

#include <mc/core/object.h>
#include <mc/engine/interface.h>
#include <mc/engine/metadata.h>
#include <mc/exception.h>
#include <mc/expr.h>
#include <mc/log.h>

namespace mc::engine {
namespace detail {

template <typename ObjectType>
auto get_object_static_interfaces()
{
    const auto& members = mc::reflect::get_static_members<ObjectType>();
    return mc::reflect::detail::filter_members<ObjectType, filter_interface>(members);
}

template <typename DeclaredInterfaces, typename Members>
constexpr void set_object_member_flags(Members& members)
{
    mc::traits::tuple_for_each(members, [&](auto& member) {
        using member_info_type = std::decay_t<decltype(member)>;
        if constexpr (mc::reflect::has_tag_v<mc::reflect::property_tag, member_info_type>) {
            using member_type = typename member_info_type::member_type;
            if constexpr (filter_interface::template check<member_info_type>) {
                // 标记对象的属性是 interface 类型的反射元数据
                member.set_flags(MC_REFLECT_FLAG_INTERFACE);
                // 设置接口属性在声明接口列表中的索引
                member.set_data(get_interface_index<member_type, DeclaredInterfaces>());
            } else if constexpr (mc::traits::is_specialization_of_v<member_type, property>) {
                // 标记对象的属性是 property<T> 类型的反射元数据
                member.set_flags(MC_REFLECT_FLAG_PROPERTY_TPL);
            }
        }
    });
}

template <typename ObjectType>
object_metadata make_object_interfaces_metadata()
{
    auto arr = make_interface_metadatas<typename ObjectType::DeclaredInterfaces>();
    if (arr.size() == 0) {
        return object_metadata(ObjectType::class_name, mc::reflect::reflector<ObjectType>::get_metadata());
    }
    return object_metadata(ObjectType::class_name, mc::reflect::reflector<ObjectType>::get_metadata(), &arr[0],
                           arr.size());
}
} // namespace detail

template <typename ValueType>
class object_optional_data {
public:
    object_optional_data()
    {}

    // 禁止拷贝和移动
    object_optional_data(const object_optional_data&)            = delete;
    object_optional_data& operator=(const object_optional_data&) = delete;
    object_optional_data(object_optional_data&&)                 = delete;
    object_optional_data& operator=(object_optional_data&&)      = delete;

    void set(std::string_view interface, std::string_view property, const ValueType& value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_map[std::string(interface)][std::string(property)] = value;
    }

    void unset(std::string_view interface, std::string_view property)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto                        interface_it = m_map.find(std::string(interface));
        if (interface_it == m_map.end()) {
            return;
        }
        auto& properties  = interface_it->second;
        auto  property_it = properties.find(std::string(property));
        if (property_it == properties.end()) {
            return;
        }
        properties.erase(property_it);
        if (properties.empty()) {
            m_map.erase(interface_it);
        }
    }

    ValueType get(std::string_view interface, std::string_view property)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto                        interface_it = m_map.find(std::string(interface));
        if (interface_it == m_map.end()) {
            return {};
        }
        auto property_it = interface_it->second.find(std::string(property));
        if (property_it == interface_it->second.end()) {
            return {};
        }
        return property_it->second;
    }

    void visit_interface(std::string_view interface, const std::function<void(std::string_view, const ValueType&)>& cb)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto                        interface_it = m_map.find(std::string(interface));
        if (interface_it == m_map.end()) {
            return;
        }
        for (const auto& [name, value] : interface_it->second) {
            cb(name, value);
        }
    }

private:
    std::unordered_map<std::string, std::unordered_map<std::string, ValueType>> m_map;
    std::mutex                                                                  m_mutex;
};

class MC_API object_impl : public abstract_object {
public:
    object_impl(core_object* parent);
    ~object_impl() override;

    bool init(const mc::dict& args);

    std::string_view get_object_path() const override;

    const managed_objects& get_managed_objects() const override;

    void                     notify_property_changed(const mc::variant& value, const property_base& prop) override;
    property_changed_signal& property_changed() override;
    void                     notify_property_update_shm(const mc::variant& value, const property_base& prop) override;
    property_changed_signal& property_update_shm() override;

    abstract_object* get_owner() const override;
    void             set_owner(abstract_object* owner) override;

    std::string_view get_object_name() const override;
    void             set_object_name(std::string_view name) override;

    object_identifier_t get_object_identifier() const override;
    void                set_object_identifier(const object_identifier_t& identifier) override;

    void set_object_path(std::string_view path) override;

    std::string_view get_position() const override;
    void             set_position(std::string_view position) override;

    void     set_service(service* s) override;
    service* get_service() const override;

    mc::variant get_property(std::string_view property_name, std::string_view interface_name,
                             int options) const override;
    bool        set_property(std::string_view property_name, const mc::variant& value,
                             std::string_view interface_name) override;
    mc::dict    get_all_properties(std::string_view interface_name = {}, int options = 0) const override;
    bool        has_property(std::string_view property_name, std::string_view interface_name) const override;

    void                   set_property_ref_info(std::string_view property_name, const std::string& info,
                                                 std::string_view interface_name = {}) override;
    std::string            get_property_ref_info(std::string_view property_name,
                                                 std::string_view interface_name = {}) const override;
    void                   set_property_sync_info(std::string_view property_name, property_sync_info_ptr info,
                                                  std::string_view interface_name = {}) override;
    property_sync_info_ptr get_property_sync_info(std::string_view property_name,
                                                  std::string_view interface_name = {}) const override;

    abstract_interface* get_interface(std::string_view interface_name) const noexcept override;
    bool                has_interface(std::string_view interface_name) const override;

    mc::connection_type connect(std::string_view signal_name, slot_type slot,
                                std::string_view interface_name = {}) override;
    mc::variant         emit(std::string_view signal_name, const mc::variants& args,
                             std::string_view interface_name = {}) override;

    bool                has_method(std::string_view method_name, std::string_view interface_name = {}) const override;
    invoke_result       invoke(std::string_view method_name, const mc::variants& args = {},
                               std::string_view interface_name = {}) override;
    result<mc::variant> async_invoke(std::string_view method_name, const mc::variants& args = {},
                                     std::string_view interface_name = {}) override;

    static void from_variant(const mc::dict& d, object_impl& obj);
    static void to_variant(const object_impl& obj, mc::dict& dict, int options = 0);

    virtual std::string_view get_path_pattern() const = 0;

protected:
    void add_managed_object(abstract_object* obj) override;
    void remove_managed_object(abstract_object* obj) override;
    void init_interface_object(const object_metadata& metadata);
    bool handle_override(property_base* prop, std::string_view property_name, const mc::variant& value,
                         std::string_view interface_name);

protected:
    mutable std::string                                           m_object_path;
    mutable std::string                                           m_position;
    abstract_object*                                              m_owner{nullptr};
    service*                                                      m_service{nullptr};
    managed_objects                                               m_managed_objects;
    std::unique_ptr<property_changed_signal>                      m_property_changed_signal;
    std::unique_ptr<property_changed_signal>                      m_property_update_shm_signal;
    object_identifier_t                                           m_object_identifier;
    std::unique_ptr<object_optional_data<std::string>>            m_properties_ref_info;
    std::unique_ptr<object_optional_data<property_sync_info_ptr>> m_properties_sync_info;
};

template <typename ObjectType>
class object : public object_impl {
public:
    using object_type = ObjectType;

    template <typename Members>
    static constexpr void initial_members(Members& members)
    {
        detail::set_object_member_flags<typename object_type::DeclaredInterfaces>(members);
    }

    object(core_object* parent = nullptr) : object_impl(parent)
    {
        init_interface_object(metadata());
    }

    virtual ~object() = default;

    std::string_view get_class_name() const noexcept override
    {
        return object_type::class_name;
    }

    std::string_view get_path_pattern() const noexcept override
    {
        return object_type::path_pattern;
    }

    static mc::shared_ptr<object_type> create()
    {
        return mc::make_shared<object_type>();
    }

    // 获取静态接口属性信息，仅在 MC_REFLECT 可见范围可用
    static auto get_static_interface_infos() noexcept
    {
        return detail::get_object_static_interfaces<object_type>();
    }

    static const object_metadata& metadata() noexcept
    {
        auto& extension = mc::reflect::reflector<object_type>::get_extension();

        mc::atomic_ref<void*> extension_ref(extension.data);
        if (extension_ref.load() != nullptr) {
            return *reinterpret_cast<object_metadata*>(extension_ref.load());
        }

        // 将接口的元数据缓存到 mc::reflect::static_metadata<object_type>::extension 中，这块内存没有多少
        // 就泄漏掉吧，跟随进程结束释放
        auto* md       = new object_metadata(detail::make_object_interfaces_metadata<object_type>());
        void* expected = nullptr;
        if (extension_ref.compare_exchange_strong(expected, md)) {
            return *md;
        }

        // 其他线程已经创建了元数据，这里释放掉用已有的数据
        delete md;
        return *reinterpret_cast<object_metadata*>(expected);
    }

    const object_metadata& get_metadata() const noexcept override
    {
        return metadata();
    }
};

} // namespace mc::engine

#endif // MC_ENGINE_OBJECT_H