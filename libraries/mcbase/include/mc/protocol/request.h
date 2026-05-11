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

#ifndef MC_PROTOCOL_REQUEST_H
#define MC_PROTOCOL_REQUEST_H

#include <mc/exception.h>
#include <mc/protocol/common.h>
#include <mc/protocol/context.h>
#include <mc/protocol/packet.h>

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace mc::proto {

class protocol;

class MC_API proto_request {
public:
    proto_request();
    ~proto_request();

    mc::proto::buffer& buffer() noexcept;

    const mc::proto::buffer& buffer() const noexcept;

    execution_state state() const noexcept;

    flow_direction direction() const noexcept;

    const protocol_error& error() const noexcept;

    void clear_error() noexcept;

    void set_error(mc::string_view name, mc::string_view message);

    template <typename Context, typename... Args>
    Context& add_context(protocol* owner, Args&&... args)
    {
        static_assert(std::is_base_of_v<proto_context, Context>, "context must derive from proto_context");
        auto ctx = std::make_unique<Context>(std::forward<Args>(args)...);
        return static_cast<Context&>(_append_context(std::move(ctx), owner));
    }

    template <typename Context>
    Context* find_context() noexcept
    {
        static_assert(std::is_base_of_v<proto_context, Context>, "context must derive from proto_context");
        return static_cast<Context*>(_find_context_if(nullptr, &_match_context<Context>));
    }

    template <typename Context>
    const Context* find_context() const noexcept
    {
        static_assert(std::is_base_of_v<proto_context, Context>, "context must derive from proto_context");
        return static_cast<const Context*>(_find_context_if(nullptr, &_match_context<Context>));
    }

    template <typename Context>
    Context* find_context(const protocol* owner) noexcept
    {
        static_assert(std::is_base_of_v<proto_context, Context>, "context must derive from proto_context");
        return static_cast<Context*>(_find_context_if(owner, &_match_context<Context>));
    }

    template <typename Context>
    const Context* find_context(const protocol* owner) const noexcept
    {
        static_assert(std::is_base_of_v<proto_context, Context>, "context must derive from proto_context");
        return static_cast<const Context*>(_find_context_if(owner, &_match_context<Context>));
    }

    template <typename Context, typename... Args>
    Context& ensure_context(protocol* owner, Args&&... args)
    {
        if (auto* ctx = find_context<Context>(owner)) {
            return *ctx;
        }
        return add_context<Context>(owner, std::forward<Args>(args)...);
    }

    template <typename Context>
    Context& require_context(protocol* owner)
    {
        auto* ctx = find_context<Context>(owner);
        MC_ASSERT_THROW(ctx != nullptr, mc::invalid_arg_exception, "缺少请求上下文");
        return *ctx;
    }

    template <typename Visitor>
    void visit_contexts(Visitor&& visitor)
    {
        for (auto* ctx = m_context_tail; ctx != nullptr; ctx = ctx->prev()) {
            visitor(*ctx);
        }
    }

    protocol_list_view route_trace() const noexcept;

    const protocol* resume_target() const noexcept;

private:
    friend class protocol;
    using context_matcher = bool (*)(const proto_context&) noexcept;

    void           begin(protocol& entry, flow_direction direction);
    proto_context& _append_context(std::unique_ptr<proto_context> ctx, protocol* owner);

    void set_state(execution_state state) noexcept;
    void set_resume(protocol* proto, flow_direction direction) noexcept;

    protocol*            resume_protocol() const noexcept;
    flow_direction       resume_direction() const noexcept;
    protocol*            current_protocol() const noexcept;
    protocol*            next_traced_child() const noexcept;
    protocol*            prev_traced_parent() const noexcept;
    bool                 enter_route(protocol& target) noexcept;
    void                 enter_push(protocol& target);
    void                 enter_pop(protocol& target);
    void                 enter_pull(protocol& target);
    proto_context*       _find_context_if(const protocol* owner, context_matcher matcher) noexcept;
    const proto_context* _find_context_if(const protocol* owner, context_matcher matcher) const noexcept;

    template <typename Context>
    static bool _match_context(const proto_context& ctx) noexcept
    {
        return dynamic_cast<const Context*>(&ctx) != nullptr;
    }

    proto::buffer                               m_buffer;
    execution_state                             m_state{execution_state::idle};
    flow_direction                              m_direction{flow_direction::push};
    protocol_error                              m_error;
    proto_context*                              m_context_tail{nullptr};
    std::vector<std::unique_ptr<proto_context>> m_contexts;
    std::vector<protocol*>                      m_route_trace;
    std::size_t                                 m_route_index{0};
    protocol*                                   m_current{nullptr};
    protocol*                                   m_resume_protocol{nullptr};
    flow_direction                              m_resume_direction{flow_direction::push};
};

} // namespace mc::proto

#endif // MC_PROTOCOL_REQUEST_H
