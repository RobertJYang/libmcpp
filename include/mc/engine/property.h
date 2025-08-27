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

// 属性类型枚举
enum class p_type : uint32_t {
    normal     = 0, // 普通属性
    sync       = 1, // 同步属性
    reference  = 2, // 引用属性
    ref_object = 3  // 引用对象属性
};

// 定义常量以便与 int 类型兼容
namespace property_options {
constexpr int memory   = 1;
constexpr int from_mdb = 2;
} // namespace property_options

namespace detail {

struct empty_observer {
    void notify(const mc::variant& value, const property_base& prop) {
    }
};

// 用于存储函数调用相关的数据
struct func_data {
    mc::expr::func   func_obj;
    mc::mutable_dict params;
};

class interface_observer {
public:
    interface_observer() {
    }

    void set_interface(abstract_interface* interface) {
        m_interface = interface;
    }

    abstract_interface* get_interface() const {
        return m_interface;
    }

    void notify(const mc::variant& value, const property_base& prop);

protected:
    // 不要初始化这个值，由 interface 的基类填充
    abstract_interface* m_interface;
};

} // namespace detail

// 引用对象类，实现弱引用语义
class ref_object : public variant_extension_base {
public:
    using object_finder_type = std::function<abstract_object*(const std::string&)>;

    ref_object(const std::string& object_name, object_finder_type finder = nullptr)
        : m_object_name(object_name), m_object_finder(finder) {
    }

    // 获取被引用对象的属性
    mc::variant get_property(const std::string_view property_name) const {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在: ${object_name}", ("object_name", m_object_name));
        }
        return target_object->get_property(property_name);
    }

    // 获取被引用对象的接口属性
    mc::variant get_property(const std::string_view interface_name, const std::string_view property_name) const {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在: ${object_name}", ("object_name", m_object_name));
        }

        if (!interface_name.empty()) {
            auto interface_obj = target_object->get_interface(interface_name);
            if (interface_obj == nullptr) {
                MC_THROW(mc::invalid_op_exception, "Interface not found: ${interface} in object: ${object_name}",
                         ("interface", interface_name)("object_name", m_object_name));
            }
            return interface_obj->get_property(property_name);
        }

        return target_object->get_property(property_name);
    }

    // 设置被引用对象的属性
    void set_property(const std::string_view property_name, const mc::variant& value) const {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在，无法设置属性: ${object_name}", ("object_name", m_object_name));
        }
        target_object->set_property(property_name, value);
    }

    // 设置被引用对象的接口属性
    void set_property(const std::string_view interface_name, const std::string_view property_name, const mc::variant& value) const {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在，无法设置属性: ${object_name}", ("object_name", m_object_name));
        }

        if (!interface_name.empty()) {
            auto interface_obj = target_object->get_interface(interface_name);
            if (interface_obj == nullptr) {
                MC_THROW(mc::invalid_op_exception, "Interface not found: ${interface} in object: ${object_name}",
                         ("interface", interface_name)("object_name", m_object_name));
            }
            interface_obj->set_property(property_name, value);
        } else {
            target_object->set_property(property_name, value);
        }
    }

    invoke_result invoke(std::string_view method_name, const mc::variants& args) {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在: ${object_name}", ("object_name", m_object_name));
        }

        return target_object->invoke(method_name, args);
    }

    invoke_result invoke(const std::string& interface_name, std::string_view method_name, const mc::variants& args) {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在: ${object_name}", ("object_name", m_object_name));
        }

        if (!interface_name.empty()) {
            auto interface_obj = target_object->get_interface(interface_name);
            if (interface_obj == nullptr) {
                MC_THROW(mc::invalid_op_exception, "Interface not found: ${interface} in object: ${object_name}",
                         ("interface", interface_name)("object_name", m_object_name));
            }
            return interface_obj->invoke(method_name, args);
        }

        return target_object->invoke(method_name, args);
    }

    async_result async_invoke(std::string_view method_name, const mc::variants& args = {}) {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在: ${object_name}", ("object_name", m_object_name));
        }

        return target_object->async_invoke(method_name, args);
    }

    async_result async_invoke(const std::string& interface_name, std::string_view method_name, const mc::variants& args = {}) {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在: ${object_name}", ("object_name", m_object_name));
        }

        if (!interface_name.empty()) {
            auto interface_obj = target_object->get_interface(interface_name);
            if (interface_obj == nullptr) {
                MC_THROW(mc::invalid_op_exception, "Interface not found: ${interface} in object: ${object_name}",
                         ("interface", interface_name)("object_name", m_object_name));
            }
            return interface_obj->async_invoke(method_name, args);
        }

        return target_object->async_invoke(method_name, args);
    }

    // 获取对象名称
    const std::string& get_object_name() const {
        return m_object_name;
    }

    // 检查被引用的对象是否存在
    bool is_valid() const {
        return find_related_object() != nullptr;
    }

    // 获取被引用的对象指针（可能为空）
    abstract_object* get_object() const {
        return find_related_object();
    }

    std::string as_string() const override {
        return m_object_name;
    }

    bool equals(const variant_extension_base& other) const override {
        if (auto* other_ref = dynamic_cast<const ref_object*>(&other)) {
            return m_object_name == other_ref->m_object_name;
        }
        return false;
    }

    // 实现 variant_extension_base 的纯虚函数
    mc::shared_ptr<variant_extension_base> clone() const override {
        return mc::make_shared<ref_object>(m_object_name, m_object_finder);
    }

    std::string_view get_type_name() const override {
        return "ref_object";
    }

private:
    std::string        m_object_name;
    object_finder_type m_object_finder;

    // 查找被引用的对象（弱引用，可能返回 nullptr）
    abstract_object* find_related_object() const {
        if (m_object_finder) {
            return m_object_finder(m_object_name);
        }
        return nullptr;
    }
};

template <typename T, typename Observer = detail::interface_observer>
class property : public property_base {
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
        if (m_outsider_getter && realtime) {
            const_cast<property_type*>(this)->update_value();
        }

        // 如果是引用对象类型，m_value 已经存储了转换后的结果
        if (m_property_type == p_type::ref_object) {
            // 确保缓存已初始化（这会同时更新 m_value）
            ensure_ref_object_cache();
            return m_value;
        }

        if (m_getter && realtime) {
            const_cast<property_type*>(this)->update_value();
        }
        return m_value;
    }
    void set_value(param_type new_value) {
        if (m_setter) {
            m_setter(new_value);
        }
        set_value_impl(new_value);
    }
    void set_value(rvalue_type new_value) {
        if (m_setter) {
            m_setter(new_value);
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
        m_outsider_getter = std::move(outsider_getter);
        m_outsider_setter = std::move(outsider_setter);
    }

    observer_type& observer() {
        return m_observer;
    }

    const observer_type& observer() const {
        return m_observer;
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
        if (m_property_type == p_type::ref_object && m_ref_object_cache) {
            // 从缓存的 ref_object 获取对象名称
            if (m_ref_object_cache->is_extension()) {
                auto ref_obj_ptr = m_ref_object_cache->as<ref_object*>();
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
        if (!m_ref_object_cache) {
            const_cast<property_type*>(this)->initialize_ref_object_cache();
        }
        return *m_ref_object_cache;
    }

    // 处理引用对象的 from_variant 逻辑
    void process_ref_object_from_variant(const std::string& ref_object_str) {
        auto ref_obj    = func_parser::get_instance().parse_ref_object(ref_object_str);
        m_property_type = p_type::ref_object;

        // 清理旧的缓存（如果有的话）
        m_ref_object_cache.reset();

        // 暂时存储对象名称到属性值中（懒加载，等到需要时再转换）
        if constexpr (std::is_convertible_v<T, std::string>) {
            m_value = static_cast<T>(ref_obj.object_name);
        } else {
            from_variant(mc::variant(ref_obj.object_name), m_value);
        }

        m_setter = [this](const T& value) {
            MC_THROW(mc::invalid_op_exception, "设置引用对象属性值不被允许: ${name}", ("name", get_name()));
        };
    }

public:
    // 获取引用对象的variant包装器（现在直接使用缓存）
    mc::variant get_ref_object_variant() const {
        return ensure_ref_object_cache();
    }

    // 查找对象的辅助方法
    abstract_object* find_related_object(const std::string& object_name) {
        auto position = get_object()->get_position();
        auto service  = func_collection::get_instance().get_service(position);
        if (service == nullptr) {
            elog("Service not found for position: ${position}",
                 ("position", std::string(position)));
            return nullptr;
        }

        std::string full_object_name = object_name + "_" + std::string(position);

        auto& object_table = service->get_object_table();
        auto& idx          = object_table.template get<mc::engine::by_object_name>();
        auto  obj_it       = idx.find(full_object_name);

        if (obj_it == idx.end()) {
            obj_it = idx.find(full_object_name + "_dev");
        }

        if (obj_it == idx.end()) {
            elog("Object not found: ${object_name}, searched for: ${full_name}",
                 ("object_name", object_name)("full_name", full_object_name));
            return nullptr;
        }

        return const_cast<mc::engine::abstract_object*>(&(*obj_it));
    }

    // 初始化引用对象缓存（只在属性设置时调用一次）
    void initialize_ref_object_cache() {
        if (m_property_type != p_type::ref_object) {
            return;
        }

        // 分配缓存内存
        if (!m_ref_object_cache) {
            m_ref_object_cache = std::make_unique<mc::variant>();

            // 获取对象名称（从当前 m_value 中获取，因为这时候还未改变 m_value）
            std::string object_name;
            if constexpr (std::is_convertible_v<T, std::string>) {
                object_name = static_cast<std::string>(m_value);
            } else {
                mc::variant temp_variant(m_value);
                if (temp_variant.is_string()) {
                    object_name = temp_variant.as_string();
                } else {
                    MC_THROW(mc::invalid_op_exception, "引用对象属性值不是字符串类型: ${name}", ("name", get_name()));
                }
            }

            // 创建弱引用对象包装器
            auto object_finder = [this](const std::string& name) -> abstract_object* {
                return const_cast<property_type*>(this)->find_related_object(name);
            };
            auto ref_obj = std::make_shared<ref_object>(object_name, object_finder);

            // 在缓存中存储 ref_object
            *m_ref_object_cache = mc::variant(ref_obj);

            // 将转换后的结果存储到 m_value
            if constexpr (std::is_same_v<T, mc::variant>) {
                const_cast<property_type*>(this)->m_value = mc::variant(ref_obj);
            } else {
                // 对于非 variant 类型，尝试转换并存储
                try {
                    from_variant(mc::variant(ref_obj), const_cast<property_type*>(this)->m_value);
                } catch (const std::exception&) {
                    // 转换失败时使用默认值
                    const_cast<property_type*>(this)->m_value = T{};
                }
            }
        }
    }

    // 执行函数调用的辅助方法
    mc::variant call_function_with_result() {
        if (!m_func_data) {
            elog("Function data is null for property: ${name}", ("name", get_name()));
            return mc::variant();
        }

        try {
            auto position = get_object()->get_position();
            auto result   = m_func_data->func_obj.call(position, m_func_data->params);
            if (result.is_null()) {
                elog("Function call failed for property: ${name}", ("name", get_name()));
            }
            return result;
        } catch (const std::exception& e) {
            elog("Function call exception for property: ${name}, 错误: ${error}",
                 ("name", get_name())("error", e.what()));
            return mc::variant();
        }
    }

    // 设置函数getter的辅助方法
    void setup_function_getter() {
        m_getter = [this]() -> T {
            auto result = call_function_with_result();
            if (result.is_null()) {
                return T{};
            }

            T value;
            from_variant(result, value);
            return value;
        };
    }

    void hook_ref_properties(mc::mutable_dict& relate_properties) {
        setup_function_getter();

        // 引用属性不支持设置值
        m_setter = [this](const T& value) {
            MC_THROW(mc::invalid_op_exception, "设置引用属性值不被允许: ${name}", ("name", get_name()));
        };
    }

    mc::variant get_relate_property(const relate_property& relate_property) {
        auto* target_object = find_related_object(relate_property.object_name);
        if (target_object == nullptr) {
            return mc::variant();
        }

        // 如果指定了接口，先获取接口再获取属性
        if (!relate_property.interface.empty()) {
            auto interface_obj = target_object->get_interface(relate_property.interface);
            if (interface_obj == nullptr) {
                elog("Interface not found: ${interface} in object: ${object_name}",
                     ("interface", relate_property.interface)("object_name", relate_property.object_name));
                return mc::variant();
            }
            return interface_obj->get_property(relate_property.property_name);
        } else {
            // 传统方式：直接从对象获取属性
            return target_object->get_property(relate_property.property_name);
        }
    }

    void set_relate_property(const relate_property& relate_property, const mc::variant& value) {
        auto* target_object = find_related_object(relate_property.object_name);
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "set_relate_property ${name} failed: Object not found: ${object_name}",
                     ("name", get_name())("object_name", relate_property.object_name));
        }

        // 如果指定了接口，先获取接口再设置属性
        if (!relate_property.interface.empty()) {
            auto interface_obj = target_object->get_interface(relate_property.interface);
            if (interface_obj == nullptr) {
                MC_THROW(mc::invalid_op_exception, "set_relate_property ${name} failed: Interface not found: ${interface} in object: ${object_name}",
                         ("name", get_name())("interface", relate_property.interface)("object_name", relate_property.object_name));
            }
            interface_obj->set_property(relate_property.property_name, value);
        } else {
            // 传统方式：直接在对象上设置属性
            target_object->set_property(relate_property.property_name, value);
        }
    }

    void hook_ref_property(const relate_property& relate_property) {
        m_getter = [this, relate_property]() -> T {
            auto result = get_relate_property(relate_property);
            if (result.is_null()) {
                MC_THROW(mc::invalid_op_exception, "获取引用属性${name}失败: ${object_name}.${property_name}",
                         ("name", get_name())("object_name", relate_property.object_name)("property_name", relate_property.property_name));
            }

            T value;
            from_variant(result, value);
            return value;
        };

        m_setter = [this, relate_property](const T& value) {
            mc::variant variant_value(value);
            set_relate_property(relate_property, variant_value);
        };
    }

    // 连接属性变化监听器的辅助方法
    void connect_property_listener(abstract_object&      target_object,
                                   const std::string&    property_name,
                                   std::function<void()> callback) {
        target_object.property_changed().connect(
            [this, property_name, callback](const mc::variant& value, const property_base& prop) {
            if (prop.get_name() == property_name) {
                callback();
            }
        });
    }

    // 断开所有已有的连接
    void disconnect_all_connections() {
        // 主动断开所有连接
        for (auto& slot : m_connection_slots) {
            slot.disconnect();
        }
        // 清理现有的连接状态
        m_connection_slots.clear();
    }

    // 设置同步属性连接的辅助方法
    void setup_sync_property_connection(const relate_property& relate_property,
                                        abstract_object&       target_object) {
        // 断开旧连接
        disconnect_all_connections();

        // 创建新的连接
        auto slot = target_object.property_changed().connect(
            [this, relate_property](const mc::variant& value, const property_base& prop) {
            if (prop.get_name() == relate_property.property_name) {
                auto result = get_relate_property(relate_property);
                if (!result.is_null()) {
                    T new_value;
                    from_variant(result, new_value);
                    set_value_impl(std::move(new_value));
                }
            }
        });

        // 存储连接以便后续管理
        m_connection_slots.push_back(slot);

        // 设置初始值
        auto initial_value = target_object.get_property(relate_property.property_name);
        if (!initial_value.is_null()) {
            T value;
            from_variant(initial_value, value);
            set_value_impl(std::move(value));
        }
    }

    // 设置延迟同步连接的辅助方法
    void setup_deferred_sync_connection(const relate_property& relate_property) {
        auto position = get_object()->get_position();
        auto service  = func_collection::get_instance().get_service(position);
        if (service == nullptr) {
            return;
        }

        std::string full_object_name = relate_property.object_name + "_" + std::string(position);
        auto&       object_table     = service->get_object_table();

        auto slot = object_table.on_object_added.connect(
            [this, full_object_name, relate_property](mc::core::object_base& base_object) {
            auto& object = static_cast<mc::engine::abstract_object&>(base_object);
            if (object.get_name() == full_object_name) {
                setup_sync_property_connection(relate_property, object);
            }
        });

        // 存储延迟连接
        m_connection_slots.push_back(slot);
    }

    void hook_sync_property(const relate_property& relate_property) {
        auto* target_object = find_related_object(relate_property.object_name);
        if (target_object == nullptr) {
            setup_deferred_sync_connection(relate_property);
        } else {
            setup_sync_property_connection(relate_property, *target_object);
        }

        // 同步属性不支持设置值
        m_setter = [this](const T& value) {
            MC_THROW(mc::invalid_op_exception, "设置同步属性值不被允许: ${name}", ("name", get_name()));
        };
    }

    // 更新同步属性值的辅助方法
    void update_sync_value_from_function() {
        auto result = call_function_with_result();
        if (result.is_null()) {
            return;
        }

        T value;
        from_variant(result, value);
        set_value_impl(std::move(value));
    }

    // 按对象分组属性的辅助方法
    mc::mutable_dict group_properties_by_object(const mc::mutable_dict& relate_properties) {
        mc::mutable_dict object_properties;

        for (const auto& entry : relate_properties) {
            auto relate_property = entry.value.template as<mc::expr::relate_property>();

            if (!object_properties.contains(relate_property.object_name)) {
                object_properties[relate_property.object_name] = mc::mutable_dict{};
            }

            auto object_property = object_properties[relate_property.object_name].as<mc::mutable_dict>();
            object_property.insert(relate_property.property_name, true);
            object_properties[relate_property.object_name] = object_property;
        }

        return object_properties;
    }

    // 处理单个对象的同步属性
    void process_sync_properties_for_object(const std::string&      object_name,
                                            const mc::mutable_dict& object_properties) {
        auto* target_object = find_related_object(object_name);
        if (target_object == nullptr) {
            setup_deferred_multi_sync_connection(object_name, object_properties);
            return;
        }

        setup_multi_sync_connection(*target_object, object_properties);
    }

    // 设置多属性同步连接
    void setup_multi_sync_connection(abstract_object&        target_object,
                                     const mc::mutable_dict& object_properties) {
        auto slot = target_object.property_changed().connect(
            [this, object_properties](const mc::variant& value, const property_base& prop) {
            if (object_properties.contains(prop.get_name())) {
                update_sync_value_from_function();
            }
        });

        // 存储连接
        m_connection_slots.push_back(slot);
    }

    // 设置延迟多属性同步连接
    void setup_deferred_multi_sync_connection(const std::string&      object_name,
                                              const mc::mutable_dict& object_properties) {
        auto position = get_object()->get_position();
        auto service  = func_collection::get_instance().get_service(position);
        if (service == nullptr) {
            return;
        }

        std::string full_object_name = object_name + "_" + std::string(position);
        auto&       object_table     = service->get_object_table();

        auto slot = object_table.on_object_added.connect(
            [this, full_object_name, object_properties](mc::core::object_base& base_object) {
            auto& object = static_cast<mc::engine::abstract_object&>(base_object);
            if (object.get_name() == full_object_name) {
                setup_multi_sync_connection(object, object_properties);
                update_sync_value_from_function();
            }
        });

        // 存储延迟连接
        m_connection_slots.push_back(slot);
    }

    void hook_sync_properties(mc::mutable_dict& relate_properties) {
        // 断开旧连接
        disconnect_all_connections();

        auto grouped_properties = group_properties_by_object(relate_properties);

        for (const auto& entry : grouped_properties) {
            auto object_name       = entry.key.template as<std::string>();
            auto object_properties = entry.value.template as<mc::mutable_dict>();
            process_sync_properties_for_object(object_name, object_properties);
        }

        // 所有监听器设置完毕后，统一进行一次初始化
        update_sync_value_from_function();

        // 同步属性不支持设置值
        m_setter = [](const T& value) {
            MC_THROW(mc::invalid_op_exception, "设置同步属性值不被允许");
        };
    }

    void process_property_value(const std::string& value_str) {
        auto func_info = func_parser::get_instance().parse_function_call(value_str);
        auto position  = get_object()->get_position();
        auto call_func = func_collection::get_instance().get(position, func_info.func);

        if (call_func.is_null()) {
            elog("函数未找到: ${name}", ("name", func_info.func));
            // 函数未找到时，不修改属性值，保持原有值
            return;
        }

        auto func_params       = func_info.params;
        auto relate_properties = call_func.template as<func>().get_relate_properties(
            get_object()->get_position(), func_params);

        if (relate_properties.empty()) {
            // 没有关联属性，只计算一次，不需要保存函数对象
            auto result = call_func.template as<func>().call(position, func_params);
            if (result.is_null()) {
                return;
            }
            T value;
            from_variant(result, value);
            set_value_impl(std::move(value));
            return;
        }

        // 分配func_data，保存函数对象
        m_func_data = std::make_unique<detail::func_data>();
        mc::from_variant(call_func, m_func_data->func_obj); // 正确转换到func类型
        m_func_data->params = func_params;

        // 处理第一个关联属性的类型
        for (const auto& entry : relate_properties) {
            auto value = entry.value.template as<mc::expr::relate_property>();
            if (value.type == "ref") {
                hook_ref_properties(relate_properties);
                m_property_type = p_type::reference;
            } else if (value.type == "sync") {
                hook_sync_properties(relate_properties);
                m_property_type = p_type::sync;
            }
            break;
        }
    }

    friend inline void from_variant(const mc::variant& v, property_type& value) {
        if (!v.is_string()) {
            from_variant(v, value.m_value);
            return;
        }

        auto str = v.as<std::string>();

        if (str.substr(0, 3) == "<=/") {
            auto sync_prop = func_parser::get_instance().parse_sync_property(str);
            value.hook_sync_property(sync_prop);
            value.m_property_type = p_type::sync;
        } else if (str.substr(0, 2) == "#/") {
            // 根据是否包含点号来区分引用对象和引用属性
            if (str.find('.') != std::string::npos) {
                auto ref_prop = func_parser::get_instance().parse_ref_property(str);
                value.hook_ref_property(ref_prop);
                value.m_property_type = p_type::reference;
            } else {
                // 处理引用对象
                value.process_ref_object_from_variant(str);
            }
        } else if (str.substr(0, 6) == "$Func_") {
            value.process_property_value(str);
        } else {
            from_variant(v, value.m_value);
        }
    }

    observer_type& get_observer() {
        return m_observer;
    }

    const observer_type& get_observer() const {
        return m_observer;
    }

    std::string_view get_name() const override {
        if constexpr (std::is_same_v<observer_type, detail::interface_observer>) {
            return m_observer.get_interface()->get_property_name(this);
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

    abstract_interface* get_interface() const override {
        if constexpr (std::is_same_v<observer_type, detail::interface_observer>) {
            return m_observer.get_interface();
        }

        return nullptr;
    }

    abstract_object* get_object() const override {
        if constexpr (std::is_same_v<observer_type, detail::interface_observer>) {
            return m_observer.get_interface()->get_owner();
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
        set_value_impl(value.as<T>());
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

        m_observer.notify(value, *this);
    }

    void set_value_impl(param_type new_value, bool direct_set = false) {
        if (is_equal(new_value)) {
            return;
        }
        if (m_outsider_setter && !direct_set) {
            if (!m_outsider_setter(new_value)) {
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
        if (m_outsider_setter && !direct_set) {
            if (!m_outsider_setter(new_value)) {
                // 外部setter返回false，不进行实际设置
                return;
            }
        }
        m_value = std::move(new_value);
        notify();
    }

    void update_value() {
        if (m_getter) {
            set_value_impl(m_getter(), true);
        } else if (m_outsider_getter) {
            set_value_impl(m_outsider_getter(), true);
        }
    }

    T                                        m_value{};
    observer_type                            m_observer;
    std::unique_ptr<property_changed_signal> m_signal;

    std::function<T()>                   m_getter;
    std::function<void(const T&)>        m_setter;
    std::function<value_type()>          m_outsider_getter; // 外部getter方法
    std::function<bool(param_type)>      m_outsider_setter; // 外部setter方法，返回false表示不进行实际设置
    std::unique_ptr<detail::func_data>   m_func_data;       // 只有需要时才分配
    std::vector<mc::connection_type>     m_connection_slots;
    p_type                               m_property_type{p_type::normal}; // 属性类型，默认为普通属性
    mutable std::unique_ptr<mc::variant> m_ref_object_cache;              // 缓存引用对象的 variant
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