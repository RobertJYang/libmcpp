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

#ifndef MC_ENGINE_INTERFACE_H
#define MC_ENGINE_INTERFACE_H

#include <mc/atomic_ref.h>
#include <mc/core/object.h>
#include <mc/engine/base.h>
#include <mc/engine/property.h>
#include <mc/engine/signal_info.h>
#include <mc/reflect/base.h>
#include <mc/traits.h>

namespace mc::engine {
namespace detail {
template <typename Members>
constexpr void set_property_type_flag(Members& members)
{
    mc::traits::tuple_for_each(members, [](auto& member) {
        using member_info_type = mc::traits::remove_pointers_t<std::decay_t<decltype(member)>>;
        if constexpr (mc::reflect::has_tag_v<mc::reflect::property_tag, member_info_type>) {
            using member_type = typename member_info_type::member_type;
            if constexpr (mc::traits::is_specialization_of_v<member_type, property>) {
                member.set_flags(MC_REFLECT_FLAG_PROPERTY_TPL); // 标记对象的属性是 property<T> 类型的反射元数据
            }
        }
    });
}

template <typename T, typename BaseT, typename Tuple>
constexpr auto make_inherit_types_impl(Tuple&& tuple)
{
    if constexpr (std::is_void_v<BaseT>) {
        return std::tuple_cat(tuple, std::tuple<T*>{nullptr});
    } else {
        return make_inherit_types_impl<BaseT, typename BaseT::base_type>(
            std::tuple_cat(tuple, std::tuple<T*>{nullptr}));
    }
}

template <typename T, typename BaseT>
constexpr auto make_inherit_types()
{
    return make_inherit_types_impl<T, BaseT>(std::tuple<>{});
}

template <typename InheritTypes>
auto make_interface_metadata()
{
    std::array<const mc::reflect::struct_metadata*, std::tuple_size_v<InheritTypes>> metadatas{};
    mc::traits::tuple_element_for_each<InheritTypes>([&](auto type) {
        using type_t = std::remove_pointer_t<typename decltype(type)::type>;

        metadatas[decltype(type)::index] = &mc::reflect::reflector<type_t>::get_metadata();
    });
    return metadatas;
}

} // namespace detail

class MC_API interface_impl : public abstract_interface {
public:
    using abstract_interface::connect;

    interface_impl();
    ~interface_impl() override;

    abstract_object* get_owner() const override;
    void             set_owner(abstract_object* owner);

    const property_type_info* get_property_info(std::string_view property_name) const override;
    const method_type_info*   get_method_info(std::string_view method_name) const override;
    const signal_type_info*   get_signal_info(std::string_view signal_name) const override;
    const property_type_info* get_property_info(const void* prop_addr) const override;

    bool        set_property(std::string_view property_name, const mc::variant& value) override;
    mc::variant get_property(std::string_view property_name, int options) const override;
    mc::dict    get_all_properties(int options) const override;

    invoke_result invoke(std::string_view method_name, const mc::variants& args) override;
    async_result  async_invoke(std::string_view method_name, const mc::variants& args) override;

    mc::connection_type connect(std::string_view signal_name, slot_type slot) override;
    mc::variant         emit(std::string_view signal_name, const mc::variants& args) override;
    void                notify_property_changed(
                       const mc::variant& value, const property_base& prop) override;
    property_changed_signal& property_changed() override;

    static void from_variant(const mc::dict& d, interface_impl& obj);
    static void to_variant(const interface_impl& obj, mc::dict& dict, int options = 0);

protected:
    void init_property_base(const mc::reflect::struct_metadata& metadata);

    abstract_object*                         m_owner;
    std::unique_ptr<property_changed_signal> m_property_changed_signal;
};

template <typename T, typename BaseT = interface_impl>
class interface : public BaseT {
public:
    static_assert(std::is_same_v<interface_impl, BaseT> || std::is_base_of_v<interface_impl, BaseT>,
                  "BaseT must be interface_impl or derived from interface_impl");

    using is_interface   = std::true_type;
    using interface_type = T;
    using base_type      = std::conditional_t<std::is_same_v<interface_impl, BaseT>, void, BaseT>;
    using inherit_types  = decltype(detail::make_inherit_types<interface_type, base_type>());

    static constexpr bool has_base_v = !std::is_void_v<base_type>;

    template <typename Members>
    static constexpr void initial_members(Members& members)
    {
        detail::set_property_type_flag(members);
    }

    interface()
    {
        interface_impl::init_property_base(mc::reflect::reflector<interface_type>::get_metadata());
    }

    std::string_view get_interface_name() const override
    {
        return interface_type::interface_name;
    }

    static const metadata_list& metadata()
    {
        auto& extension = mc::reflect::reflector<interface_type>::get_extension();

        mc::atomic_ref<void*> extension_ref(extension.data);
        if (extension_ref.load() != nullptr) {
            return *reinterpret_cast<metadata_list*>(extension_ref.load());
        }

        // 将接口的元数据缓存到 mc::reflect::static_metadata<interface_type>::extension 中，这块内存没有多少
        // 就泄漏掉吧，跟随进程结束释放
        auto  metadatas = detail::make_interface_metadata<inherit_types>();
        auto* list      = new metadata_list{
            interface_type::interface_name, &metadatas[0], metadatas.size()};
        void* expected = nullptr;
        if (extension_ref.compare_exchange_strong(expected, list)) {
            return *list;
        }

        // 其他线程已经创建了元数据，这里释放掉用已有的数据
        delete list;
        return *reinterpret_cast<metadata_list*>(expected);
    }

    const metadata_list& get_metadata() const override
    {
        return metadata();
    }
};

} // namespace mc::engine

#endif // MC_ENGINE_INTERFACE_H