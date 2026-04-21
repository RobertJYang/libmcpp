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

namespace {

using namespace std::chrono_literals;

struct route_hint_context : public mc::proto::proto_context {
    int branch{0};
};

struct shared_context : public mc::proto::proto_context {
    int value{0};
};

struct right_leaf_state : public mc::proto::proto_context {
    int push_hits{0};
    int pop_hits{0};
};

struct terminal_state : public mc::proto::proto_context {
    int hits{0};
};

class right_leaf_protocol;

class root_protocol : public mc::proto::protocol {
public:
    mc::proto::protocol* m_right{nullptr};
    int                  m_pop_hits{0};

protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override
    {
        const auto* hint = req.find_context<route_hint_context>();
        if (hint != nullptr && hint->branch == 2 && m_right != nullptr) {
            return push_to(req, *m_right);
        }
        return push_next(req);
    }

    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override
    {
        ++m_pop_hits;
        req.buffer().flags |= 0x80u;
        return push_next(req);
    }
};

class left_leaf_protocol : public mc::proto::protocol {
public:
    int m_push_hits{0};
    int m_pop_hits{0};

protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override
    {
        ++m_push_hits;
        req.buffer().flags |= 0x01u;
        return complete(req);
    }

    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override
    {
        ++m_pop_hits;
        return pop_next(req);
    }
};

class right_leaf_protocol : public mc::proto::protocol {
public:
    int m_push_hits{0};
    int m_pop_hits{0};

protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override
    {
        ++m_push_hits;
        auto& ctx = ensure_context<right_leaf_state>(req);
        ++ctx.push_hits;
        req.buffer().flags |= 0x20u;
        return complete(req);
    }

    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override
    {
        ++m_pop_hits;
        auto& ctx = ensure_context<right_leaf_state>(req);
        ++ctx.pop_hits;
        req.buffer().flags |= 0x08u;
        return pop_next(req);
    }
};

class suspend_protocol : public mc::proto::protocol {
public:
    int m_hits{0};

protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override
    {
        ++m_hits;
        if (m_hits == 1) {
            return suspend(req);
        }
        return push_next(req);
    }

    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override
    {
        (void)req;
        return fail(req, "unexpected_pop", "suspend_protocol does not handle pop");
    }
};

class terminal_protocol : public mc::proto::protocol {
protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override
    {
        auto& ctx = ensure_context<terminal_state>(req);
        ++ctx.hits;
        req.buffer().flags |= 0x40u;
        return complete(req);
    }

    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override
    {
        (void)req;
        return fail(req, "unexpected_pop", "terminal_protocol does not handle pop");
    }
};

class pull_child_protocol : public mc::proto::protocol {
public:
    int        m_pop_hits{0};
    mc::string m_first_payload{"part-a"};
    mc::string m_second_payload{"part-b"};

protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override
    {
        (void)req;
        return fail(req, "unexpected_push", "pull_child_protocol does not handle push");
    }

    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override
    {
        ++m_pop_hits;
        req.buffer().clear();
        if (m_pop_hits == 1) {
            req.buffer().append_payload(m_first_payload.data(), m_first_payload.size());
            return pop_next(req);
        }
        if (m_pop_hits == 2) {
            req.buffer().append_payload(m_second_payload.data(), m_second_payload.size());
            return pop_next(req);
        }
        return suspend(req);
    }
};

class pull_parent_protocol : public mc::proto::protocol {
protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override
    {
        (void)req;
        return fail(req, "unexpected_push", "pull_parent_protocol does not handle push");
    }

    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override
    {
        const auto& buffer = req.buffer();
        const auto  base   = buffer.payload_base();
        const auto  size   = buffer.length() >= base ? buffer.length() - base : 0U;
        if (size == 0U) {
            return pull_next(req);
        }

        const auto payload = mc::string(reinterpret_cast<const char*>(buffer.data() + base), size);
        if (payload == "part-a") {
            return pull_next(req);
        }
        if (payload == "part-b") {
            req.buffer().flags |= 0x10u;
            return complete(req);
        }
        return fail(req, "unexpected_payload", "pull_parent_protocol received unexpected payload");
    }
};

class protocol_async_timer_test : public mc::test::TestWithRuntime {};

TEST(protocol, pop_response_returns_along_recorded_branch)
{
    root_protocol       root;
    left_leaf_protocol  left;
    right_leaf_protocol right;

    root.add_child(left);
    root.add_child(right);
    root.m_right = &right;

    mc::proto::proto_request req;

    const auto state = right.pop(req);

    EXPECT_EQ(state, mc::proto::execution_state::completed);
    EXPECT_EQ(root.m_pop_hits, 1);
    EXPECT_EQ(left.m_push_hits, 0);
    EXPECT_EQ(right.m_pop_hits, 1);
    EXPECT_EQ(right.m_push_hits, 1);

    auto* leaf_ctx = req.find_context<right_leaf_state>(&right);
    ASSERT_NE(leaf_ctx, nullptr);
    EXPECT_EQ(leaf_ctx->pop_hits, 1);
    EXPECT_EQ(leaf_ctx->push_hits, 1);
    EXPECT_EQ(req.buffer().flags, 0xA8u);
}

TEST(protocol, context_lookup_supports_owner_scoped_search)
{
    root_protocol       root;
    right_leaf_protocol right;

    root.add_child(right);
    root.m_right = &right;

    mc::proto::proto_request req;
    req.add_context<shared_context>(&root).value  = 11;
    req.add_context<shared_context>(&right).value = 22;

    auto* latest = req.find_context<shared_context>();
    ASSERT_NE(latest, nullptr);
    EXPECT_EQ(latest->value, 22);

    auto* root_ctx = req.find_context<shared_context>(&root);
    ASSERT_NE(root_ctx, nullptr);
    EXPECT_EQ(root_ctx->value, 11);
}

TEST(protocol, suspend_then_resume_reenters_current_node)
{
    suspend_protocol  head;
    terminal_protocol tail;
    head.add_child(tail);

    mc::proto::proto_request req;

    const auto first = head.push(req);
    EXPECT_EQ(first, mc::proto::execution_state::suspended);
    EXPECT_EQ(head.m_hits, 1);

    const auto second = head.resume(req);
    EXPECT_EQ(second, mc::proto::execution_state::completed);
    EXPECT_EQ(head.m_hits, 2);

    auto* ctx = req.find_context<terminal_state>(&tail);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->hits, 1);
    EXPECT_EQ(req.buffer().flags, 0x40u);
}

TEST(protocol, push_next_fails_when_branch_is_ambiguous)
{
    root_protocol       root;
    left_leaf_protocol  left;
    right_leaf_protocol right;

    root.add_child(left);
    root.add_child(right);

    mc::proto::proto_request req;

    const auto state = root.push(req);

    EXPECT_EQ(state, mc::proto::execution_state::failed);
    EXPECT_EQ(req.error().name, "ambiguous_route");
}

TEST(protocol, resume_without_suspend_fails)
{
    suspend_protocol  head;
    terminal_protocol tail;
    head.add_child(tail);

    mc::proto::proto_request req;

    const auto state = head.resume(req);

    EXPECT_EQ(state, mc::proto::execution_state::failed);
    EXPECT_EQ(req.error().name, "invalid_resume");
}

TEST(protocol, pull_next_reenters_child_on_pop_path)
{
    pull_parent_protocol parent;
    pull_child_protocol  child;
    parent.add_child(child);

    mc::proto::proto_request req;

    const auto state = parent.pop(req);

    EXPECT_EQ(state, mc::proto::execution_state::completed);
    EXPECT_EQ(child.m_pop_hits, 2);
    EXPECT_EQ(req.buffer().flags, 0x10u);
}

TEST_F(protocol_async_timer_test, suspend_resume_from_timer_callback)
{
    suspend_protocol  head;
    terminal_protocol tail;
    head.add_child(tail);

    mc::proto::proto_request req;

    const auto first = head.push(req);
    EXPECT_EQ(first, mc::proto::execution_state::suspended);
    EXPECT_EQ(head.m_hits, 1);

    std::promise<void> done;
    auto               wait_done = done.get_future();

    mc::runtime::steady_timer timer(mc::get_io_executor());
    timer.expires_after(20ms);
    timer.async_wait([&head, &tail, &req, p = std::move(done)](const std::error_code& ec) mutable {
        EXPECT_FALSE(ec);

        const auto second = head.resume(req);
        EXPECT_EQ(second, mc::proto::execution_state::completed);
        EXPECT_EQ(head.m_hits, 2);

        auto* ctx = req.find_context<terminal_state>(&tail);
        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(ctx->hits, 1);
        EXPECT_EQ(req.buffer().flags, 0x40u);
        p.set_value();
    });

    EXPECT_EQ(wait_done.wait_for(5s), std::future_status::ready);
}

} // namespace
