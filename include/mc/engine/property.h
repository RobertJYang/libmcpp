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
#include <tuple>
#include <vector>

using namespace mc::expr;

namespace mc::engine {

namespace detail {

struct empty_observer {
    void notify(const mc::variant& value, const property_base& prop) {
    }
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

// 用于存储同步和引用属性的值缓存
template <typename... Types>
class property_cache {
public:
    using tuple_type = std::tuple<Types...>;

    template <size_t I>
    void set(const std::tuple_element_t<I, tuple_type>& value) {
        std::get<I>(m_values) = value;
    }

    template <size_t I>
    const std::tuple_element_t<I, tuple_type>& get() const {
        return std::get<I>(m_values);
    }

    const tuple_type& get_all() const {
        return m_values;
    }

private:
    tuple_type m_values;
};

} // namespace detail

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

    template <typename Getter, std::size_t... Is, typename... SyncProps>
    auto make_sync_getter(Getter&& getter, std::index_sequence<Is...>, SyncProps&... syncs) {
        auto* cache = static_cast<detail::property_cache<typename SyncProps::value_type...>*>(m_cache.get());
        (cache->template set<Is>(syncs.value()), ...);
        return [getter = std::forward<Getter>(getter), cache = cache]() -> T {
            return getter(cache->template get<Is>()...);
        };
    }

    // 同步属性构造函数
    template <typename Getter, typename... SyncProps,
              typename = std::enable_if_t<(sizeof...(SyncProps) > 0)>,
              typename = std::enable_if_t<std::is_invocable_r_v<T, Getter, const typename SyncProps::value_type&...>>>
    property(Getter&& getter, SyncProps&... syncs)
        : m_cache(new detail::property_cache<typename SyncProps::value_type...>(), [](void* p) {
              delete static_cast<detail::property_cache<typename SyncProps::value_type...>*>(p);
          }) {
        static_assert(sizeof...(SyncProps) <= 10, "At most 10 sync properties are allowed");

        // 添加同步源
        m_syncs = {&syncs...};

        // 包装getter
        m_getter = make_sync_getter(std::forward<Getter>(getter), std::index_sequence_for<SyncProps...>{}, syncs...);

        // 设置同步处理器
        setup_sync_handlers<SyncProps...>(std::index_sequence_for<SyncProps...>{}, syncs...);

        // 更新初始值
        update_value();
    }

    template <typename Getter, std::size_t... Is, typename... Props>
    auto make_ref_getter(Getter&& getter, std::index_sequence<Is...>, Props&... refs) {
        return [getter = std::forward<Getter>(getter), refs_ptrs = std::make_tuple(&refs...)]() -> T {
            return getter(*std::get<Is>(refs_ptrs)...);
        };
    }

    template <typename Setter, std::size_t... Is, typename... Props>
    auto make_ref_setter(Setter&& setter, std::index_sequence<Is...>, Props&... refs) {
        return [setter = std::forward<Setter>(setter), refs_ptrs = std::make_tuple(&refs...)](const T& v) {
            setter(v, *std::get<Is>(refs_ptrs)...);
        };
    }

    // 引用属性构造函数
    template <typename Getter, typename Setter, typename... Props,
              typename = std::enable_if_t<(sizeof...(Props) > 0)>,
              typename = std::enable_if_t<std::is_invocable_r_v<T, Getter, Props&...>>,
              typename = std::enable_if_t<std::is_same_v<Setter, std::nullptr_t> ||
                                          std::is_invocable_r_v<void, Setter, const T&, Props&...>>>
    property(Getter&& getter, Setter&& setter, Props&... refs) {
        static_assert(sizeof...(Props) <= 10, "At most 10 reference properties are allowed");

        // 添加引用源
        (m_refs.push_back(&refs), ...);

        // 包装 getter
        m_getter = make_ref_getter(std::forward<Getter>(getter), std::index_sequence_for<Props...>{}, refs...);

        // 包装 setter
        if constexpr (!std::is_same_v<std::decay_t<Setter>, std::nullptr_t>) {
            m_setter = make_ref_setter(std::forward<Setter>(setter), std::index_sequence_for<Props...>{}, refs...);
        }

        // 更新初始值
        update_value();
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

    mc::variant get_value() const override {
        return value();
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

    // 查找对象的辅助方法
    abstract_object* find_related_object(const std::string& object_name) {
        auto position = get_object()->get_position();
        auto service  = func_collection::get_instance().get_service(position);
        if (service == nullptr) {
            elog("Service not found for position: ${position}", ("position", std::string(position)));
            return nullptr;
        }

        std::string full_object_name = object_name + "_" + std::string(position);
        auto&       object_table     = service->get_object_table();
        auto&       idx              = object_table.template get<mc::engine::by_object_name>();
        auto        obj_it           = idx.find(full_object_name);

        if (obj_it == idx.end()) {
            elog("Object not found: ${object_name}", ("object_name", full_object_name));
            return nullptr;
        }

        return const_cast<mc::engine::abstract_object*>(&(*obj_it));
    }

    // 执行函数调用的辅助方法
    mc::variant call_function_with_result() {
        auto position = get_object()->get_position();
        auto result   = m_func.template as<func>().call(position, m_func_params);
        if (result.is_null()) {
            elog("Function call failed for property: ${name}", ("name", get_name()));
        }
        return result;
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
        m_setter = [](const T& value) {
            MC_THROW(mc::invalid_op_exception, "设置引用属性值不被允许");
        };
    }

    mc::variant get_relate_property(const relate_property& relate_property) {
        auto* target_object = find_related_object(relate_property.object_name);
        if (target_object == nullptr) {
            return mc::variant();
        }

        return target_object->get_property(relate_property.property_name);
    }

    void set_relate_property(const relate_property& relate_property, const mc::variant& value) {
        auto* target_object = find_related_object(relate_property.object_name);
        if (target_object == nullptr) {
            return;
        }

        target_object->set_property(relate_property.property_name, value);
    }

    void hook_ref_property(const relate_property& relate_property) {
        m_getter = [this, relate_property]() -> T {
            auto result = get_relate_property(relate_property);
            if (result.is_null()) {
                elog("获取引用属性失败: ${object_name}.${property_name}",
                     ("object_name", relate_property.object_name)("property_name", relate_property.property_name));
                return T{};
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
        m_setter = [](const T& value) {
            MC_THROW(mc::invalid_op_exception, "设置同步属性值不被允许");
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

        // 设置初始值
        update_sync_value_from_function();
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

        // 同步属性不支持设置值
        m_setter = [](const T& value) {
            MC_THROW(mc::invalid_op_exception, "设置同步属性值不被允许");
        };
    }

    void hook_relate_properties(mc::variant& call_func, mc::mutable_dict& func_params) {
        m_func        = call_func;
        m_func_params = func_params;

        auto relate_properties = m_func.template as<func>().get_relate_properties(
            get_object()->get_position(), func_params);

        if (relate_properties.empty()) {
            return;
        }

        // 处理第一个关联属性的类型
        for (const auto& entry : relate_properties) {
            auto value = entry.value.template as<mc::expr::relate_property>();
            if (value.type == "ref") {
                hook_ref_properties(relate_properties);
            } else if (value.type == "sync") {
                hook_sync_properties(relate_properties);
            }
            break;
        }
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

        hook_relate_properties(call_func, func_info.params);

        auto result = call_func.template as<func>().call(position, func_info.params);
        if (result.is_null()) {
            elog("函数调用失败: ${name}", ("name", func_info.func));
            return;
        }

        T value;
        from_variant(result, value);
        set_value_impl(std::move(value));
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
        } else if (str.substr(0, 2) == "#/") {
            auto ref_prop = func_parser::get_instance().parse_ref_property(str);
            value.hook_ref_property(ref_prop);
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

    void set_value_impl(param_type new_value) {
        if (is_equal(new_value)) {
            return;
        }
        m_value = new_value;
        notify();
    }

    void set_value_impl(rvalue_type new_value) {
        if (is_equal(new_value)) {
            return;
        }
        m_value = std::move(new_value);
        notify();
    }

    template <typename... SyncProps, size_t... Is>
    void setup_sync_handlers(std::index_sequence<Is...>, SyncProps&... syncs) {
        (setup_sync_handler<Is, SyncProps...>(syncs), ...);
    }

    template <size_t I, typename... SyncProps>
    void setup_sync_handler(property_base& sync) {
        sync.property_changed().connect([this](const mc::variant& value, const property_base&) {
            using ValueType = typename std::tuple_element<I, std::tuple<typename SyncProps::value_type...>>::type;
            auto* cache     = static_cast<detail::property_cache<typename SyncProps::value_type...>*>(m_cache.get());
            cache->template set<I>(value.as<ValueType>());
            update_value();
        });
    }

    void update_value() {
        if (m_getter) {
            set_value_impl(m_getter());
        }
    }

    T                                        m_value{};
    observer_type                            m_observer;
    std::unique_ptr<property_changed_signal> m_signal;
    std::vector<property_base*>              m_syncs;
    std::vector<property_base*>              m_refs;
    std::unique_ptr<void, void (*)(void*)>   m_cache{nullptr, [](void*) {
    }};
    std::function<T()>                       m_getter;
    std::function<void(const T&)>            m_setter;
    mc::variant                              m_func;
    mc::mutable_dict                         m_func_params;
    std::vector<mc::connection_type>         m_connection_slots;
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

#endif // MC_ENGINE_PROPERTY_H