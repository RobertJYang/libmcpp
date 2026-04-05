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

#include <gtest/gtest.h>

#include <mc/protocol.h>
#include <mc/runtime.h>
#include <test_utilities/base.h>

#include <chrono>
#include <future>
#include <vector>

namespace {

struct routing_layer;
struct codec_layer;
struct inbound_transport_layer;

struct application_layer {
    struct state {
        int push_count{0};
        int pop_count{0};
        int observed_code{0};
    };

    template <typename Ctx>
    static mc::protocol::command on_push(Ctx& ctx, mc::protocol::packet&)
    {
        auto& self = ctx.self();
        ++self.push_count;
        ctx.template get<routing_layer>().route_code = 41;
        return ctx.push_next();
    }

    template <typename Ctx>
    static mc::protocol::command on_pop(Ctx& ctx, mc::protocol::packet&)
    {
        auto& self = ctx.self();
        ++self.pop_count;
        self.observed_code = ctx.template get<routing_layer>().decoded_code;
        return ctx.complete();
    }
};

struct routing_layer {
    struct state {
        int route_code{0};
        int decoded_code{0};
    };

    template <typename Ctx>
    static mc::protocol::command on_push(Ctx& ctx, mc::protocol::packet& packet)
    {
        packet.flags = static_cast<uint32_t>(ctx.self().route_code);
        return ctx.push_next();
    }

    template <typename Ctx>
    static mc::protocol::command on_pop(Ctx& ctx, mc::protocol::packet&)
    {
        ctx.self().decoded_code = ctx.template get<codec_layer>().decoded_code;
        return ctx.pop_next();
    }
};

struct codec_layer {
    struct state {
        int encoded_code{0};
        int decoded_code{0};
    };

    template <typename Ctx>
    static mc::protocol::command on_push(Ctx& ctx, mc::protocol::packet& packet)
    {
        ctx.self().encoded_code = static_cast<int>(packet.flags) + 1;
        packet.flags            = static_cast<uint32_t>(ctx.self().encoded_code);
        return ctx.push_next();
    }

    template <typename Ctx>
    static mc::protocol::command on_pop(Ctx& ctx, mc::protocol::packet& packet)
    {
        ctx.self().decoded_code = static_cast<int>(packet.flags);
        return ctx.pop_next();
    }
};

struct transport_layer {
    struct state {
        int send_count{0};
    };

    template <typename Ctx>
    static mc::protocol::command on_push(Ctx& ctx, mc::protocol::packet& packet)
    {
        ++ctx.self().send_count;
        packet.flags += 100;
        return ctx.pop_next();
    }
};

using roundtrip_stack = mc::protocol::stack_spec<application_layer, routing_layer, codec_layer, transport_layer>;

struct async_layer {
    struct state {
        int  push_count{0};
        bool ready{false};
    };

    template <typename Ctx>
    static mc::protocol::command on_push(Ctx& ctx, mc::protocol::packet&)
    {
        auto& self = ctx.self();
        ++self.push_count;
        if (!self.ready) {
            self.ready = true;
            return ctx.suspend();
        }
        return ctx.push_next();
    }
};

struct terminal_layer {
    struct state {
        int enter_count{0};
    };

    template <typename Ctx>
    static mc::protocol::command on_push(Ctx& ctx, mc::protocol::packet&)
    {
        ++ctx.self().enter_count;
        return ctx.complete();
    }
};

using suspend_stack = mc::protocol::stack_spec<async_layer, terminal_layer>;

struct unsafe_source_layer {
    struct state {
        int value{77};
    };
};

struct unsafe_probe_layer {
    struct state {
        int observed_value{0};
    };

    template <typename Ctx>
    static mc::protocol::command on_push(Ctx& ctx, mc::protocol::packet&)
    {
        auto* source              = static_cast<unsafe_source_layer::state*>(ctx.unsafe_layer_ptr(
            mc::protocol::detail::layer_index<unsafe_source_layer, unsafe_source_layer, unsafe_probe_layer>::value));
        ctx.self().observed_value = source != nullptr ? source->value : -1;
        return ctx.complete();
    }
};

using unsafe_stack = mc::protocol::stack_spec<unsafe_source_layer, unsafe_probe_layer>;

struct inbound_app_layer {
    struct state {
        int decoded_value{0};
    };

    template <typename Ctx>
    static mc::protocol::command on_pop(Ctx& ctx, mc::protocol::packet&)
    {
        ctx.self().decoded_value = ctx.template get<inbound_transport_layer>().wire_value;
        return ctx.complete();
    }
};

struct inbound_transport_layer {
    struct state {
        int wire_value{0};
    };

    template <typename Ctx>
    static mc::protocol::command on_pop(Ctx& ctx, mc::protocol::packet& packet)
    {
        ctx.self().wire_value = static_cast<int>(packet.flags) + 5;
        return ctx.pop_next();
    }
};

using inbound_stack = mc::protocol::stack_spec<inbound_app_layer, inbound_transport_layer>;

struct inbound_handler {
    int           call_count{0};
    std::uint32_t last_flags{0};

    void on_message(const mc::protocol::packet& packet)
    {
        ++call_count;
        last_flags = packet.flags;
    }
};

struct application_dispatch_layer {
    struct state {
        inbound_handler* handler{nullptr};
    };

    template <typename Context>
    static mc::protocol::command on_pop(Context& ctx, mc::protocol::packet& packet)
    {
        auto& self = ctx.self();
        EXPECT_NE(self.handler, nullptr);
        self.handler->on_message(packet);
        return ctx.complete();
    }
};

struct checksum_layer {
    struct state {
        int checksum_hits{0};
    };

    template <typename Context>
    static mc::protocol::command on_pop(Context& ctx, mc::protocol::packet& packet)
    {
        ++ctx.self().checksum_hits;
        packet.flags |= 0x10u;
        return ctx.pop_next();
    }
};

struct example_transport_layer {
    struct state {
        int receive_hits{0};
    };

    template <typename Context>
    static mc::protocol::command on_pop(Context& ctx, mc::protocol::packet& packet)
    {
        ++ctx.self().receive_hits;
        packet.flags |= 0x01u;
        return ctx.pop_next();
    }
};

using inbound_handler_stack =
    mc::protocol::stack_spec<application_dispatch_layer, checksum_layer, example_transport_layer>;

struct failing_layer {
    struct state {
        int hit_count{0};
    };

    template <typename Ctx>
    static mc::protocol::command on_push(Ctx& ctx, mc::protocol::packet&)
    {
        ++ctx.self().hit_count;
        return ctx.fail("missing_route", "route lookup failed");
    }
};

using failing_stack = mc::protocol::stack_spec<failing_layer>;

} // namespace

class protocol_async_timer_test : public mc::test::TestWithRuntime {};

TEST(protocol, umbrella_header_push_pop_roundtrip)
{
    mc::protocol::request<roundtrip_stack> request;

    auto& app       = request.get<application_layer>();
    auto& route     = request.get<routing_layer>();
    auto& codec     = request.get<codec_layer>();
    auto& transport = request.get<transport_layer>();

    auto result = mc::protocol::runtime<roundtrip_stack>::push(request);

    EXPECT_EQ(result, mc::protocol::execution_state::completed);
    EXPECT_EQ(app.push_count, 1);
    EXPECT_EQ(app.pop_count, 1);
    EXPECT_EQ(app.observed_code, 142);
    EXPECT_EQ(route.route_code, 41);
    EXPECT_EQ(route.decoded_code, 142);
    EXPECT_EQ(codec.encoded_code, 42);
    EXPECT_EQ(codec.decoded_code, 142);
    EXPECT_EQ(transport.send_count, 1);
}

TEST(protocol, suspend_then_resume)
{
    mc::protocol::request<suspend_stack> request;

    auto& async    = request.get<async_layer>();
    auto& terminal = request.get<terminal_layer>();

    auto first_result = mc::protocol::runtime<suspend_stack>::push(request);
    EXPECT_EQ(first_result, mc::protocol::execution_state::suspended);
    EXPECT_EQ(async.push_count, 1);
    EXPECT_EQ(terminal.enter_count, 0);

    auto resume_result = mc::protocol::runtime<suspend_stack>::resume(request);
    EXPECT_EQ(resume_result, mc::protocol::execution_state::completed);
    EXPECT_EQ(async.push_count, 2);
    EXPECT_EQ(terminal.enter_count, 1);
}

TEST_F(protocol_async_timer_test, suspend_resume_from_timer_callback)
{
    using namespace std::chrono_literals;

    mc::protocol::request<suspend_stack> request;

    auto first_result = mc::protocol::runtime<suspend_stack>::push(request);
    EXPECT_EQ(first_result, mc::protocol::execution_state::suspended);
    EXPECT_EQ(request.get<async_layer>().push_count, 1);
    EXPECT_EQ(request.get<terminal_layer>().enter_count, 0);

    std::promise<void> done;
    auto               wait_done = done.get_future();

    mc::runtime::steady_timer timer(mc::get_io_executor());
    timer.expires_after(20ms);
    timer.async_wait([&request, p = std::move(done)](const std::error_code& ec) mutable {
        EXPECT_FALSE(ec);
        auto resume_result = mc::protocol::runtime<suspend_stack>::resume(request);
        EXPECT_EQ(resume_result, mc::protocol::execution_state::completed);
        EXPECT_EQ(request.get<async_layer>().push_count, 2);
        EXPECT_EQ(request.get<terminal_layer>().enter_count, 1);
        p.set_value();
    });

    EXPECT_EQ(wait_done.wait_for(5s), std::future_status::ready);
}

TEST(protocol, pop_from_bottom_routes_to_application)
{
    mc::protocol::request<inbound_stack> request;

    auto& app       = request.get<inbound_app_layer>();
    auto& transport = request.get<inbound_transport_layer>();

    request.packet().flags = 13;

    auto result = mc::protocol::runtime<inbound_stack>::pop(request);

    EXPECT_EQ(result, mc::protocol::execution_state::completed);
    EXPECT_EQ(transport.wire_value, 18);
    EXPECT_EQ(app.decoded_value, 18);
}

TEST(protocol, trace_emit_order_inbound_pop)
{
    mc::protocol::request<inbound_stack> request;
    request.packet().flags = 1;

    std::vector<std::size_t> layers;
    mc::protocol::trace_sink sink{};
    sink.user_data = &layers;
    sink.emit      = [](void* user, const mc::protocol::trace_event& e) {
        static_cast<std::vector<std::size_t>*>(user)->push_back(e.layer_index);
    };
    request.set_trace(sink);

    auto result = mc::protocol::runtime<inbound_stack>::pop(request);
    EXPECT_EQ(result, mc::protocol::execution_state::completed);
    ASSERT_EQ(layers.size(), 2u);
    EXPECT_EQ(layers[0], 1u);
    EXPECT_EQ(layers[1], 0u);
}

TEST(protocol, trace_filter_and_layer_mask_gate)
{
    // filter：仅第 0 层进入 emit（与 gate 不同代码路径）
    {
        mc::protocol::request<inbound_stack> request;
        request.packet().flags = 1;

        int                      emit_count = 0;
        mc::protocol::trace_sink sink{};
        sink.user_data = &emit_count;
        sink.filter    = [](void* user, const mc::protocol::trace_event& e) {
            (void)user;
            return e.layer_index == 0;
        };
        sink.emit = [](void* user, const mc::protocol::trace_event& e) {
            (void)e;
            ++*static_cast<int*>(user);
        };
        request.set_trace(sink);

        auto result = mc::protocol::runtime<inbound_stack>::pop(request);
        EXPECT_EQ(result, mc::protocol::execution_state::completed);
        EXPECT_EQ(emit_count, 1);
    }
    // layer_mask：仅第 0 位，不调用其它层的 filter
    {
        mc::protocol::request<inbound_stack> request;
        request.packet().flags = 1;

        std::vector<std::size_t> layers;
        mc::protocol::trace_sink sink{};
        sink.user_data  = &layers;
        sink.layer_mask = mc::protocol::layer_bit(0);
        sink.emit       = [](void* user, const mc::protocol::trace_event& e) {
            static_cast<std::vector<std::size_t>*>(user)->push_back(e.layer_index);
        };
        request.set_trace(sink);

        auto result = mc::protocol::runtime<inbound_stack>::pop(request);
        EXPECT_EQ(result, mc::protocol::execution_state::completed);
        ASSERT_EQ(layers.size(), 1u);
        EXPECT_EQ(layers[0], 0u);
    }
}

TEST(protocol, trace_clear_disables_emit)
{
    mc::protocol::request<inbound_stack> request;
    request.packet().flags = 1;

    int                      emit_count = 0;
    mc::protocol::trace_sink sink{};
    sink.user_data = &emit_count;
    sink.emit      = [](void* user, const mc::protocol::trace_event& e) {
        (void)e;
        ++*static_cast<int*>(user);
    };
    request.set_trace(sink);
    request.clear_trace();

    auto result = mc::protocol::runtime<inbound_stack>::pop(request);
    EXPECT_EQ(result, mc::protocol::execution_state::completed);
    EXPECT_EQ(emit_count, 0);
}

TEST(protocol, pop_inbound_chain_delegates_handler_from_layer_state)
{
    inbound_handler                              handler;
    mc::protocol::request<inbound_handler_stack> request;

    auto& dispatch  = request.get<application_dispatch_layer>();
    auto& checksum  = request.get<checksum_layer>();
    auto& transport = request.get<example_transport_layer>();

    dispatch.handler       = &handler;
    request.packet().flags = 0x20u;

    auto result = mc::protocol::runtime<inbound_handler_stack>::pop(request);

    EXPECT_EQ(result, mc::protocol::execution_state::completed);
    EXPECT_EQ(transport.receive_hits, 1);
    EXPECT_EQ(checksum.checksum_hits, 1);
    EXPECT_EQ(handler.call_count, 1);
    EXPECT_EQ(handler.last_flags, 0x31u);
}

TEST(protocol, unsafe_layer_ptr_and_fail)
{
    mc::protocol::request<roundtrip_stack> request;

    auto& route = request.get<routing_layer>();

    auto* route_state = static_cast<routing_layer::state*>(request.unsafe_layer_ptr(1));
    ASSERT_NE(route_state, nullptr);
    route_state->route_code = 99;
    EXPECT_EQ(route.route_code, 99);
    EXPECT_EQ(request.unsafe_layer_ptr(99), nullptr);

    mc::protocol::request<failing_stack> failing_request;
    auto&                                failing = failing_request.get<failing_layer>();
    auto                                 result  = mc::protocol::runtime<failing_stack>::push(failing_request);

    EXPECT_EQ(result, mc::protocol::execution_state::failed);
    EXPECT_EQ(failing.hit_count, 1);
    EXPECT_EQ(failing_request.error().name, "missing_route");
    EXPECT_EQ(failing_request.error().message, "route lookup failed");
}

TEST(protocol, context_unsafe_layer_ptr_reads_other_layer_state)
{
    mc::protocol::request<unsafe_stack> request;

    auto& probe = request.get<unsafe_probe_layer>();

    auto result = mc::protocol::runtime<unsafe_stack>::push_from<unsafe_probe_layer>(request);

    EXPECT_EQ(result, mc::protocol::execution_state::completed);
    EXPECT_EQ(probe.observed_value, 77);
}

namespace protocol_usability_demo {
struct head_a {
    static constexpr std::size_t push_headroom = 2;
};
struct head_b {
    static constexpr std::size_t push_headroom = 5;
};
} // namespace protocol_usability_demo

TEST(protocol, stack_spec_sums_layer_headroom)
{
    using spec = mc::protocol::stack_spec<protocol_usability_demo::head_a, protocol_usability_demo::head_b>;
    EXPECT_EQ(spec::push_headroom, 7u);
}
