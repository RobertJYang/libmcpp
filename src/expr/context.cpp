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

#include <mc/exception.h>
#include <mc/expr/context.h>
#include <mc/log.h>

#include <unordered_map>

namespace mc::expr {

static mc::variant empty_variant;

enum class symbol_type {
    invalid,
    variable,
    function,
    object,
};

struct symbol_info {
    symbol_info() : type(symbol_type::invalid) {
    }

    symbol_info(std::string name, function_ptr func)
        : name(std::move(name)), type(symbol_type::function), function(std::move(func)) {
    }

    symbol_info(std::string name, mc::variant variable)
        : name(std::move(name)), type(symbol_type::variable), variable(std::move(variable)) {
    }

    symbol_info(std::string name, mc::engine::abstract_object* object)
        : name(std::move(name)), type(symbol_type::object), object(object) {
    }

    ~symbol_info() {
    }

    std::string name;
    symbol_type type;
    union {
        mc::variant                  variable;
        std::shared_ptr<function>    function;
        mc::engine::abstract_object* object;
    };
};

context_base::context_base(context_base* parent) : m_parent(parent) {
}

void context_base::set_parent(context_base* parent) {
    m_parent = parent;
}

context_base* context_base::get_parent() const {
    return m_parent;
}

context_base::context_base(const context_base& other) : m_parent(other.m_parent) {
}

context_base& context_base::operator=(const context_base& other) {
    if (this != &other) {
        m_parent = other.m_parent;
    }
    return *this;
}

context_base::context_base(context_base&& other) noexcept : m_parent(other.m_parent) {
    other.m_parent = nullptr;
}

context_base& context_base::operator=(context_base&& other) noexcept {
    if (this != &other) {
        m_parent       = other.m_parent;
        other.m_parent = nullptr;
    }
    return *this;
}

bool context_base::has_variable(std::string_view name, std::string_view iface) const {
    if (m_parent) {
        return m_parent->has_variable(name, iface);
    }

    return false;
}

bool context_base::has_function(std::string_view name, std::string_view iface) const {
    if (m_parent) {
        return m_parent->has_function(name, iface);
    }

    return false;
}

const mc::variant& context_base::get_variable(std::string_view name, std::string_view iface) const {
    if (m_parent) {
        return m_parent->get_variable(name, iface);
    }

    return empty_variant;
}

mc::variant context_base::invoke(std::string_view name, const mc::variants& args,
                                 std::string_view iface) const {
    if (m_parent) {
        return m_parent->invoke(name, args, iface);
    }

    if (iface.empty()) {
        MC_THROW(mc::invalid_arg_exception, "函数 ${name} 不存在", ("name", name));
    } else {
        MC_THROW(mc::invalid_arg_exception, "接口 ${iface}.${name} 不存在",
                 ("iface", iface)("name", name));
    }
}

// 符号名称集合，函数和变量也不能同名
using symbol_id_map   = std::unordered_map<int, symbol_info>;
using symbol_name_map = std::unordered_map<std::string_view, symbol_info*>;

class context_impl {
public:
    context_impl() {
    }

    explicit context_impl(const mc::dict& dict) {
        import_from_dict(dict);
    }

    int register_variable(std::string name, const mc::variant& value) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto id = m_next_id++;
        auto it = m_symbols
                      .emplace(std::piecewise_construct, std::forward_as_tuple(id),
                               std::forward_as_tuple(std::move(name), value))
                      .first;
        add_symbol_info(it->second);
        return id;
    }

    int register_function(std::shared_ptr<function> func) {
        if (!func) {
            return -1;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        auto        id   = m_next_id++;
        std::string name = func->get_name();
        auto        it   = m_symbols
                      .emplace(std::piecewise_construct, std::forward_as_tuple(id),
                               std::forward_as_tuple(std::move(name), std::move(func)))
                      .first;
        add_symbol_info(it->second);
        return id;
    }

    int register_object(std::string name, mc::engine::abstract_object* obj) {
        if (!obj) {
            return -1;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        auto id = m_next_id++;
        auto it = m_symbols
                      .emplace(std::piecewise_construct, std::forward_as_tuple(id),
                               std::forward_as_tuple(std::move(name), obj))
                      .first;
        add_symbol_info(it->second);
        return id;
    }

    symbol_info* get_symbol(std::string_view name) const {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_symbol_names.find(name);
        if (it != m_symbol_names.end()) {
            return it->second;
        }

        return nullptr;
    }

    void add_symbol_info(symbol_info& info) {
        auto name          = std::string_view(info.name);
        auto it_name       = m_symbol_names.find(name);
        bool inserted_name = false;

        if (it_name == m_symbol_names.end()) {
            m_symbol_names.emplace(name, &info);
            inserted_name = true;
        } else {
            it_name->second = &info;
        }

        if (!inserted_name) {
            wlog("注册函数 ${name} 重复", ("name", name));
        }
    }

    symbol_info* get_symbol(int id) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_symbols.find(id);
        if (it != m_symbols.end()) {
            return &it->second;
        }

        return nullptr;
    }

    void import_from_dict(const mc::dict& dict) {
        for (const auto& entry : dict) {
            const auto& key = entry.key;
            if (key.is_string()) {
                register_variable(key.get_string(), entry.value);
            }
        }
    }

private:
    mutable std::mutex m_mutex;
    symbol_id_map      m_symbols;
    symbol_name_map    m_symbol_names;
    int                m_next_id{0};
};

context::context() : m_impl(std::make_shared<context_impl>()) {
}

context::context(context_base* parent)
    : context_base(parent), m_impl(std::make_shared<context_impl>()) {
}

context::context(const mc::dict& dict, context_base* parent)
    : context_base(parent), m_impl(std::make_shared<context_impl>(dict)) {
}

context::context(const context& other) : context_base(other.get_parent()), m_impl(other.m_impl) {
}

context& context::operator=(const context& other) {
    if (this != &other) {
        context_base::operator=(other);
        m_impl = other.m_impl;
    }
    return *this;
}

context::context(context&& other) noexcept
    : context_base(other.get_parent()), m_impl(other.m_impl) {
    other.set_parent(nullptr);
    other.m_impl = nullptr;
}

context& context::operator=(context&& other) noexcept {
    if (this != &other) {
        context_base::operator=(other);
        m_impl = other.m_impl;

        other.set_parent(nullptr);
        other.m_impl = nullptr;
    }
    return *this;
}

int context::register_variable(std::string name, const mc::variant& value) {
    return m_impl->register_variable(std::move(name), value);
}

const mc::variant& context::get_variable(std::string_view name, std::string_view iface) const {
    MC_UNUSED(iface);

    auto symbol = m_impl->get_symbol(name);
    if (symbol) {
        // 遮蔽父上下文同名符号
        return symbol->type == symbol_type::variable ? symbol->variable : empty_variant;
    }

    return context_base::get_variable(name);
}

bool context::has_variable(std::string_view name, std::string_view iface) const {
    MC_UNUSED(iface);

    auto symbol = m_impl->get_symbol(name);
    if (symbol) {
        // 遮蔽父上下文同名符号
        return symbol->type == symbol_type::variable;
    }

    return context_base::has_variable(name);
}

bool context::has_function(std::string_view name, std::string_view iface) const {
    MC_UNUSED(iface);

    auto symbol = m_impl->get_symbol(name);
    if (symbol) {
        // 遮蔽父上下文同名符号
        return symbol->type == symbol_type::function;
    }

    return context_base::has_function(name);
}

mc::variant context::invoke(std::string_view name, const mc::variants& args,
                            std::string_view iface) const {
    if (iface.empty()) {
        auto sym_info = m_impl->get_symbol(name);
        if (sym_info && sym_info->type == symbol_type::function) {
            return sym_info->function->call(args);
        }
    }

    return context_base::invoke(name, args, iface);
}

void context::import_from_dict(const mc::dict& dict) {
    m_impl->import_from_dict(dict);
}

int context::register_function(std::shared_ptr<function> func) {
    return m_impl->register_function(std::move(func));
}

int context::register_object(std::string name, mc::engine::abstract_object* obj) {
    return m_impl->register_object(std::move(name), obj);
}

object_context::object_context(mc::engine::abstract_object* obj, context_base* parent)
    : context_base(parent), m_object(obj) {
}

mc::engine::abstract_object* object_context::get_object() const {
    return m_object;
}

bool object_context::has_variable(std::string_view name, std::string_view iface) const {
    return m_object->has_property(name, iface);
}

bool object_context::has_function(std::string_view name, std::string_view iface) const {
    return m_object->has_method(name, iface);
}

const mc::variant& object_context::get_variable(std::string_view name,
                                                std::string_view iface) const {
    m_property = m_object->get_property(name, iface);
    return m_property;
}

mc::variant object_context::invoke(std::string_view name, const mc::variants& args,
                                   std::string_view iface) const {
    return m_object->invoke(name, args, iface);
}

} // namespace mc::expr