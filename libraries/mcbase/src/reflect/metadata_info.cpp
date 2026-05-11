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

#include <mc/reflect/metadata_info.h>

namespace mc::reflect {

mc::variant property_type_info::get_value(const void* obj) const
{
    return get_value_erased(obj);
}

void property_type_info::set_value(void* obj, const mc::variant& value) const
{
    set_value_erased(obj, value);
}

property_info::property_info(mc::string_view n, uint32_t relative_offset, get_value_func_t getter,
                             set_value_func_t setter, std::type_index typeinfo, mc::string_view type_name,
                             mc::string_view signature)
    : property_type_info(n), m_relative_offset(relative_offset), m_get_value(getter), m_set_value(setter),
      m_typeinfo(typeinfo), m_type_name(type_name), m_signature(signature)
{
    if (!n.empty()) {
        this->name_quark = mc::quark{mc::detail::intern_trusted_literal(n)};
    }
}

mc::variant property_info::get_value_erased(const void* obj) const
{
    return m_get_value(get_member_ptr(obj));
}

void property_info::set_value_erased(void* obj, const mc::variant& value) const
{
    m_set_value(get_member_ptr(obj), value);
}

std::type_index property_info::typeinfo() const
{
    return m_typeinfo;
}

mc::string_view property_info::type_name() const noexcept
{
    return m_type_name;
}

mc::string_view property_info::get_signature() const
{
    return m_signature;
}

int property_info::type() const noexcept
{
    return static_cast<int>(member_info_type::property);
}

uint32_t property_info::offset() const noexcept
{
    return this->base_offset + m_relative_offset;
}

member_info_base* property_info::clone() const
{
    return new property_info(*this);
}

const void* property_info::get_member_ptr(const void* obj) const noexcept
{
    return reinterpret_cast<const char*>(obj) + offset();
}

void* property_info::get_member_ptr(void* obj) const noexcept
{
    return reinterpret_cast<char*>(obj) + offset();
}

mc::variant method_type_info::invoke(void* obj, const mc::variants& args) const
{
    return invoke_erased(obj, args);
}

async_result method_type_info::async_invoke(void* obj, const mc::variants& args) const
{
    return async_invoke_erased(obj, args);
}

method_info::method_info(mc::string_view n, uint32_t relative_offset, uint32_t function_size, uint32_t method_arg_count,
                         bool is_static_method, invoke_func_t invoke_func, async_invoke_func_t async_invoke_func,
                         invoke_static_func_t invoke_static_func, async_invoke_static_func_t async_invoke_static_func,
                         invoke_raw_func_t invoke_raw_func, async_invoke_raw_func_t async_invoke_raw_func,
                         invoke_static_raw_func_t       invoke_static_raw_func,
                         async_invoke_static_raw_func_t async_invoke_static_raw_func, std::type_index typeinfo,
                         mc::string_view type_name, mc::string_view args_signature,
                         mc::string_view full_args_signature, mc::string_view result_signature,
                         bool has_explicit_context_arg)
    : method_type_info(n), m_relative_offset(relative_offset), m_function_size(function_size),
      m_arg_count(method_arg_count), m_is_static(is_static_method), m_invoke(invoke_func),
      m_async_invoke(async_invoke_func), m_invoke_static(invoke_static_func),
      m_async_invoke_static(async_invoke_static_func), m_invoke_raw(invoke_raw_func),
      m_async_invoke_raw(async_invoke_raw_func), m_invoke_static_raw(invoke_static_raw_func),
      m_async_invoke_static_raw(async_invoke_static_raw_func), m_typeinfo(typeinfo), m_type_name(type_name),
      m_args_signature(args_signature), m_full_args_signature(full_args_signature),
      m_result_signature(result_signature), m_has_explicit_context_arg(has_explicit_context_arg)
{
    if (!n.empty()) {
        this->name_quark = mc::quark{mc::detail::intern_trusted_literal(n)};
    }
}

method_info* method_info::create(mc::string_view n, uint32_t relative_offset, uint32_t function_size,
                                 uint32_t method_arg_count, bool is_static_method, invoke_func_t invoke_func,
                                 async_invoke_func_t async_invoke_func, invoke_static_func_t invoke_static_func,
                                 async_invoke_static_func_t async_invoke_static_func, invoke_raw_func_t invoke_raw_func,
                                 async_invoke_raw_func_t        async_invoke_raw_func,
                                 invoke_static_raw_func_t       invoke_static_raw_func,
                                 async_invoke_static_raw_func_t async_invoke_static_raw_func, std::type_index typeinfo,
                                 mc::string_view type_name, mc::string_view args_signature,
                                 mc::string_view full_args_signature, mc::string_view result_signature,
                                 bool has_explicit_context_arg, const void* function_data)
{
    auto* storage = static_cast<char*>(operator new(sizeof(method_info) + function_size));
    auto* method  = new (storage) method_info(
        n, relative_offset, function_size, method_arg_count, is_static_method, invoke_func, async_invoke_func,
        invoke_static_func, async_invoke_static_func, invoke_raw_func, async_invoke_raw_func, invoke_static_raw_func,
        async_invoke_static_raw_func, typeinfo, type_name, args_signature, full_args_signature, result_signature,
        has_explicit_context_arg);
    std::memcpy(method->function_storage(), function_data, function_size);
    return method;
}

bool method_info::is_static() const
{
    return m_is_static;
}

mc::variant method_info::invoke_erased(void* obj, const mc::variants& args) const
{
    if (m_is_static) {
        return m_invoke_static(*this, args);
    }
    return m_invoke(*this, obj, args);
}

async_result method_info::async_invoke_erased(void* obj, const mc::variants& args) const
{
    try {
        if (m_async_invoke) {
            return m_async_invoke(*this, obj, args);
        }
        return invoke_erased(obj, args);
    } catch (...) {
        return mc::reject<async_result>(std::current_exception());
    }
}

mc::variant method_info::invoke_static_erased(const mc::variants& args) const
{
    return m_invoke_static(*this, args);
}

async_result method_info::async_invoke_static_erased(const mc::variants& args) const
{
    try {
        if (m_async_invoke_static) {
            return m_async_invoke_static(*this, args);
        }
        return invoke_static_erased(args);
    } catch (...) {
        return mc::reject<async_result>(std::current_exception());
    }
}

std::type_index method_info::typeinfo() const
{
    return m_typeinfo;
}

mc::string_view method_info::type_name() const noexcept
{
    return m_type_name;
}

size_t method_info::arg_count() const
{
    return m_arg_count;
}

mc::string_view method_info::get_args_signature() const
{
    return m_args_signature;
}

mc::string_view method_info::get_full_args_signature() const
{
    return m_full_args_signature;
}

mc::string_view method_info::get_result_signature() const
{
    return m_result_signature;
}

bool method_info::has_explicit_context_arg() const
{
    return m_has_explicit_context_arg;
}

uint32_t method_info::offset() const noexcept
{
    return m_relative_offset;
}

int method_info::type() const noexcept
{
    return static_cast<int>(member_info_type::method);
}

member_info_base* method_info::clone() const
{
    auto* storage = static_cast<char*>(operator new(sizeof(method_info) + m_function_size));
    auto* method  = new (storage) method_info(*this);
    std::memcpy(method->function_storage(), function_storage(), m_function_size);
    return method;
}

const void* method_info::function_storage() const noexcept
{
    return reinterpret_cast<const char*>(this) + sizeof(method_info);
}

void* method_info::function_storage() noexcept
{
    return reinterpret_cast<char*>(this) + sizeof(method_info);
}

mc::variant method_info::invoke_raw(void* obj, const void* converted_args) const
{
    return m_invoke_raw(*this, obj, converted_args);
}

mc::variant method_info::invoke_static_raw(const void* converted_args) const
{
    return m_invoke_static_raw(*this, converted_args);
}

async_result method_info::async_invoke_raw(void* obj, const void* converted_args) const
{
    return m_async_invoke_raw(*this, obj, converted_args);
}

async_result method_info::async_invoke_static_raw(const void* converted_args) const
{
    return m_async_invoke_static_raw(*this, converted_args);
}

// 类型擦除后通过反射获取基类属性值和调用基类方法，用 std::monostate 类型作为对象类型，
// 因为这里只需要复用基类偏移后的调用路径。
mc::variant base_class_type_info::get_value(void* obj, mc::string_view name) const
{
    return reinterpret_cast<const base_class_info_base<std::monostate>*>(this)->get_value(
        *static_cast<std::monostate*>(obj), name);
}

void base_class_type_info::set_value(void* obj, mc::string_view name, const mc::variant& value) const
{
    reinterpret_cast<const base_class_info_base<std::monostate>*>(this)->set_value(*static_cast<std::monostate*>(obj),
                                                                                   name, value);
}

mc::variant base_class_type_info::invoke(void* obj, mc::string_view name, const mc::variants& args) const
{
    return reinterpret_cast<const base_class_info_base<std::monostate>*>(this)->invoke(
        *static_cast<std::monostate*>(obj), name, args);
}

async_result base_class_type_info::async_invoke(void* obj, mc::string_view name, const mc::variants& args) const
{
    return reinterpret_cast<const base_class_info_base<std::monostate>*>(this)->async_invoke(
        *static_cast<std::monostate*>(obj), name, args);
}

} // namespace mc::reflect
