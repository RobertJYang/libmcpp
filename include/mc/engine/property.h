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

#include <mc/engine/base.h>
#include <mc/reflect.h>
#include <mc/signal_slot.h>
#include <mc/traits.h>
#include <mc/variant.h>

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

    property() {
    }

    explicit property(param_type value) : m_value(value) {
    }

    explicit property(rvalue_type value) : m_value(std::move(value)) {
    }

    property_type& operator=(rvalue_type new_value) {
        set_value_impl(std::move(new_value));
        return *this;
    }

    property_type& operator=(param_type new_value) {
        set_value_impl(new_value);
        return *this;
    }

    param_type value() const {
        return this->m_value;
    }

    void set_value(rvalue_type new_value) {
        set_value_impl(std::forward<T>(new_value));
    }

    void set_value(param_type new_value) {
        set_value_impl(new_value);
    }

    mc::variant get_value() const override {
        return m_value;
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

    template <typename U>
    void set_value_impl(U new_value) {
        if (is_equal(new_value)) {
            return;
        }

        this->m_value = std::move(new_value);
        notify();
    }

    T             m_value{};
    observer_type m_observer;

    std::unique_ptr<property_changed_signal> m_signal;
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