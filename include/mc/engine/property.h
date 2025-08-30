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
#ifndef MC_ENGINE_PROPERTY_H
#define MC_ENGINE_PROPERTY_H

#include <functional>
#include <mc/engine/base.h>
#include <mc/engine/property/detail.h>
#include <mc/engine/property/helper.h>
#include <mc/engine/property/processor.h>
#include <mc/engine/property/processors.h>
#include <mc/engine/property/ref_object.h>
#include <mc/engine/property/types.h>
#include <mc/expr/function/call.h>
#include <mc/expr/function/collection.h>
#include <mc/expr/function/parser.h>
#include <mc/log.h>
#include <mc/reflect.h>
#include <mc/signal_slot.h>
#include <mc/traits.h>
#include <mc/variant.h>
#include <mc/variant/variant_extension.h>
#include <tuple>
#include <vector>

using namespace mc::expr;

namespace mc::engine {

template <typename T, typename Observer = detail::interface_observer>
class property : public property_helper, public Observer {
    static_assert(std::is_same_v<std::decay_t<T>, T>, "T must be a non-reference type");

public:
    using property_traits = mc::traits::property_traits<T>;

    using value_type    = typename property_traits::value_type;
    using param_type    = typename property_traits::param_type;
    using rvalue_type   = typename property_traits::rvalue_type;
    using observer_type = Observer;
    using property_type = property<T, Observer>;

    // 普通属性构造函数
    property() = default;

    template <typename U = T, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    explicit property(param_type value)
        : m_value(value) {
    }

    template <typename U = T, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    explicit property(rvalue_type value)
        : m_value(std::move(value)) {
    }

    // 赋值与取值
    property_type& operator=(rvalue_type new_value) {
        set_value(std::move(new_value));
        return *this;
    }
    property_type& operator=(param_type new_value) {
        set_value(new_value);
        return *this;
    }
    param_type value(bool realtime = false) const {
        // 如果设置了外部getter，优先调用外部getter
        if (has_extension_data() && m_extension_data->outsider_getter && realtime) {
            const_cast<property_type*>(this)->update_value();
        }

        // 如果是引用对象类型，m_value 已经存储了转换后的结果
        if (m_property_type == p_type::ref_object) {
            // 确保缓存已初始化（这会同时更新 m_value）
            ensure_ref_object_cache();
            return m_value;
        }

        if (has_extension_data() && m_extension_data->getter && realtime) {
            const_cast<property_type*>(this)->update_value();
        }
        return m_value;
    }
    void set_value(param_type new_value) {
        if (has_extension_data() && m_extension_data->setter) {
            mc::variant variant_value(new_value);
            m_extension_data->setter(variant_value);
        }
        set_value_impl(new_value);
    }
    void set_value(rvalue_type new_value) {
        if (has_extension_data() && m_extension_data->setter) {
            mc::variant variant_value(new_value);
            m_extension_data->setter(variant_value);
        }
        set_value_impl(std::move(new_value));
    }

    mc::variant get_value(int options = 0) const override {
        if (m_property_type == p_type::ref_object) {
            if ((options & property_options::memory) || (options & property_options::from_mdb)) {
                // 返回对象名称字符串
                return mc::variant(get_ref_object_name());
            } else {
                // 返回缓存的引用对象
                return ensure_ref_object_cache();
            }
        }
        return value(!(options & property_options::memory));
    }

    param_type operator*() const {
        return value();
    }

    operator param_type() const {
        return value();
    }

    template <typename F>
    void modify(F&& func) {
        if (func(m_value)) {
            notify();
        }
    }

    // 设置外部getter和setter方法
    void make_outsider_getter_setter(std::function<value_type()>     outsider_getter,
                                     std::function<bool(param_type)> outsider_setter) {
        ensure_extension_data();
        // 包装getter函数，将返回值转换为variant
        if (outsider_getter) {
            m_extension_data->outsider_getter = [outsider_getter]() -> mc::variant {
                return mc::variant(outsider_getter());
            };
        }
        // 包装setter函数，将variant参数转换为具体类型
        if (outsider_setter) {
            m_extension_data->outsider_setter = [outsider_setter](const mc::variant& value) -> bool {
                T concrete_value;
                from_variant(value, concrete_value);
                return outsider_setter(concrete_value);
            };
        }
    }

    observer_type& get_observer() {
        return *this;
    }

    const observer_type& get_observer() const {
        return *this;
    }

    template <typename U>
    auto operator==(const U& v) const
        -> std::enable_if_t<!std::is_base_of_v<property_base, U>, bool> {
        return is_equal(v);
    }

    template <typename U>
    auto operator!=(const U& v) const
        -> std::enable_if_t<!std::is_base_of_v<property_base, U>, bool> {
        return !(*this == v);
    }

    template <typename U, typename UObserver>
    bool operator==(const property<U, UObserver>& other) const {
        return is_equal(other.value());
    }

    template <typename U, typename UObserver>
    bool operator!=(const property<U, UObserver>& other) const {
        return !(*this == other);
    }

    template <typename U>
    friend auto operator==(const U& v, const property_type& p)
        -> std::enable_if_t<!std::is_base_of_v<property_base, U>, bool> {
        return p == v;
    }

    template <typename U>
    friend auto operator!=(const U& v, const property_type& p)
        -> std::enable_if_t<!std::is_base_of_v<property_base, U>, bool> {
        return !(p == v);
    }

    friend inline void to_variant(const property_type& value, mc::variant& v) {
        to_variant(value.m_value, v);
    }

private:
    // 获取引用对象名称的统一方法
    std::string get_ref_object_name() const {
        if (m_property_type == p_type::ref_object && has_extension_data() && m_extension_data->ref_object_cache) {
            // 从缓存的 ref_object 获取对象名称
            if (m_extension_data->ref_object_cache->is_extension()) {
                auto ref_obj_ptr = m_extension_data->ref_object_cache->template as<ref_object*>();
                if (ref_obj_ptr) {
                    return ref_obj_ptr->get_object_name();
                }
            }
        }

        // 对于非引用对象类型或未初始化的情况，从 m_value 获取
        if constexpr (std::is_convertible_v<T, std::string>) {
            return static_cast<std::string>(m_value);
        } else {
            mc::variant temp_variant(m_value);
            if (temp_variant.is_string()) {
                return temp_variant.as_string();
            }
        }
        return "";
    }

    // 确保引用对象缓存已初始化，返回引用对象的 variant
    mc::variant& ensure_ref_object_cache() const {
        auto& ext_data = ensure_extension_data();
        if (!ext_data.ref_object_cache) {
            // 如果没有缓存，返回空的variant（处理器负责初始化）
            ext_data.ref_object_cache = std::make_unique<mc::variant>();
        }
        return *ext_data.ref_object_cache;
    }

public:
    friend inline void from_variant(const mc::variant& v, property_type& value) {
        if (!v.is_string()) {
            value.set_value_impl(v.as<T>());
            return;
        }

        auto str = v.as<std::string>();

        // 尝试使用处理器工厂处理特殊属性格式
        if (property_processor_factory::get_instance().process_property_value(&value, str)) {
            return;
        }

        // 如果没有匹配的处理器，按普通值处理
        from_variant(v, value.m_value);
    }

    std::string_view get_name() const override {
        if constexpr (std::is_same_v<observer_type, detail::interface_observer>) {
            auto* info = get_observer().get_interface()->get_property_info(this);
            if (info) {
                return info->name;
            }
        }

        return {};
    }

    std::string_view get_signature() const override {
        return mc::reflect::get_signature<T>();
    }

    uint32_t get_access() const override {
        return 0;
    }

    uint64_t get_flags() const override {
        return 0;
    }

    property_type get_property_type() const {
        return m_property_type;
    }

    uint32_t get_property_type_value() const {
        return static_cast<uint32_t>(m_property_type);
    }

    // property_helper的虚拟方法实现
    void set_property_type(p_type type) override {
        m_property_type = type;
    }

    p_type get_property_type_enum() const override {
        return m_property_type;
    }

    void set_variant_value(const mc::variant& value) override {
        T temp_value;
        from_variant(value, temp_value);
        set_value_impl(std::move(temp_value));
    }

    // 内部设置值的方法，绕过from_variant，供processor使用
    void set_internal_value(const mc::variant& value) override {
        // 直接设置m_value，完全绕过set_value_impl的通知机制，供processor初始化使用
        if constexpr (std::is_same_v<T, mc::variant>) {
            m_value = value;
        } else {
            try {
                m_value = value.as<T>();
            } catch (...) {
                // 如果转换失败，存储默认值，但不抛出异常
                m_value = T{};
            }
        }
    }

    mc::variant get_variant_value() const override {
        return mc::variant(m_value);
    }

    bool has_extension_data() const override {
        return m_extension_data != nullptr;
    }

    void ensure_extension_data() override {
        if (!m_extension_data) {
            m_extension_data = std::make_unique<detail::property_extension_data>();
        }
    }

    void set_ref_object_cache(std::unique_ptr<mc::variant> cache) override {
        ensure_extension_data();
        m_extension_data->ref_object_cache = std::move(cache);
    }

    mc::variant* get_ref_object_cache() const override {
        if (has_extension_data() && m_extension_data->ref_object_cache) {
            return m_extension_data->ref_object_cache.get();
        }
        return nullptr;
    }

    void set_getter_function(std::function<mc::variant()> getter) override {
        ensure_extension_data();
        m_extension_data->getter = getter;
    }

    void set_setter_function(std::function<void(const mc::variant&)> setter) override {
        ensure_extension_data();
        m_extension_data->setter = setter;
    }

    void add_connection_slot(mc::connection_type slot) override {
        ensure_extension_data();
        m_extension_data->connection_slots.push_back(slot);
    }

    void clear_connection_slots() override {
        if (has_extension_data()) {
            // 先断开所有连接
            for (auto& connection : m_extension_data->connection_slots) {
                connection.disconnect();
            }
            // 然后清空向量
            m_extension_data->connection_slots.clear();
        }
    }

    void set_function_data(std::unique_ptr<detail::func_data> data) override {
        ensure_extension_data();
        m_extension_data->function_data = std::move(data);
    }

    detail::func_data* get_function_data() const override {
        if (has_extension_data() && m_extension_data->function_data) {
            return m_extension_data->function_data.get();
        }
        return nullptr;
    }

    abstract_interface* get_interface() const override {
        if constexpr (std::is_same_v<observer_type, detail::interface_observer>) {
            return get_observer().get_interface();
        }

        return nullptr;
    }

    void set_interface(abstract_interface* interface) override {
        if constexpr (std::is_same_v<observer_type, detail::interface_observer>) {
            get_observer().set_interface(interface);
        } else {
            MC_UNUSED(interface);
        }
    }

    abstract_object* get_object() const override {
        if constexpr (std::is_same_v<observer_type, detail::interface_observer>) {
            return get_observer().get_interface()->get_owner();
        }

        return nullptr;
    }

    property_changed_signal& property_changed() override {
        if (m_signal == nullptr) {
            m_signal = std::make_unique<property_changed_signal>();
        }

        return *m_signal;
    }

protected:
    void set_variant(const mc::variant& value) override {
        value.as(*this);
    }

    template <typename U>
    bool is_equal(const U& v) const {
        if constexpr (mc::traits::has_operator_equal_v<T, U>) {
            if (v == this->m_value) {
                return true;
            }
        }
        return false;
    }

    void notify() {
        mc::variant value(m_value);
        if (m_signal) {
            (*m_signal)(value, *this);
            return;
        }

        get_observer().notify(value, *this);
    }

   void set_value_impl(param_type new_value, bool direct_set = false) {	
        if (is_equal(new_value)) {	
            return;	
        }	
        if (has_extension_data() && m_extension_data->outsider_setter && !direct_set) {	
            mc::variant variant_value(new_value);	
            if (!m_extension_data->outsider_setter(variant_value)) {
                // 外部setter返回false，不进行实际设置	
                return;	
            }	
        }	
        m_value = new_value;	
        notify();	
    }	

    void set_value_impl(rvalue_type new_value, bool direct_set = false) {	
        if (is_equal(new_value)) {	
            return;	
        }	
        if (has_extension_data() && m_extension_data->outsider_setter && !direct_set) {	
            mc::variant variant_value(new_value);	
            if (!m_extension_data->outsider_setter(variant_value)) {
                // 外部setter返回false，不进行实际设置	
                return;	
            }	
        }	
        m_value = std::move(new_value);	
        notify();	
    }	

    void update_value() {	
        if (has_extension_data()) {	
            if (m_extension_data->getter) {	
                auto result = m_extension_data->getter();	
                T    value;	
                from_variant(result, value);
                set_value_impl(value, true);
            } else if (m_extension_data->outsider_getter) {
                auto result = m_extension_data->outsider_getter();
                T    value;
                from_variant(result, value);
                set_value_impl(value, true);
            }
        }	
    }

    T                                        m_value{};
    std::unique_ptr<property_changed_signal> m_signal;
    p_type                                   m_property_type{p_type::normal}; // 属性类型，默认为普通属性

    // 将可选的功能数据延迟分配，大大减少内存占用
    mutable std::unique_ptr<detail::property_extension_data> m_extension_data;

private:
    // 确保扩展数据已分配的辅助方法
    detail::property_extension_data& ensure_extension_data() const {
        if (!m_extension_data) {
            m_extension_data = std::make_unique<detail::property_extension_data>();
        }
        return *m_extension_data;
    }
};

} // namespace mc::engine

namespace mc::reflect::detail {
template <typename T, typename Observer>
struct signature_helper<mc::engine::property<T, Observer>> {
    static void apply(std::string& sig) {
        sig += mc::reflect::get_signature<T>();
    }
};

} // namespace mc::reflect::detail

// 为 ref_object* 指针类型添加 from_variant 特化，支持 obj_variant.as<ref_object*>() 语法
namespace mc {
template <typename Config>
void from_variant(const mc::variant_base<Config>& var, mc::engine::ref_object*& ptr) {
    if (var.is_extension()) {
        auto ext_ptr = var.as_extension();
        if (ext_ptr) {
            ptr = dynamic_cast<mc::engine::ref_object*>(ext_ptr.get());
            if (ptr) {
                return;
            }
        }
    }
    ptr = nullptr;
}
} // namespace mc

#endif // MC_ENGINE_PROPERTY_H