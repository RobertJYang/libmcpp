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

#include <mc/engine/interface.h>

using namespace mc::reflect;

namespace mc::engine {

namespace detail {
static property_base* to_property_base(const interface_impl* self, const property_type_info* info) {
    return MC_MEMBER_PTR(property_base*, self, info->offset());
}
} // namespace detail

interface_impl::interface_impl() {
}

interface_impl::~interface_impl() {
}

abstract_object* interface_impl::get_owner() const {
    return m_owner;
}

void interface_impl::set_owner(abstract_object* owner) {
    m_owner = owner;
}

property_changed_signal& interface_impl::property_changed() {
    if (m_property_changed_signal == nullptr) {
        m_property_changed_signal = std::make_unique<property_changed_signal>();
    }

    return *m_property_changed_signal;
}

void interface_impl::notify_property_changed(const mc::variant& value, const property_base& prop) {
    if (m_property_changed_signal) {
        (*m_property_changed_signal)(value, prop);
    }
}

bool interface_impl::set_property(std::string_view property_name, const mc::variant& value) {
    try {
        const auto* property_info = get_property_info(property_name);
        if (property_info == nullptr) {
            return false;
        }

        if (property_info->has_flags(MC_REFLECT_FLAG_PROPERTY_TPL)) {
            detail::to_property_base(this, property_info)->set_value(value);
        } else {
            property_info->set_value(this, value);
        }
    } catch (const std::exception& e) {
        dlog("set property ${class}.${name} failed: ${error}",
             ("class", get_interface_name())("name", property_name)("error", e.what()));
        return false;
    }
    return true;
}

const signal_type_info* interface_impl::get_signal_info(std::string_view signal_name) const {
    return get_metadata().get_signal_info(signal_name);
}

const method_type_info* interface_impl::get_method_info(std::string_view method_name) const {
    return get_metadata().get_method_info(method_name);
}

const property_type_info* interface_impl::get_property_info(std::string_view property_name) const {
    return get_metadata().get_property_info(property_name);
}

const property_type_info* interface_impl::get_property_info(const void* prop_addr) const {
    std::uintptr_t offset = reinterpret_cast<std::uintptr_t>(prop_addr) -
                            reinterpret_cast<std::uintptr_t>(this);
    return get_metadata().get_property_info(offset);
}

mc::connection_type interface_impl::connect(std::string_view signal_name, slot_type slot) {
    auto* signal_info = get_signal_info(signal_name);
    if (!signal_info) {
        return {};
    }

    return signal_info->connect(this, std::move(slot));
}

mc::variant interface_impl::emit(std::string_view signal_name, const mc::variants& args) {
    auto* signal_info = get_signal_info(signal_name);
    if (!signal_info) {
        return {};
    }

    return signal_info->emit(this, args);
}

invoke_result interface_impl::invoke(std::string_view method_name, const mc::variants& args) {
    auto* method = get_method_info(method_name);
    if (method == nullptr) {
        return {};
    }

    auto* ctx = context_stack::top_value();
    if (ctx) {
        ctx->set_method(method);
    }
    return method->invoke(this, args);
}

async_result interface_impl::async_invoke(std::string_view method_name, const mc::variants& args) {
    auto* method = get_method_info(method_name);
    if (method == nullptr) {
        return {};
    }

    auto* ctx = context_stack::top_value();
    if (ctx) {
        ctx->set_method(method);
    }
    return method->async_invoke(this, args);
}

mc::variant interface_impl::get_property(std::string_view property_name, int options) const {
    auto* info = get_property_info(property_name);
    if (info == nullptr) {
        return {};
    }

    if (info->has_flags(MC_REFLECT_FLAG_PROPERTY_TPL)) {
        return detail::to_property_base(this, info)->get_value(options);
    } else {
        return info->get_value(this);
    }
}

mc::dict interface_impl::get_all_properties(int options) const {
    mc::mutable_dict dict;
    get_metadata().visit_properties([&](const property_type_info* property) {
        if (dict.contains(property->name)) {
            return visit_status::VS_CONTINUE;
        }

        if (property->has_flags(MC_REFLECT_FLAG_PROPERTY_TPL)) {
            dict[property->name] = detail::to_property_base(this, property)->get_value(options);
        } else {
            dict[property->name] = property->get_value(this);
        }
        return visit_status::VS_CONTINUE;
    });
    return dict;
}

void interface_impl::from_variant(const mc::dict& d, interface_impl& obj) {
    std::unordered_set<std::string_view> visited;
    obj.get_metadata().visit_properties([&](const property_type_info* property) {
        if (!d.contains(property->name) || visited.find(property->name) != visited.end()) {
            return visit_status::VS_CONTINUE;
        }

        if (property->has_flags(MC_REFLECT_FLAG_PROPERTY_TPL)) {
            detail::to_property_base(&obj, property)->set_value(d[property->name]);
        } else {
            property->set_value(&obj, d[property->name]);
        }

        visited.insert(property->name); // 标记为已访问，基类同名属性不再使用
        return visit_status::VS_CONTINUE;
    });
}

void interface_impl::to_variant(const interface_impl& obj, mc::mutable_dict& dict, int options) {
    obj.get_metadata().visit_properties([&](const property_type_info* property) {
        if (dict.contains(property->name)) {
            return visit_status::VS_CONTINUE;
        }

        if (property->has_flags(MC_REFLECT_FLAG_PROPERTY_TPL)) {
            dict[property->name] = detail::to_property_base(&obj, property)->get_value(options);
        } else {
            dict[property->name] = property->get_value(&obj);
        }
        return visit_status::VS_CONTINUE;
    });
}

void interface_impl::init_property_base(const struct_metadata& metadata) {
    auto* self = static_cast<interface_impl*>(this);
    metadata.visit_properties([self](const property_type_info* info) {
        if (info->has_flags(MC_REFLECT_FLAG_PROPERTY_TPL)) {
            // TODO :: 这里要特别注意，property<T> 的 offset 是相对于 interface_impl 的偏移，我们统一用
            // property<int32_t> 代替是因为 observer 是 property<T> 的基类，所有类型都兼容，后续 property<T>
            // 修改也报保证兼容性
            auto* p_base = MC_MEMBER_PTR(property<int32_t>*, self, info->offset());
            p_base->get_observer().set_interface(self);
        }
        return visit_status::VS_CONTINUE;
    });
}

} // namespace mc::engine
