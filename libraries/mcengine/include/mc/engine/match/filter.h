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

#ifndef MC_ENGINE_MATCH_FILTER_H
#define MC_ENGINE_MATCH_FILTER_H

#include <mc/common.h>
#include <mc/engine/message.h>
#include <mc/memory.h>
#include <mc/string.h>
#include <mc/string_view.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace mc::engine::match {

constexpr std::uint32_t no_filter        = 0;
constexpr std::uint32_t condition_filter = 1;

struct filter_spec {
    std::uint32_t backend_type{no_filter};
    mc::string    text;

    bool empty() const noexcept
    {
        return backend_type == no_filter;
    }
};

class MC_API filter_compiled : public mc::shared_base {
public:
    ~filter_compiled() override = default;
};
using filter_compiled_ptr = mc::shared_ptr<filter_compiled>;

class MC_API filter_backend : public mc::shared_base {
public:
    ~filter_backend() override = default;

    virtual std::uint32_t   backend_type() const noexcept = 0;
    virtual mc::string_view name() const noexcept         = 0;

    virtual filter_compiled_ptr compile(mc::string_view text) const                                 = 0;
    virtual bool                evaluate(const filter_compiled& compiled, const message& msg) const = 0;
};
using filter_backend_ptr = mc::shared_ptr<filter_backend>;

class MC_API filter_backend_registry {
public:
    static filter_backend_registry& instance();

    void               register_backend(filter_backend_ptr backend);
    filter_backend_ptr find(std::uint32_t backend_type) const;

    filter_compiled_ptr get_or_compile(std::uint32_t backend_type, mc::string_view text);

    bool evaluate(std::uint32_t backend_type, mc::string_view text, const message& msg);

    void        set_cache_capacity(std::size_t capacity);
    std::size_t cache_capacity() const noexcept;
    std::size_t cache_size() const noexcept;
    void        clear_cache();

    static void reset_for_test();

private:
    filter_backend_registry();
    ~filter_backend_registry();

    filter_backend_registry(const filter_backend_registry&)            = delete;
    filter_backend_registry& operator=(const filter_backend_registry&) = delete;

    struct impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace mc::engine::match

#endif // MC_ENGINE_MATCH_FILTER_H
