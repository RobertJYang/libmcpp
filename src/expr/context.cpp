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
#include <mc/expr/function.h>
#include <mc/log.h>

#include <unordered_map>

namespace mc::expr {

static mc::variant empty_variant;

enum class symbol_type {
    variable,
    function,
};

struct symbol_info : mc::noncopyable {
    symbol_info(std::string name, function_ptr func)
        : name(std::move(name)), type(symbol_type::function), function(std::move(func)) {
    }

    symbol_info(std::string name, mc::variant variable)
        : name(std::move(name)), type(symbol_type::variable), variable(std::move(variable)) {
    }

    ~symbol_info() {
    }

    std::string name;
    symbol_type type;
    union {
        mc::variant               variable;
        std::shared_ptr<function> function;
    };
};

// 符号名称集合，函数和变量也不能同名
using symbol_id_map   = std::unordered_map<int, symbol_info>;
using symbol_name_map = std::unordered_map<std::string_view, symbol_info*>;

class context_impl {
public:
    context_impl() = default;

    explicit context_impl(std::shared_ptr<context_impl> parent) : m_parent(std::move(parent)) {
    }

    explicit context_impl(const mc::dict& dict) {
        import_from_dict(dict);
    }

    context_impl(const mc::dict& dict, std::shared_ptr<context_impl> parent)
        : m_parent(std::move(parent)) {
        import_from_dict(dict);
    }

    void set_parent(std::shared_ptr<context_impl> parent) {
        m_parent = std::move(parent);
    }

    std::shared_ptr<context_impl> get_parent() const {
        return m_parent;
    }

    int register_variable(std::string name, const mc::variant& value) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto id = m_next_id++;
        // 使用emplace的两阶段构造，避免symbol_info的复制
        auto it = m_symbols
                      .emplace(std::piecewise_construct, std::forward_as_tuple(id),
                               std::forward_as_tuple(std::move(name), value))
                      .first;

        // 同名符号相互覆盖
        auto name_ref      = std::string_view(it->second.name);
        auto it_name       = m_symbol_names.find(name_ref);
        bool inserted_name = false;

        if (it_name == m_symbol_names.end()) {
            m_symbol_names.emplace(name_ref, &it->second);
            inserted_name = true;
        } else {
            it_name->second = &it->second;
        }

        if (!inserted_name) {
            wlog("注册变量 ${name} 重复", ("name", name_ref));
        }

        return id;
    }

    int register_function(std::shared_ptr<function> func) {
        if (!func) {
            return -1;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        auto        id   = m_next_id++;
        std::string name = func->get_name();

        // 使用emplace的两阶段构造，避免symbol_info的复制
        auto it = m_symbols
                      .emplace(std::piecewise_construct, std::forward_as_tuple(id),
                               std::forward_as_tuple(std::move(name), std::move(func)))
                      .first;

        // 同名符号相互覆盖
        auto name_ref      = std::string_view(it->second.name);
        auto it_name       = m_symbol_names.find(name_ref);
        bool inserted_name = false;

        if (it_name == m_symbol_names.end()) {
            m_symbol_names.emplace(name_ref, &it->second);
            inserted_name = true;
        } else {
            it_name->second = &it->second;
        }

        if (!inserted_name) {
            wlog("注册函数 ${name} 重复", ("name", name_ref));
        }

        return id;
    }

    symbol_info* get_symbol(std::string_view name) const {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_symbol_names.find(name);
        if (it != m_symbol_names.end()) {
            return it->second;
        }

        if (m_parent) {
            return m_parent->get_symbol(name);
        }

        return nullptr;
    }

    symbol_info* get_symbol(int id) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_symbols.find(id);
        if (it != m_symbols.end()) {
            return &it->second;
        }

        return nullptr;
    }

    bool has_symbol(std::string_view name) const {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_symbol_names.find(name) != m_symbol_names.end()) {
            return true;
        }

        if (m_parent) {
            return m_parent->has_symbol(name);
        }

        return false;
    }

    bool has_symbol(int id) const {
        std::lock_guard<std::mutex> lock(m_mutex);

        return m_symbols.find(id) != m_symbols.end();
    }

    void import_from_dict(const mc::dict& dict) {
        for (const auto& entry : dict) {
            const auto& key = entry.key;
            if (key.is_string()) {
                register_variable(key.get_string(), entry.value);
            }
        }
    }

    bool is_empty() const {
        return m_symbols.empty() && !m_parent;
    }

private:
    mutable std::mutex            m_mutex;
    symbol_id_map                 m_symbols;
    symbol_name_map               m_symbol_names;
    std::shared_ptr<context_impl> m_parent;
    int                           m_next_id{0};
};

context::context() : m_impl(std::make_shared<context_impl>()) {
}

context::context(const context* parent)
    : m_impl(std::make_shared<context_impl>(parent ? parent->m_impl : nullptr)) {
}

context::context(const mc::dict& dict, const context* parent)
    : m_impl(std::make_shared<context_impl>(dict, parent ? parent->m_impl : nullptr)) {
}

void context::set_parent(const context& parent) {
    m_impl->set_parent(parent.m_impl);
}

context context::get_parent() const {
    auto parent_impl = m_impl->get_parent();
    if (!parent_impl) {
        MC_THROW(invalid_op_exception, "表达式上下文错误: 父级上下文不存在");
    }

    // 创建新的context对象并设置其impl为父级impl
    context parent_context;
    parent_context.m_impl = parent_impl;
    return parent_context;
}

int context::register_variable(std::string name, const mc::variant& value) {
    return m_impl->register_variable(std::move(name), value);
}

mc::variant& context::get_variable(std::string_view name) const {
    auto symbol = m_impl->get_symbol(name);
    if (!symbol) {
        return empty_variant;
    }

    return symbol->variable;
}

mc::variant& context::get_variable(int id) const {
    auto symbol = m_impl->get_symbol(id);
    if (!symbol) {
        return empty_variant;
    }

    return symbol->variable;
}

bool context::has_variable(std::string_view name) const {
    auto symbol = m_impl->get_symbol(name);
    if (!symbol) {
        return false;
    }

    return symbol->type == symbol_type::variable;
}

void context::import_from_dict(const mc::dict& dict) {
    m_impl->import_from_dict(dict);
}

int context::register_function(std::shared_ptr<function> func) {
    return m_impl->register_function(std::move(func));
}

std::shared_ptr<function> context::get_function(std::string_view name) const {
    auto symbol = m_impl->get_symbol(name);
    if (!symbol) {
        return nullptr;
    }

    return symbol->function;
}

bool context::has_function(std::string_view name) const {
    auto symbol = m_impl->get_symbol(name);
    if (!symbol) {
        return false;
    }

    return symbol->type == symbol_type::function;
}

bool context::is_empty() const {
    return m_impl->is_empty();
}

} // namespace mc::expr