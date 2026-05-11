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

#include <mc/db/query/condition.h>
#include <mc/engine/engine.h>
#include <mc/engine/match.h>
#include <mc/engine/message.h>

namespace {

namespace match = mc::engine::match;

mc::engine::message make_signal(const mc::string& iface, const mc::string& member,
                                const mc::string& path = "/x")
{
    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::signal;
    msg.header.interface_name = iface;
    msg.header.member_name    = member;
    msg.header.path           = path;
    return msg;
}

class match_table_test : public ::testing::Test {
protected:
    void SetUp() override
    {
        mc::engine::engine::reset_for_test();
        match::register_condition_filter_backend();
    }

    void TearDown() override
    {
        mc::engine::engine::reset_for_test();
    }
};

} // namespace

// 端到端：纯粗匹配 → find_targets 命中 → lookup_callback 拿回原 callback。
TEST_F(match_table_test, coarse_only_subscription_hits_and_callback_round_trips)
{
    auto table = mc::engine::engine::get_match_table();
    ASSERT_TRUE(table);

    int hits = 0;
    auto cb  = [&hits](const mc::engine::message&) { ++hits; };

    match::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = "org.test.events";
    auto id = table->subscribe("svc.a", rule, match::filter_spec{}, cb);
    ASSERT_NE(id, 0u);

    auto msg     = make_signal("org.test.events", "Updated");
    auto targets = table->find_targets(msg);
    ASSERT_EQ(targets.size(), 1u);
    EXPECT_EQ(targets[0].id, id);
    EXPECT_EQ(targets[0].service_name, "svc.a");

    auto looked = table->lookup_callback("svc.a", id);
    ASSERT_TRUE(looked);
    looked(msg);
    EXPECT_EQ(hits, 1);

    auto miss = make_signal("org.test.other", "Updated");
    EXPECT_TRUE(table->find_targets(miss).empty());
}

// 跨 service：同 rule、不同 service_name；find_targets 应分别命中两条 target。
TEST_F(match_table_test, find_targets_groups_per_service)
{
    auto table = mc::engine::engine::get_match_table();
    match::match_rule rule;
    rule.type = "signal";

    int hits_a = 0, hits_b = 0;
    auto id_a = table->subscribe("svc.a", rule, match::filter_spec{},
                                 [&](const mc::engine::message&) { ++hits_a; });
    auto id_b = table->subscribe("svc.b", rule, match::filter_spec{},
                                 [&](const mc::engine::message&) { ++hits_b; });

    auto targets = table->find_targets(make_signal("org.x", "Y"));
    ASSERT_EQ(targets.size(), 2u);

    table->lookup_callback("svc.a", id_a)(make_signal("org.x", "Y"));
    table->lookup_callback("svc.b", id_b)(make_signal("org.x", "Y"));
    EXPECT_EQ(hits_a, 1);
    EXPECT_EQ(hits_b, 1);

    // service_name 不匹配时 lookup_callback 必须返回空，避免错调。
    EXPECT_FALSE(table->lookup_callback("svc.b", id_a));
}

// condition filter 全程下沉：subscribe 时编译，find_targets 直接跑命中判定。
TEST_F(match_table_test, condition_filter_evaluates_against_message_header)
{
    auto table = mc::engine::engine::get_match_table();
    match::match_rule rule;
    rule.type = "signal";

    namespace q = mc::db::query::conditions;
    auto cond   = q::eq("interface_name", mc::variant(mc::string("org.test.events")));
    auto spec   = match::make_condition_filter(cond);
    EXPECT_EQ(spec.backend_type, match::condition_filter);

    int hits = 0;
    auto id  = table->subscribe("svc.a", rule, spec,
                                [&](const mc::engine::message&) { ++hits; });
    ASSERT_NE(id, 0u);

    auto hit_targets = table->find_targets(make_signal("org.test.events", "Updated"));
    ASSERT_EQ(hit_targets.size(), 1u);
    table->lookup_callback("svc.a", id)(make_signal("org.test.events", "Updated"));
    EXPECT_EQ(hits, 1);

    EXPECT_TRUE(table->find_targets(make_signal("org.test.other", "Updated")).empty());
}

// unsubscribe 反注册后，find_targets 不应再命中，lookup_callback 也回空。
TEST_F(match_table_test, unsubscribe_removes_entry)
{
    auto table = mc::engine::engine::get_match_table();
    match::match_rule rule;
    rule.type = "signal";

    auto id = table->subscribe("svc.a", rule, match::filter_spec{},
                               [](const mc::engine::message&) {});
    ASSERT_EQ(table->size(), 1u);

    table->unsubscribe(id);
    EXPECT_EQ(table->size(), 0u);
    EXPECT_TRUE(table->find_targets(make_signal("any", "any")).empty());
    EXPECT_FALSE(table->lookup_callback("svc.a", id));
}

// 未注册的 backend：subscribe 阶段就应该抛出，避免延迟到 publisher 静默不命中。
TEST_F(match_table_test, subscribe_rejects_unknown_filter_backend)
{
    auto table = mc::engine::engine::get_match_table();
    match::filter_spec spec;
    spec.backend_type = 9999;
    spec.text         = "{}";

    EXPECT_THROW(table->subscribe("svc.a", match::match_rule{}, std::move(spec),
                                  [](const mc::engine::message&) {}),
                 mc::invalid_arg_exception);
}

// engine façade：add_filter_backend / find_filter_backend 应与底层 registry 一致。
TEST_F(match_table_test, engine_facade_forwards_filter_backend_registration)
{
    auto found = mc::engine::engine::find_filter_backend(match::condition_filter);
    ASSERT_TRUE(found);
    EXPECT_EQ(found->backend_type(), match::condition_filter);
    EXPECT_EQ(found->name(), mc::string_view("condition"));
}

// matches：最基础的字段通配语义。
TEST(engine_match_rule, empty_field_is_wildcard)
{
    match::match_rule rule;
    rule.interface_name = "org.test";
    EXPECT_TRUE(match::matches(rule, make_signal("org.test", "Sig").header));
    EXPECT_FALSE(match::matches(rule, make_signal("org.other", "Sig").header));
}

TEST(engine_match_rule, type_field_compared_by_name)
{
    match::match_rule rule;
    rule.type = "signal";
    EXPECT_TRUE(match::matches(rule, make_signal("a", "b").header));

    mc::engine::message call;
    call.header.type = mc::engine::message_type::method_call;
    EXPECT_FALSE(match::matches(rule, call.header));
}
