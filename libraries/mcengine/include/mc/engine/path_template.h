/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_ENGINE_PATH_TEMPLATE_H
#define MC_ENGINE_PATH_TEMPLATE_H

#include <mc/common.h>
#include <mc/memory.h>
#include <mc/string.h>
#include <mc/string_view.h>
#include <vector>

namespace mc::engine {

class abstract_object;

class MC_API path_template_backend : public mc::shared_base {
public:
    virtual ~path_template_backend() = default;

    virtual mc::string_view name() const noexcept = 0;
    virtual bool try_resolve(mc::string_view path_pattern, const abstract_object& object, mc::string& path) const = 0;
};

using path_template_backend_ptr  = mc::shared_ptr<path_template_backend>;

class path_template_backend_list {
public:
    using value_type = path_template_backend_ptr;
    using iterator = std::vector<value_type>::iterator;
    using const_iterator = std::vector<value_type>::const_iterator;

    path_template_backend_list() = default;
    path_template_backend_list(std::initializer_list<value_type> init) : m_items(init)
    {}

    bool empty() const noexcept
    {
        return m_items.empty();
    }

    std::size_t size() const noexcept
    {
        return m_items.size();
    }

    void push_back(value_type value)
    {
        m_items.push_back(std::move(value));
    }

    void clear() noexcept
    {
        m_items.clear();
    }

    value_type& operator[](std::size_t index)
    {
        return m_items[index];
    }

    const value_type& operator[](std::size_t index) const
    {
        return m_items[index];
    }

    iterator begin() noexcept
    {
        return m_items.begin();
    }

    const_iterator begin() const noexcept
    {
        return m_items.begin();
    }

    iterator end() noexcept
    {
        return m_items.end();
    }

    const_iterator end() const noexcept
    {
        return m_items.end();
    }

private:
    std::vector<value_type> m_items;
};

} // namespace mc::engine

#endif // MC_ENGINE_PATH_TEMPLATE_H
