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
#include <mc/reflect.h>
#include <mc/signal_slot.h>
#include <mc/traits.h>
#include <mc/variant.h>
#include <tuple>
#include <vector>

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
    
    template<typename U = T, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    explicit property(param_type value) : m_value(value) {}
    
    template<typename U = T, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    explicit property(rvalue_type value) : m_value(std::move(value)) {}

    // 同步属性构造函数
    template <typename Getter, typename... SyncProps,
              typename = std::enable_if_t<(sizeof...(SyncProps) > 0)>,
              typename = std::enable_if_t<std::is_invocable_r_v<T, Getter, const typename SyncProps::value_type&...>>>
    property(Getter&& getter, SyncProps&... syncs)
        : m_cache(new detail::property_cache<typename SyncProps::value_type...>(), 
                  [](void* p) { delete static_cast<detail::property_cache<typename SyncProps::value_type...>*>(p); })
    {
        static_assert(sizeof...(SyncProps) <= 10, "At most 10 sync properties are allowed");
        
        // 添加同步源
        m_syncs = { &syncs... };
        
        // 初始化缓存值
        auto* cache = static_cast<detail::property_cache<typename SyncProps::value_type...>*>(m_cache.get());
        [&]<size_t... I>(std::index_sequence<I...>) {
            (cache->template set<I>(syncs.value()), ...);
        }(std::make_index_sequence<sizeof...(SyncProps)>{});
        
        // 包装getter
        m_getter = [this, getter = std::forward<Getter>(getter)]() -> T {
            auto* cache = static_cast<detail::property_cache<typename SyncProps::value_type...>*>(m_cache.get());
            return [&]<size_t... I>(std::index_sequence<I...>) -> T {
                return getter(cache->template get<I>()...);
            }(std::make_index_sequence<sizeof...(SyncProps)>{});
        };
        
        // 设置同步处理器
        setup_sync_handlers<SyncProps...>(std::index_sequence_for<SyncProps...>{}, syncs...);
        
        // 更新初始值
        update_value();
    }

    // 引用属性构造函数
    template <typename Getter, typename Setter, typename... Props,
              typename = std::enable_if_t<(sizeof...(Props) > 0)>,
              typename = std::enable_if_t<std::is_invocable_r_v<T, Getter, Props&...>>,
              typename = std::enable_if_t<std::is_same_v<Setter, std::nullptr_t> || 
                                         std::is_invocable_r_v<void, Setter, const T&, Props&...>>>
    property(Getter&& getter, Setter&& setter, Props&... refs)
    {
        static_assert(sizeof...(Props) <= 10, "At most 10 reference properties are allowed");
        
        // 添加引用源
        (m_refs.push_back(&refs), ...);

        // 包装 getter
        m_getter = [getter = std::forward<Getter>(getter),
                   refs_ptrs = std::make_tuple(&refs...)]() -> T {
            return [&getter, &refs_ptrs]<size_t... I>(std::index_sequence<I...>) -> T {
                return getter(*std::get<I>(refs_ptrs)...);
            }(std::make_index_sequence<sizeof...(Props)>{});
        };

        // 包装 setter
        if constexpr (!std::is_same_v<std::decay_t<Setter>, std::nullptr_t>) {
            m_setter = [setter = std::forward<Setter>(setter),
                       refs_ptrs = std::make_tuple(&refs...)](const T& v) {
                [&setter, &v, &refs_ptrs]<size_t... I>(std::index_sequence<I...>) {
                    setter(v, *std::get<I>(refs_ptrs)...);
                }(std::make_index_sequence<sizeof...(Props)>{});
            };
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

    friend inline void from_variant(const mc::variant& v, property_type& value) {
        from_variant(v, value.m_value);
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
            auto* cache = static_cast<detail::property_cache<typename SyncProps::value_type...>*>(m_cache.get());
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
    std::unique_ptr<void, void (*)(void*)>   m_cache{nullptr, [](void*) {}};
    std::function<T()>                       m_getter;
    std::function<void(const T&)>            m_setter;
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