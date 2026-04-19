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
#include <mc/protocol/detail/runtime_core.h>
#include <mc/runtime.h>
#include <mc/time.h>
#include <test_utilities/base.h>

#include <chrono>
#include <future>
#include <vector>

namespace {

struct routing_proto;
struct codec_proto;
struct inbound_transport_proto;

struct application_proto : public mc::proto::protocol {
    struct packet {
        int push_count{0};
        int pop_count{0};
        int observed_code{0};
    };

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        auto& self = req.template packet<application_proto>();
        ++self.push_count;
        req.template packet<routing_proto>().route_code = 41;
        return req.push_next();
    }

    template <typename Req>
    static mc::proto::command on_pop(Req& req)
    {
        auto& self = req.template packet<application_proto>();
        ++self.pop_count;
        self.observed_code = req.template packet<routing_proto>().decoded_code;
        return req.complete();
    }
};

struct routing_proto : public mc::proto::protocol {
    struct packet {
        int route_code{0};
        int decoded_code{0};
    };

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        req.buffer().flags = static_cast<uint32_t>(req.template packet<routing_proto>().route_code);
        return req.push_next();
    }

    template <typename Req>
    static mc::proto::command on_pop(Req& req)
    {
        req.template packet<routing_proto>().decoded_code = req.template packet<codec_proto>().decoded_code;
        return req.pop_next();
    }
};

struct codec_proto : public mc::proto::protocol {
    struct packet {
        int encoded_code{0};
        int decoded_code{0};
    };

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        auto& self        = req.template packet<codec_proto>();
        auto& buffer      = req.buffer();
        self.encoded_code = static_cast<int>(buffer.flags) + 1;
        buffer.flags      = static_cast<uint32_t>(self.encoded_code);
        return req.push_next();
    }

    template <typename Req>
    static mc::proto::command on_pop(Req& req)
    {
        req.template packet<codec_proto>().decoded_code = static_cast<int>(req.buffer().flags);
        return req.pop_next();
    }
};

struct transport_proto : public mc::proto::protocol {
    struct packet {
        int send_count{0};
    };

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        ++req.template packet<transport_proto>().send_count;
        req.buffer().flags += 100;
        return req.pop_next();
    }
};

using roundtrip_proto = mc::proto::stack<application_proto, routing_proto, codec_proto, transport_proto>;

class copied_descriptor_instance : public mc::proto::instance {
public:
    explicit copied_descriptor_instance(const mc::proto::detail::stack_descriptor& descriptor)
        : m_descriptor(descriptor)
    {}

protected:
    const mc::proto::detail::stack_descriptor& stack_descriptor() const noexcept override
    {
        return m_descriptor;
    }

    void* protocol_storage() noexcept override
    {
        return nullptr;
    }

    const void* protocol_storage() const noexcept override
    {
        return nullptr;
    }

    mc::shared_ptr<mc::proto::detail::request_session> make_session() override
    {
        return nullptr;
    }

    void* unsafe_protocol(const std::type_info&) noexcept override
    {
        return nullptr;
    }

    const void* unsafe_protocol(const std::type_info&) const noexcept override
    {
        return nullptr;
    }

private:
    mc::proto::detail::stack_descriptor m_descriptor;
};

struct async_proto : public mc::proto::protocol {
    struct packet {
        int  push_count{0};
        bool ready{false};
    };

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        auto& self = req.template packet<async_proto>();
        ++self.push_count;
        if (!self.ready) {
            self.ready = true;
            return req.suspend();
        }
        return req.push_next();
    }
};

struct terminal_proto : public mc::proto::protocol {
    struct packet {
        int enter_count{0};
    };

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        ++req.template packet<terminal_proto>().enter_count;
        return req.complete();
    }
};

using suspend_proto = mc::proto::stack<async_proto, terminal_proto>;

struct unsafe_source_proto : public mc::proto::protocol {
    struct packet {
        int value{77};
    };
};

struct unsafe_probe_proto : public mc::proto::protocol {
    struct packet {
        int observed_value{0};
    };

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        auto* source = static_cast<unsafe_source_proto::packet*>(req.unsafe_layer_ptr(
            mc::proto::detail::layer_index<unsafe_source_proto, unsafe_source_proto, unsafe_probe_proto>::value));
        req.template packet<unsafe_probe_proto>().observed_value = source != nullptr ? source->value : -1;
        return req.complete();
    }
};

using unsafe_proto = mc::proto::stack<unsafe_source_proto, unsafe_probe_proto>;

struct inbound_app_proto : public mc::proto::protocol {
    struct packet {
        int decoded_value{0};
    };

    template <typename Req>
    static mc::proto::command on_pop(Req& req)
    {
        req.template packet<inbound_app_proto>().decoded_value =
            req.template packet<inbound_transport_proto>().wire_value;
        return req.complete();
    }
};

struct inbound_transport_proto : public mc::proto::protocol {
    struct packet {
        int wire_value{0};
    };

    template <typename Req>
    static mc::proto::command on_pop(Req& req)
    {
        req.template packet<inbound_transport_proto>().wire_value = static_cast<int>(req.buffer().flags) + 5;
        return req.pop_next();
    }
};

using inbound_proto = mc::proto::stack<inbound_app_proto, inbound_transport_proto>;

struct notify_top_proto : public mc::proto::protocol {
    int done_hits{0};
    int fail_hits{0};

    bool on_continue(mc::proto::detail::request_core&) noexcept override
    {
        ++done_hits;
        return true;
    }

    bool on_failed(mc::proto::detail::request_core&) noexcept override
    {
        ++fail_hits;
        return true;
    }
};

struct notify_mid_proto : public mc::proto::protocol {};
struct notify_leaf_proto : public mc::proto::protocol {};

using notify_proto = mc::proto::stack<notify_top_proto, notify_mid_proto, notify_leaf_proto>;

struct inbound_handler {
    int           call_count{0};
    std::uint32_t last_flags{0};

    void on_message(const mc::proto::packet& packet)
    {
        ++call_count;
        last_flags = packet.flags;
    }
};

struct application_dispatch_proto : public mc::proto::protocol {
    struct packet {
        inbound_handler* handler{nullptr};
    };

    template <typename Req>
    static mc::proto::command on_pop(Req& req)
    {
        auto& self = req.template packet<application_dispatch_proto>();
        EXPECT_NE(self.handler, nullptr);
        self.handler->on_message(req.buffer());
        return req.complete();
    }
};

struct checksum_proto : public mc::proto::protocol {
    struct packet {
        int checksum_hits{0};
    };

    template <typename Req>
    static mc::proto::command on_pop(Req& req)
    {
        ++req.template packet<checksum_proto>().checksum_hits;
        req.buffer().flags |= 0x10u;
        return req.pop_next();
    }
};

struct example_transport_proto : public mc::proto::protocol {
    struct packet {
        int receive_hits{0};
    };

    template <typename Req>
    static mc::proto::command on_pop(Req& req)
    {
        ++req.template packet<example_transport_proto>().receive_hits;
        req.buffer().flags |= 0x01u;
        return req.pop_next();
    }
};

using inbound_handler_proto = mc::proto::stack<application_dispatch_proto, checksum_proto, example_transport_proto>;

struct failing_proto : public mc::proto::protocol {
    struct packet {
        int hit_count{0};
    };

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        ++req.template packet<failing_proto>().hit_count;
        return req.fail("missing_route", "route lookup failed");
    }
};

using failing_proto_stack = mc::proto::stack<failing_proto>;

struct context_source_protocol : public mc::proto::protocol {
    struct context {
        int base_code{0};
    };

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        req.template context<context_source_protocol>().base_code = 21;
        return req.push_next();
    }
};

struct context_sink_protocol : public mc::proto::protocol {
    struct packet {
        int observed_code{0};
    };

    struct context {
        bool touched{false};
    };

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        auto& self         = req.template packet<context_sink_protocol>();
        self.observed_code = req.template context<context_source_protocol>().base_code +
                             (req.template context<context_sink_protocol>().touched ? 1 : 0);
        return req.complete();
    }
};

using context_proto = mc::proto::stack<context_source_protocol, context_sink_protocol>;

struct member_suspend_protocol : public mc::proto::protocol {
    struct packet {
        int observed_hits{0};
    };

    int m_hits{0};

    template <typename Req>
    mc::proto::command on_push(Req& req)
    {
        ++m_hits;
        req.template packet<member_suspend_protocol>().observed_hits = m_hits;
        if (m_hits == 1) {
            return req.suspend();
        }
        return req.complete();
    }
};

using member_suspend_proto = mc::proto::stack<member_suspend_protocol>;

struct request_view_proto : public mc::proto::protocol {
    struct packet {
        int observed_hits{0};
    };

    struct context {
        int base_code{11};
    };

    using request_type = mc::proto::request_view<packet, context>;

    static mc::proto::command on_push(request_type& req)
    {
        ++req.packet().observed_hits;
        req.buffer().flags = static_cast<std::uint32_t>(req.context().base_code + req.packet().observed_hits);
        return req.complete();
    }
};

using request_view_stack = mc::proto::stack<request_view_proto>;

struct dual_signature_proto : public mc::proto::protocol {
    struct packet {
        int typed_hits{0};
        int template_hits{0};
    };

    struct context {
        int marker{0};
    };

    using request_type = mc::proto::request_view<packet, context>;

    static mc::proto::command on_push(request_type& req)
    {
        ++req.packet().typed_hits;
        req.context().marker = 19;
        return req.complete();
    }

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        ++req.template packet<dual_signature_proto>().template_hits;
        return req.complete();
    }
};

using dual_signature_stack = mc::proto::stack<dual_signature_proto>;

struct request_view_head_proto : public mc::proto::protocol {
    struct packet {
        int push_hits{0};
    };

    struct context {
        int next_code{31};
    };

    using request_type = mc::proto::request_view<packet, context>;

    static mc::proto::command on_push(request_type& req)
    {
        ++req.packet().push_hits;
        req.buffer().flags = static_cast<std::uint32_t>(req.context().next_code);
        return req.push_next();
    }
};

using mixed_view_stack = mc::proto::stack<request_view_head_proto, transport_proto>;

} // namespace

class protocol_async_timer_test : public mc::test::TestWithRuntime {};

TEST(protocol, umbrella_header_push_pop_roundtrip)
{
    roundtrip_proto::request request;

    auto& app       = request.packet<application_proto>();
    auto& route     = request.packet<routing_proto>();
    auto& codec     = request.packet<codec_proto>();
    auto& transport = request.packet<transport_proto>();

    auto result = mc::proto::runtime<roundtrip_proto::spec_type>::push(request);

    EXPECT_EQ(result, mc::proto::execution_state::completed);
    EXPECT_EQ(app.push_count, 1);
    EXPECT_EQ(app.pop_count, 1);
    EXPECT_EQ(app.observed_code, 142);
    EXPECT_EQ(route.route_code, 41);
    EXPECT_EQ(route.decoded_code, 142);
    EXPECT_EQ(codec.encoded_code, 42);
    EXPECT_EQ(codec.decoded_code, 142);
    EXPECT_EQ(transport.send_count, 1);
}

TEST(protocol, make_instance_push_roundtrip)
{
    mc::proto_ptr proto = roundtrip_proto::create();
    ASSERT_TRUE(proto != nullptr);

    auto req = roundtrip_proto::make_request();

    auto& app       = req.packet<application_proto>();
    auto& route     = req.packet<routing_proto>();
    auto& codec     = req.packet<codec_proto>();
    auto& transport = req.packet<transport_proto>();

    req.prepare_buffer();
    auto result = proto->push(req);

    EXPECT_EQ(result, mc::proto::execution_state::completed);
    EXPECT_EQ(app.push_count, 1);
    EXPECT_EQ(app.pop_count, 1);
    EXPECT_EQ(app.observed_code, 142);
    EXPECT_EQ(route.route_code, 41);
    EXPECT_EQ(route.decoded_code, 142);
    EXPECT_EQ(codec.encoded_code, 42);
    EXPECT_EQ(codec.decoded_code, 142);
    EXPECT_EQ(transport.send_count, 1);
    EXPECT_EQ(req.buffer().flags, 142u);
}

TEST(protocol, runtime_push_async_resolves_for_completed_request)
{
    auto req = roundtrip_proto::make_request();
    req.prepare_buffer();

    auto future = mc::proto::runtime<roundtrip_proto::spec_type>::push_async(req);

    ASSERT_EQ(future.wait_for(std::chrono::seconds{2}), mc::future_status::ready);
    EXPECT_NO_THROW(future.get());
    EXPECT_EQ(req.packet<application_proto>().push_count, 1);
    EXPECT_EQ(req.packet<application_proto>().pop_count, 1);
    EXPECT_EQ(req.packet<application_proto>().observed_code, 142);
}

TEST(protocol, runtime_pop_async_resolves_for_completed_request)
{
    inbound_proto::request request;
    request.buffer().flags = 13;

    auto future = mc::proto::runtime<inbound_proto::spec_type>::pop_async(request);

    ASSERT_EQ(future.wait_for(std::chrono::seconds{2}), mc::future_status::ready);
    EXPECT_NO_THROW(future.get());
    EXPECT_EQ(request.packet<inbound_transport_proto>().wire_value, 18);
    EXPECT_EQ(request.packet<inbound_app_proto>().decoded_value, 18);
}

TEST(protocol, request_view_signature_accesses_current_layer_packet_and_context)
{
    request_view_stack::request request;

    request.context<request_view_proto>().base_code = 20;

    auto result = mc::proto::runtime<request_view_stack::spec_type>::push(request);

    EXPECT_EQ(result, mc::proto::execution_state::completed);
    EXPECT_EQ(request.packet<request_view_proto>().observed_hits, 1);
    EXPECT_EQ(request.buffer().flags, 21u);
}

TEST(protocol, request_view_signature_has_priority_over_template_request_signature)
{
    dual_signature_stack::request request;

    auto result = mc::proto::runtime<dual_signature_stack::spec_type>::push(request);

    EXPECT_EQ(result, mc::proto::execution_state::completed);
    EXPECT_EQ(request.packet<dual_signature_proto>().typed_hits, 1);
    EXPECT_EQ(request.packet<dual_signature_proto>().template_hits, 0);
    EXPECT_EQ(request.context<dual_signature_proto>().marker, 19);
}

TEST(protocol, request_view_signature_coexists_with_template_protocols)
{
    mixed_view_stack::request request;

    auto result = mc::proto::runtime<mixed_view_stack::spec_type>::push(request);

    EXPECT_EQ(result, mc::proto::execution_state::completed);
    EXPECT_EQ(request.packet<request_view_head_proto>().push_hits, 1);
    EXPECT_EQ(request.packet<transport_proto>().send_count, 1);
    EXPECT_EQ(request.buffer().flags, 131u);
}

TEST(protocol, request_deadline_is_available_without_async_completion)
{
    roundtrip_proto::request request;
    const auto               deadline = mc::time_point(mc::milliseconds(1234));

    request.set_deadline(deadline);

    ASSERT_TRUE(request.has_deadline());
    ASSERT_TRUE(request.deadline().has_value());
    EXPECT_EQ(*request.deadline(), deadline);
}

TEST(protocol, request_cancel_propagates_between_request_and_async_future)
{
    roundtrip_proto::request request;
    auto                     future = request.enable_async_completion();

    future.cancel();
    EXPECT_TRUE(request.is_cancelled());
    EXPECT_TRUE(future.is_cancelled());

    auto other_request = roundtrip_proto::make_request();
    auto other_future  = other_request.enable_async_completion();

    other_request.cancel();
    EXPECT_TRUE(other_request.is_cancelled());
    EXPECT_TRUE(other_future.is_cancelled());
}

TEST(protocol, copied_descriptor_instance_is_rejected)
{
    auto req = roundtrip_proto::make_request();
    req.prepare_buffer();

    auto proto = mc::make_shared<copied_descriptor_instance>(
        mc::proto::detail::stack_descriptor_for<roundtrip_proto::spec_type>());
    ASSERT_TRUE(proto != nullptr);

    EXPECT_THROW((void)proto->push(req), mc::invalid_arg_exception);
}

TEST(protocol, suspend_then_resume)
{
    suspend_proto::request request;

    auto& async    = request.packet<async_proto>();
    auto& terminal = request.packet<terminal_proto>();

    auto first_result = mc::proto::runtime<suspend_proto::spec_type>::push(request);
    EXPECT_EQ(first_result, mc::proto::execution_state::suspended);
    EXPECT_EQ(async.push_count, 1);
    EXPECT_EQ(terminal.enter_count, 0);

    auto resume_result = mc::proto::runtime<suspend_proto::spec_type>::resume(request);
    EXPECT_EQ(resume_result, mc::proto::execution_state::completed);
    EXPECT_EQ(async.push_count, 2);
    EXPECT_EQ(terminal.enter_count, 1);
}

TEST(protocol, make_instance_suspend_then_resume)
{
    auto proto = suspend_proto::create();
    ASSERT_TRUE(proto != nullptr);

    auto req = suspend_proto::make_request();

    auto& async    = req.packet<async_proto>();
    auto& terminal = req.packet<terminal_proto>();

    auto first_result = proto->push(req);
    EXPECT_EQ(first_result, mc::proto::execution_state::suspended);
    EXPECT_EQ(async.push_count, 1);
    EXPECT_EQ(terminal.enter_count, 0);

    auto resume_result = proto->resume(req);
    EXPECT_EQ(resume_result, mc::proto::execution_state::completed);
    EXPECT_EQ(async.push_count, 2);
    EXPECT_EQ(terminal.enter_count, 1);
}

TEST(protocol, runtime_resume_preserves_member_protocol_state)
{
    member_suspend_proto::request request;

    auto first_result = mc::proto::runtime<member_suspend_proto::spec_type>::push(request);
    EXPECT_EQ(first_result, mc::proto::execution_state::suspended);
    EXPECT_EQ(request.packet<member_suspend_protocol>().observed_hits, 1);

    auto resume_result = mc::proto::runtime<member_suspend_proto::spec_type>::resume(request);
    EXPECT_EQ(resume_result, mc::proto::execution_state::completed);
    EXPECT_EQ(request.packet<member_suspend_protocol>().observed_hits, 2);
}

TEST(protocol, suspended_request_rejects_resume_from_different_instance)
{
    auto first_proto  = member_suspend_proto::create();
    auto second_proto = member_suspend_proto::create();
    ASSERT_NE(first_proto, nullptr);
    ASSERT_NE(second_proto, nullptr);

    auto request = member_suspend_proto::make_request();

    auto first_result = first_proto->push(request);
    EXPECT_EQ(first_result, mc::proto::execution_state::suspended);

    auto wrong_resume = second_proto->resume(request);
    EXPECT_EQ(wrong_resume, mc::proto::execution_state::failed);
    EXPECT_EQ(request.error().name, "resume_source_mismatch");
}

TEST_F(protocol_async_timer_test, suspend_resume_from_timer_callback)
{
    using namespace std::chrono_literals;

    suspend_proto::request request;

    auto first_result = mc::proto::runtime<suspend_proto::spec_type>::push(request);
    EXPECT_EQ(first_result, mc::proto::execution_state::suspended);
    EXPECT_EQ(request.packet<async_proto>().push_count, 1);
    EXPECT_EQ(request.packet<terminal_proto>().enter_count, 0);

    std::promise<void> done;
    auto               wait_done = done.get_future();

    mc::runtime::steady_timer timer(mc::get_io_executor());
    timer.expires_after(20ms);
    timer.async_wait([&request, p = std::move(done)](const std::error_code& ec) mutable {
        EXPECT_FALSE(ec);
        auto resume_result = mc::proto::runtime<suspend_proto::spec_type>::resume(request);
        EXPECT_EQ(resume_result, mc::proto::execution_state::completed);
        EXPECT_EQ(request.packet<async_proto>().push_count, 2);
        EXPECT_EQ(request.packet<terminal_proto>().enter_count, 1);
        p.set_value();
    });

    EXPECT_EQ(wait_done.wait_for(5s), std::future_status::ready);
}

TEST(protocol, pop_from_bottom_routes_to_application)
{
    inbound_proto::request request;

    auto& app       = request.packet<inbound_app_proto>();
    auto& transport = request.packet<inbound_transport_proto>();

    request.buffer().flags = 13;

    auto result = mc::proto::runtime<inbound_proto::spec_type>::pop(request);

    EXPECT_EQ(result, mc::proto::execution_state::completed);
    EXPECT_EQ(transport.wire_value, 18);
    EXPECT_EQ(app.decoded_value, 18);
}

TEST(protocol, trace_emit_order_inbound_pop)
{
    inbound_proto::request request;
    request.buffer().flags = 1;

    std::vector<std::size_t> layers;
    mc::proto::trace_sink    sink{};
    sink.user_data = &layers;
    sink.emit      = [](void* user, const mc::proto::trace_event& e) {
        static_cast<std::vector<std::size_t>*>(user)->push_back(e.layer_index);
    };
    request.set_trace(sink);

    auto result = mc::proto::runtime<inbound_proto::spec_type>::pop(request);
    EXPECT_EQ(result, mc::proto::execution_state::completed);
    ASSERT_EQ(layers.size(), 2u);
    EXPECT_EQ(layers[0], 1u);
    EXPECT_EQ(layers[1], 0u);
}

TEST(protocol, trace_filter_and_layer_mask_gate)
{
    // filter：仅第 0 层进入 emit（与 gate 不同代码路径）
    {
        inbound_proto::request request;
        request.buffer().flags = 1;

        int                   emit_count = 0;
        mc::proto::trace_sink sink{};
        sink.user_data = &emit_count;
        sink.filter    = [](void* user, const mc::proto::trace_event& e) {
            (void)user;
            return e.layer_index == 0;
        };
        sink.emit = [](void* user, const mc::proto::trace_event& e) {
            (void)e;
            ++*static_cast<int*>(user);
        };
        request.set_trace(sink);

        auto result = mc::proto::runtime<inbound_proto::spec_type>::pop(request);
        EXPECT_EQ(result, mc::proto::execution_state::completed);
        EXPECT_EQ(emit_count, 1);
    }
    // layer_mask：仅第 0 位，不调用其它层的 filter
    {
        inbound_proto::request request;
        request.buffer().flags = 1;

        std::vector<std::size_t> layers;
        mc::proto::trace_sink    sink{};
        sink.user_data  = &layers;
        sink.layer_mask = mc::proto::layer_bit(0);
        sink.emit       = [](void* user, const mc::proto::trace_event& e) {
            static_cast<std::vector<std::size_t>*>(user)->push_back(e.layer_index);
        };
        request.set_trace(sink);

        auto result = mc::proto::runtime<inbound_proto::spec_type>::pop(request);
        EXPECT_EQ(result, mc::proto::execution_state::completed);
        ASSERT_EQ(layers.size(), 1u);
        EXPECT_EQ(layers[0], 0u);
    }
}

TEST(protocol, trace_clear_disables_emit)
{
    inbound_proto::request request;
    request.buffer().flags = 1;

    int                   emit_count = 0;
    mc::proto::trace_sink sink{};
    sink.user_data = &emit_count;
    sink.emit      = [](void* user, const mc::proto::trace_event& e) {
        (void)e;
        ++*static_cast<int*>(user);
    };
    request.set_trace(sink);
    request.clear_trace();

    auto result = mc::proto::runtime<inbound_proto::spec_type>::pop(request);
    EXPECT_EQ(result, mc::proto::execution_state::completed);
    EXPECT_EQ(emit_count, 0);
}

TEST(protocol, pop_inbound_chain_delegates_handler_from_layer_state)
{
    inbound_handler                handler;
    inbound_handler_proto::request request;

    auto& dispatch  = request.packet<application_dispatch_proto>();
    auto& checksum  = request.packet<checksum_proto>();
    auto& transport = request.packet<example_transport_proto>();

    dispatch.handler       = &handler;
    request.buffer().flags = 0x20u;

    auto result = mc::proto::runtime<inbound_handler_proto::spec_type>::pop(request);

    EXPECT_EQ(result, mc::proto::execution_state::completed);
    EXPECT_EQ(transport.receive_hits, 1);
    EXPECT_EQ(checksum.checksum_hits, 1);
    EXPECT_EQ(handler.call_count, 1);
    EXPECT_EQ(handler.last_flags, 0x31u);
}

TEST(protocol, unsafe_layer_ptr_and_fail)
{
    roundtrip_proto::request request;

    auto& route = request.packet<routing_proto>();

    auto* route_state = static_cast<routing_proto::packet*>(request.unsafe_layer_ptr(1));
    ASSERT_NE(route_state, nullptr);
    route_state->route_code = 99;
    EXPECT_EQ(route.route_code, 99);
    EXPECT_EQ(request.unsafe_layer_ptr(99), nullptr);

    failing_proto_stack::request failing_request;
    auto&                        failing = failing_request.packet<failing_proto>();
    auto                         result  = mc::proto::runtime<failing_proto_stack::spec_type>::push(failing_request);

    EXPECT_EQ(result, mc::proto::execution_state::failed);
    EXPECT_EQ(failing.hit_count, 1);
    EXPECT_EQ(failing_request.error().name, "missing_route");
    EXPECT_EQ(failing_request.error().message, "route lookup failed");
}

TEST(protocol, context_unsafe_layer_ptr_reads_other_layer_state)
{
    unsafe_proto::request request;

    auto& probe = request.packet<unsafe_probe_proto>();

    auto result = mc::proto::runtime<unsafe_proto::spec_type>::push_from<unsafe_probe_proto>(request);

    EXPECT_EQ(result, mc::proto::execution_state::completed);
    EXPECT_EQ(probe.observed_value, 77);
}

TEST(protocol, request_aggregates_typed_contexts_per_protocol)
{
    auto request = context_proto::make_request();

    request.context<context_sink_protocol>().touched = true;

    auto result = mc::proto::runtime<context_proto::spec_type>::push(request);

    EXPECT_EQ(result, mc::proto::execution_state::completed);
    EXPECT_EQ(request.context<context_source_protocol>().base_code, 21);
    EXPECT_TRUE(request.context<context_sink_protocol>().touched);
    EXPECT_EQ(request.packet<context_sink_protocol>().observed_code, 22);
}

TEST(protocol, runtime_bind_links_protocol_parent_chain)
{
    notify_proto::request                                            request;
    mc::proto::runtime<notify_proto::spec_type>::protocol_tuple_type protocols;

    auto result = mc::proto::runtime<notify_proto::spec_type>::push(request, protocols);

    EXPECT_EQ(result, mc::proto::execution_state::completed);
    auto& top  = std::get<0>(protocols);
    auto& mid  = std::get<1>(protocols);
    auto& leaf = std::get<2>(protocols);
    EXPECT_EQ(top.parent(), nullptr);
    EXPECT_EQ(mid.parent(), static_cast<mc::proto::protocol*>(&top));
    EXPECT_EQ(leaf.parent(), static_cast<mc::proto::protocol*>(&mid));
}

TEST(protocol, runtime_continue_and_failed_route_to_parent)
{
    notify_proto::request                                            request;
    mc::proto::runtime<notify_proto::spec_type>::protocol_tuple_type protocols;

    auto result = mc::proto::runtime<notify_proto::spec_type>::push(request, protocols);

    EXPECT_EQ(result, mc::proto::execution_state::completed);
    auto& top = std::get<0>(protocols);

    EXPECT_TRUE(mc::proto::detail::continue_request(request.unsafe_core(), 2));
    EXPECT_TRUE(mc::proto::detail::fail_request(request.unsafe_core(), 2));
    EXPECT_EQ(top.done_hits, 1);
    EXPECT_EQ(top.fail_hits, 1);
}

namespace protocol_usability_demo {
struct head_a : public mc::proto::protocol {
    static constexpr std::size_t push_headroom = 2;
};
struct head_b : public mc::proto::protocol {
    static constexpr std::size_t push_headroom = 5;
};
} // namespace protocol_usability_demo

TEST(protocol, stack_spec_sums_layer_headroom)
{
    using proto = mc::proto::stack<protocol_usability_demo::head_a, protocol_usability_demo::head_b>;
    EXPECT_EQ(proto::spec_type::push_headroom, 7u);
}
