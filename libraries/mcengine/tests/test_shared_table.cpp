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

// shared_table（match::table 的 SHM 实现）单进程 + 跨进程端到端测试。
//
// 聚焦：
//   - SHM trie 的 subscribe / find_targets / unsubscribe 路径走通；
//   - path 通配（"*" / "**" / 精确）以及其他字段通配的语义与 local_table 一致；
//   - condition filter 在 SHM trie 叶子上被 evaluate；
//   - 析构时 owner 自动摘除自己挂在 trie 上的所有 SHM 订阅条目；
//   - fork 子进程跨进程看到 / 写入订阅；endpoint 死亡残留 sweep 与 reap；
//   - 通过 mq_proto 真正把 signal 跨进程投递到 subscriber 回调，filter 下沉生效。
//
// USE_SHM=OFF 时本文件不进编译单元（meson 按 use_shm 条件编入）。

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <mc/db/query/condition.h>
#include <mc/engine/dispatcher.h>
#include <mc/engine/engine.h>
#include <mc/engine/match.h>
#include <mc/engine/message.h>
#include <mc/engine/service.h>
#include <test_utilities/engine_shm_test_base.h>

#include "match/shared_table.h"
#include "shm_runtime_provider.h"

namespace {

namespace match = mc::engine::match;

class e2e_test_service final : public mc::engine::service {
public:
    explicit e2e_test_service(mc::string_view name) : mc::engine::service(name)
    {}
};

mc::engine::message _make_signal(const mc::string& iface, const mc::string& member, const mc::string& path = "/x")
{
    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::signal;
    msg.header.interface_name = iface;
    msg.header.member_name    = member;
    msg.header.path           = path;
    return msg;
}

// 用公共 fixture 提供 SHM region + shm_runtime 的基座；本测试在其上叠加：
//   - filter_backend_registry 重置 + 注册 condition backend；
//   - _make_table() 快速建 shared_table 实例。
class shared_table_test : public mc::test::TestWithEngineShmRegion {
protected:
    void SetUp() override
    {
        mc::runtime::reset_runtime_context();
        mc::engine::engine::reset_for_test();
        mc::test::TestWithEngineShmRegion::SetUp();
        match::filter_backend_registry::reset_for_test();
        match::register_condition_filter_backend();
    }

    void TearDown() override
    {
        match::filter_backend_registry::reset_for_test();
        mc::engine::engine::reset_for_test();
        mc::test::TestWithEngineShmRegion::TearDown();
        mc::runtime::reset_runtime_context();
    }

    std::unique_ptr<match::shared_table> _make_table(std::uint16_t endpoint_id = 7, std::uint32_t instance_id = 42)
    {
        return std::make_unique<match::shared_table>(*m_runtime, endpoint_id, instance_id);
    }
};

} // namespace

// 端到端：粗匹配命中 → owner_endpoint / instance / id / service_name 透传到 target。
TEST_F(shared_table_test, coarse_only_subscription_hits_through_shm)
{
    auto table = _make_table(11, 99);

    int  hits = 0;
    auto cb   = [&hits](const mc::engine::message&) {
        ++hits;
    };

    match::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = "org.test.events";

    auto id = table->subscribe("svc.shm", rule, match::filter_spec{}, cb);
    ASSERT_NE(id, 0u);
    EXPECT_EQ(table->size(), 1u);

    auto targets = table->find_targets(_make_signal("org.test.events", "Updated"));
    ASSERT_EQ(targets.size(), 1u);
    EXPECT_EQ(targets[0].id, id);
    EXPECT_EQ(targets[0].service_name, "svc.shm");
    EXPECT_EQ(targets[0].endpoint_id, 11);
    EXPECT_EQ(targets[0].instance_id, 99u);

    auto looked = table->lookup_callback("svc.shm", id);
    ASSERT_TRUE(looked);
    looked(_make_signal("org.test.events", "Updated"));
    EXPECT_EQ(hits, 1);

    EXPECT_TRUE(table->find_targets(_make_signal("org.test.other", "Updated")).empty());
}

// path 通配：精确 / 单段 "*" / 末尾 "**" 三种 pattern 的语义。
TEST_F(shared_table_test, path_pattern_branches_resolve_through_trie)
{
    auto table = _make_table();

    auto exact_id = table->subscribe("svc.exact", match::match_rule{"signal", "", "", "/a/b/c", "", ""},
                                     match::filter_spec{}, [](auto&) {
    });
    auto star_id  = table->subscribe("svc.star", match::match_rule{"signal", "", "", "/a/*/c", "", ""},
                                     match::filter_spec{}, [](auto&) {
    });
    auto dstar_id = table->subscribe("svc.dstar", match::match_rule{"signal", "", "", "/a/**", "", ""},
                                     match::filter_spec{}, [](auto&) {
    });
    auto any_id =
        table->subscribe("svc.any", match::match_rule{"signal", "", "", "", "", ""}, match::filter_spec{}, [](auto&) {
    });

    auto count_with_id = [](const std::vector<match::target>& v, match::match_id id) {
        std::size_t n = 0;
        for (const auto& t : v) {
            if (t.id == id) {
                ++n;
            }
        }
        return n;
    };

    // /a/b/c：exact / *single / **multi / wildcard 全命中
    auto t1 = table->find_targets(_make_signal("io.test", "M", "/a/b/c"));
    EXPECT_EQ(count_with_id(t1, exact_id), 1u);
    EXPECT_EQ(count_with_id(t1, star_id), 1u);
    EXPECT_EQ(count_with_id(t1, dstar_id), 1u);
    EXPECT_EQ(count_with_id(t1, any_id), 1u);

    // /a/x/c：exact 不命中；其余三条命中
    auto t2 = table->find_targets(_make_signal("io.test", "M", "/a/x/c"));
    EXPECT_EQ(count_with_id(t2, exact_id), 0u);
    EXPECT_EQ(count_with_id(t2, star_id), 1u);
    EXPECT_EQ(count_with_id(t2, dstar_id), 1u);
    EXPECT_EQ(count_with_id(t2, any_id), 1u);

    // /a/b/c/d/e：exact 与 *single 不命中；**multi / 全通配 命中
    auto t3 = table->find_targets(_make_signal("io.test", "M", "/a/b/c/d/e"));
    EXPECT_EQ(count_with_id(t3, exact_id), 0u);
    EXPECT_EQ(count_with_id(t3, star_id), 0u);
    EXPECT_EQ(count_with_id(t3, dstar_id), 1u);
    EXPECT_EQ(count_with_id(t3, any_id), 1u);

    // /z/y/x：所有 path 段都非 a；只有"全通配"命中
    auto t4 = table->find_targets(_make_signal("io.test", "M", "/z/y/x"));
    EXPECT_EQ(count_with_id(t4, exact_id), 0u);
    EXPECT_EQ(count_with_id(t4, star_id), 0u);
    EXPECT_EQ(count_with_id(t4, dstar_id), 0u);
    EXPECT_EQ(count_with_id(t4, any_id), 1u);
}

// condition filter 全程下沉 SHM：subscribe 把 (backend_type, text) 写进 trie 叶子，
// find_targets 在 owner 锁外 evaluate；命中和不命中都走同一份 backend。
TEST_F(shared_table_test, condition_filter_evaluates_against_message_header)
{
    auto table = _make_table();

    namespace q = mc::db::query::conditions;
    auto cond   = q::eq("interface_name", mc::variant(mc::string("org.test.events")));
    auto spec   = match::make_condition_filter(cond);
    EXPECT_EQ(spec.backend_type, match::condition_filter);

    match::match_rule rule;
    rule.type = "signal";

    int  hits = 0;
    auto id   = table->subscribe("svc.cond", rule, spec, [&hits](const mc::engine::message&) {
        ++hits;
    });
    ASSERT_NE(id, 0u);

    auto matched = table->find_targets(_make_signal("org.test.events", "Updated"));
    ASSERT_EQ(matched.size(), 1u);
    EXPECT_EQ(matched[0].id, id);

    auto missed = table->find_targets(_make_signal("org.test.other", "Updated"));
    EXPECT_TRUE(missed.empty());

    table->lookup_callback("svc.cond", id)(_make_signal("org.test.events", "Updated"));
    EXPECT_EQ(hits, 1);
}

// unsubscribe 后：size 归零、lookup 找不到 callback、find_targets 不命中。
TEST_F(shared_table_test, unsubscribe_removes_entry_from_trie)
{
    auto table = _make_table();

    match::match_rule rule;
    rule.type = "signal";

    auto id = table->subscribe("svc.x", rule, match::filter_spec{}, [](const mc::engine::message&) {
    });
    ASSERT_EQ(table->size(), 1u);

    table->unsubscribe(id);
    EXPECT_EQ(table->size(), 0u);
    EXPECT_TRUE(table->find_targets(_make_signal("any", "any")).empty());
    EXPECT_FALSE(table->lookup_callback("svc.x", id));
}

// 析构 owner table：所有自己挂在 SHM trie 上的订阅条目应被自动摘掉，
// 后续在同一 region 上重建 table 不应继承前一个进程的旧条目。
TEST_F(shared_table_test, destructor_drops_all_owned_entries)
{
    {
        auto              t1 = _make_table(31, 1);
        match::match_rule rule;
        rule.type = "signal";
        t1->subscribe("svc.a", rule, match::filter_spec{}, [](auto&) {
        });
        t1->subscribe("svc.b", rule, match::filter_spec{}, [](auto&) {
        });
        ASSERT_EQ(t1->size(), 2u);
    }
    // t1 析构后所有自己挂在 trie 的条目应该已经被摘除。新 table attach 同一 region，
    // 不该看到任何残留命中。
    auto t2 = _make_table(31, 2);
    EXPECT_EQ(t2->size(), 0u);
    EXPECT_TRUE(t2->find_targets(_make_signal("any", "any")).empty());
}

// 同一 endpoint 多 instance：owner_instance_id 必须正确透传到 target。
TEST_F(shared_table_test, target_instance_id_reflects_owner_instance)
{
    auto a = _make_table(7, 1);
    auto b = _make_table(7, 2);

    match::match_rule rule;
    rule.type = "signal";

    auto id_a = a->subscribe("svc.a", rule, match::filter_spec{}, [](auto&) {
    });
    auto id_b = b->subscribe("svc.b", rule, match::filter_spec{}, [](auto&) {
    });

    auto targets = a->find_targets(_make_signal("io", "M"));
    ASSERT_EQ(targets.size(), 2u);
    for (const auto& t : targets) {
        if (t.id == id_a) {
            EXPECT_EQ(t.instance_id, 1u);
            EXPECT_EQ(t.service_name, "svc.a");
        } else if (t.id == id_b) {
            EXPECT_EQ(t.instance_id, 2u);
            EXPECT_EQ(t.service_name, "svc.b");
        } else {
            ADD_FAILURE() << "unexpected target id=" << t.id;
        }
    }
}

// 跨进程：父进程订阅 → fork → 子进程在同一 SHM region attach 出独立 shm_runtime
// 与独立 shared_table，应当能看到父进程留在 trie 上的订阅。子进程再注册一条自己的，
// 通过 _exit 跳过子进程自身的析构 sweep（模拟"还没来得及清理就退出"），父进程 sweep
// 之前应当看到子进程残留的订阅。
TEST_F(shared_table_test, fork_child_sees_parent_subscription_via_shm)
{
    constexpr std::uint16_t kParentEp   = 11U;
    constexpr std::uint32_t kParentInst = 1U;
    constexpr std::uint16_t kChildEp    = 12U;
    constexpr std::uint32_t kChildInst  = 1U;

    auto parent_table = _make_table(kParentEp, kParentInst);

    match::match_rule rule_parent;
    rule_parent.type           = "signal";
    rule_parent.interface_name = "org.test.parent";
    auto parent_match_id =
        parent_table->subscribe("svc.parent", rule_parent, match::filter_spec{}, [](const mc::engine::message&) {
    });
    ASSERT_NE(parent_match_id, 0u);

    fork_child([&]() -> int {
        // 父进程的 unique_ptr<shared_table> 是 fork 出来的副本，destructor 会触发
        // _drop_owned_entries 把父亲的 SHM 订阅清掉；这里 *绝对不能* 让它走正常
        // C++ 析构 —— fork_child 本身用 _exit 跳过析构链，但 release() 仍是必要的
        // 兜底，避免任何意外 reset 路径。同理 m_runtime 也 release。
        (void)parent_table.release();
        (void)m_runtime.release();

        // 故意 heap-allocate 不释放：_exit 跳过析构链，模拟"子进程在还没来得及
        // 清理订阅时退出"。SHM 残留 entry 会留在 trie 上，由父进程 find_targets
        // 看见，并最终通过 sweep_dead_endpoint 清理。
        auto* child_rt = open_child_shm_runtime().release();
        if (!child_rt) {
            return 101;
        }

        match::filter_backend_registry::reset_for_test();
        match::register_condition_filter_backend();

        auto* child_table = new match::shared_table(*child_rt, kChildEp, kChildInst);

        auto seen_parent = child_table->find_targets(_make_signal("org.test.parent", "Updated"));
        if (seen_parent.size() != 1U || seen_parent[0].endpoint_id != kParentEp ||
            seen_parent[0].instance_id != kParentInst) {
            return 102;
        }

        match::match_rule rule_child;
        rule_child.type           = "signal";
        rule_child.interface_name = "org.test.child";
        auto child_match_id =
            child_table->subscribe("svc.child", rule_child, match::filter_spec{}, [](const mc::engine::message&) {
        });
        if (child_match_id == 0u) {
            return 103;
        }
        return 0;
    });

    // === 父进程（fork_child 内部已 waitpid 并断言子进程 0 退出） ===
    // 父进程应当能在 SHM trie 上看到子进程留下的订阅（子进程跳过了析构 sweep）。
    auto seen_child = parent_table->find_targets(_make_signal("org.test.child", "Hi"));
    ASSERT_EQ(seen_child.size(), 1u);
    EXPECT_EQ(seen_child[0].endpoint_id, kChildEp);
    EXPECT_EQ(seen_child[0].instance_id, kChildInst);
    EXPECT_EQ(seen_child[0].service_name, "svc.child");

    // sweep_dead_endpoint 应当把子进程残留的订阅清掉。min_alive_instance_id
    // 设为子 instance + 1 表示"子进程那一代实例已死，更新一代正在接管"。
    auto removed = parent_table->sweep_dead_endpoint(kChildEp, kChildInst + 1U);
    EXPECT_EQ(removed, 1u);

    EXPECT_TRUE(parent_table->find_targets(_make_signal("org.test.child", "Hi")).empty());
    // 父进程自己的订阅不受影响。
    auto still_parent = parent_table->find_targets(_make_signal("org.test.parent", "Updated"));
    ASSERT_EQ(still_parent.size(), 1u);
    EXPECT_EQ(still_parent[0].endpoint_id, kParentEp);
}

// endpoint sweep 单进程行为：
//   1) 对 endpoint_id == 0 sweep 是 no-op（语义保护，不能扫"本进程默认 owner"）；
//   2) sweep min_alive 大于所有现存 instance_id 时，本 endpoint 上所有 entry 全清；
//   3) 其他 endpoint 上的 entry 不受影响。
TEST_F(shared_table_test, sweep_dead_endpoint_filters_by_endpoint_and_instance)
{
    constexpr std::uint16_t kEp    = 17U;
    constexpr std::uint16_t kEpAlt = 18U;

    // 用 heap leak 模拟"残留 entry"：subscribe 后跳过 owner 的析构 sweep，
    // 让 entry 留在 SHM 里，再走 sweep_dead_endpoint 清。
    auto*             leaked = new match::shared_table(*m_runtime, kEp, /*instance=*/1U);
    match::match_rule r;
    r.type = "signal";
    leaked->subscribe("svc.leak.a", r, match::filter_spec{}, [](auto&) {
    });
    leaked->subscribe("svc.leak.b", r, match::filter_spec{}, [](auto&) {
    });

    // 另一个 endpoint 上的活订阅：sweep 不能误伤。
    auto other = _make_table(kEpAlt, 1U);
    other->subscribe("svc.other", r, match::filter_spec{}, [](auto&) {
    });

    auto sweeper = _make_table(kEp, 99U);

    // 端点 0 受保护：no-op。
    EXPECT_EQ(sweeper->sweep_dead_endpoint(0U, 1000U), 0u);

    // 不同 endpoint 不受影响。
    EXPECT_EQ(sweeper->sweep_dead_endpoint(kEp, 2U), 2u);

    auto remaining = sweeper->find_targets(_make_signal("io", "M"));
    ASSERT_EQ(remaining.size(), 1u);
    EXPECT_EQ(remaining[0].endpoint_id, kEpAlt);
    EXPECT_EQ(remaining[0].service_name, "svc.other");

    // 重复 sweep 已无残留，应当返回 0。
    EXPECT_EQ(sweeper->sweep_dead_endpoint(kEp, 2U), 0u);

    // 故意不 delete leaked：它的本地状态指向已被 sweep 清掉的 SHM 偏移，析构会
    // 触发 use-after-free。对应"crashed process"语义。
    (void)leaked;
}

// 闭环 reap：子进程通过 shm_runtime::register_endpoint 申请真实 endpoint_id，
// 订阅后 _exit 模拟进程崩溃。父进程调用 reap_dead_endpoints 应当：
//   1) 先 recover_stale_endpoints 把心跳失效的 endpoint 切到 aborting；
//   2) 枚举 endpoint 表，对非活状态的 endpoint 统一 sweep；
//   3) 子进程残留的订阅在父进程视角彻底消失。
TEST_F(shared_table_test, reap_dead_endpoints_clears_crashed_process_residue)
{
    constexpr std::uint16_t kParentEp   = 11U;
    constexpr std::uint32_t kParentInst = 1U;

    // 父进程订阅一条作为"对照组"，reap 不应误伤。
    auto              parent_table = _make_table(kParentEp, kParentInst);
    match::match_rule rule_parent;
    rule_parent.type           = "signal";
    rule_parent.interface_name = "org.test.parent";
    parent_table->subscribe("svc.parent", rule_parent, match::filter_spec{}, [](const mc::engine::message&) {
    });

    fork_child([&]() -> int {
        // 父侧 unique_ptr 不许走析构（否则父订阅会被一并清掉）。
        (void)parent_table.release();
        (void)m_runtime.release();

        // 同样用 release() 把 child_rt / child_table 的 unique_ptr 所有权泄掉，
        // 让 _exit 跳过析构链，精确模拟"进程崩溃来不及清理"。
        auto* child_rt = open_child_shm_runtime().release();
        if (!child_rt) {
            return 101;
        }
        auto ep_opt = register_running_endpoint(*child_rt, "mcengine.match.crash_child");
        if (!ep_opt.has_value() || !ep_opt->is_valid()) {
            return 102;
        }

        match::filter_backend_registry::reset_for_test();
        match::register_condition_filter_backend();

        auto* child_table = new match::shared_table(*child_rt, ep_opt->endpoint_id, ep_opt->instance_id);

        match::match_rule rule_child;
        rule_child.type           = "signal";
        rule_child.interface_name = "org.test.crash_child";
        auto child_match_id =
            child_table->subscribe("svc.crash", rule_child, match::filter_spec{}, [](const mc::engine::message&) {
        });
        if (child_match_id == 0u) {
            return 104;
        }
        return 0;
    });

    // reap 之前：父能在 SHM trie 上看到子进程的残留。
    auto seen_before = parent_table->find_targets(_make_signal("org.test.crash_child", "Hi"));
    ASSERT_EQ(seen_before.size(), 1u);
    const auto crash_ep = seen_before[0].endpoint_id;
    EXPECT_NE(crash_ep, 0U);
    EXPECT_NE(crash_ep, kParentEp);

    // 子进程实际已死，但心跳 deadline 可能还没到 → 强行把 deadline 推过去。
    // 通过 abort_endpoint 直接把 endpoint 标 aborting，就能触发 reap。
    mc::shm::endpoint dead{};
    dead.endpoint_id = crash_ep;
    dead.instance_id = seen_before[0].instance_id;
    EXPECT_TRUE(m_runtime->abort_endpoint(dead));

    // reap：枚举 endpoint 表，把非活状态的全部 sweep。
    auto reaped = match::reap_dead_endpoints(*parent_table, *m_runtime);
    EXPECT_EQ(reaped, 1u);

    // reap 之后：子进程残留消失，父订阅完好。
    EXPECT_TRUE(parent_table->find_targets(_make_signal("org.test.crash_child", "Hi")).empty());
    auto still_parent = parent_table->find_targets(_make_signal("org.test.parent", "Updated"));
    ASSERT_EQ(still_parent.size(), 1u);
    EXPECT_EQ(still_parent[0].endpoint_id, kParentEp);
    EXPECT_EQ(still_parent[0].instance_id, kParentInst);

    // 重复 reap 已无残留，应当 0。
    EXPECT_EQ(match::reap_dead_endpoints(*parent_table, *m_runtime), 0u);
}

// 跨进程端到端：子进程发布 signal（mq_proto）-> 父进程 service 收包 -> match 命中
// 本地订阅 callback。覆盖外部协议 <-> service <-> match 整体路径。
TEST_F(shared_table_test, cross_process_signal_reaches_subscriber_callback_via_mq_proto)
{
    auto subscriber_ep = register_running_endpoint(*m_runtime, "mcengine.e2e.subscriber");
    ASSERT_TRUE(subscriber_ep.has_value());

    auto subscriber_table =
        mc::make_shared<match::shared_table>(*m_runtime, subscriber_ep->endpoint_id, subscriber_ep->instance_id);
    mc::engine::engine::set_match_table(subscriber_table);

    e2e_test_service subscriber_service("svc.e2e.subscriber");
    ASSERT_TRUE(subscriber_service.init());
    ASSERT_TRUE(subscriber_service.start());

    std::promise<void> callback_triggered;
    auto               callback_future = callback_triggered.get_future();
    std::atomic<int>   callback_hits{0};
    match::match_rule  rule;
    rule.type           = "signal";
    rule.interface_name = "org.test.e2e";
    rule.member_name    = "InterfaceAdded";
    auto subscription_id =
        subscriber_service.add_match(
            rule, match::filter_spec{}, [&callback_triggered, &callback_hits](const mc::engine::message& msg) {
        if (msg.header.interface_name == "org.test.e2e" && msg.header.member_name == "InterfaceAdded") {
            auto previous = callback_hits.fetch_add(1, std::memory_order_acq_rel);
            if (previous == 0) {
                callback_triggered.set_value();
            }
        }
    });
    ASSERT_NE(subscription_id, 0u);

    mq_rx_pipeline rx;
    subscriber_service.set_proto(&rx.proto);
    rx.start(runtime_alias(), *subscriber_ep, subscriber_ep->instance_id);

    fork_child([this]() -> int {
        auto child_rt = open_child_shm_runtime();
        if (!child_rt) {
            return 101;
        }
        auto publisher_ep = register_running_endpoint(*child_rt, "mcengine.e2e.publisher");
        if (!publisher_ep.has_value()) {
            return 102;
        }

        auto publisher_table =
            mc::make_shared<match::shared_table>(*child_rt, publisher_ep->endpoint_id, publisher_ep->instance_id);
        mc::engine::engine::set_match_table(publisher_table);

        mc::engine::message signal;
        signal.header.type           = mc::engine::message_type::signal;
        signal.header.sender         = "svc.e2e.publisher";
        signal.header.path           = "/org/test/e2e";
        signal.header.interface_name = "org.test.e2e";
        signal.header.member_name    = "InterfaceAdded";
        signal.body = mc::engine::make_payload<mc::engine::signal_payload>("s", mc::variants{mc::variant("eth0")});

        auto targets = mc::engine::engine::get_match_table()->find_targets(signal);
        if (targets.empty()) {
            return 104;
        }

        for (const auto& target : targets) {
            auto target_info = child_rt->get_endpoint(target.endpoint_id);
            if (!target_info.has_value()) {
                return 105;
            }
            auto target_ep = to_endpoint(*target_info);

            // 按 wire 协议约定：publisher 把 (service_name, match_id) 写入
            // header.context 的并行数组，subscriber 的 dispatch_event 直接按
            // 列表分发到每个 service 的 callback，避免在 SHM 共享 trie 上
            // 重复跑 find_targets。
            auto routed = signal;
            match::set_target_match_ids(routed.header, {{target.service_name, target.id}});
            send_via_mq(*child_rt, *publisher_ep, target_ep, routed);
        }
        return 0;
    });

    ASSERT_EQ(callback_future.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(callback_hits.load(std::memory_order_acquire), 1);

    rx.stop();
    subscriber_service.set_proto(nullptr);
    subscriber_service.remove_match(subscription_id);
    subscriber_service.stop();
    mc::engine::engine::set_match_table(nullptr);
}

// 验证跨进程 filter 下沉确实在 publisher 侧生效：
//   subscriber 订阅 (signal, org.test.e2e, FilteredSignal)，附带 condition filter
//   要求 sender == "svc.desired"；
//   publisher 发两条 signal：一条 sender=svc.desired（应命中），一条 sender=
//   svc.other（应被 filter 过滤掉）；
//   断言：
//     1) publisher 端对未命中 signal find_targets 返回空（filter 真的在 publisher
//        侧 evaluate，不是 subscriber 侧再做判定）；
//     2) subscriber callback 只被触发 1 次，且收到的是 sender=svc.desired 那条。
TEST_F(shared_table_test, cross_process_condition_filter_is_evaluated_at_publisher)
{
    auto subscriber_ep = register_running_endpoint(*m_runtime, "mcengine.e2e.filter.subscriber");
    ASSERT_TRUE(subscriber_ep.has_value());

    auto subscriber_table =
        mc::make_shared<match::shared_table>(*m_runtime, subscriber_ep->endpoint_id, subscriber_ep->instance_id);
    mc::engine::engine::set_match_table(subscriber_table);

    e2e_test_service subscriber_service("svc.e2e.filter.subscriber");
    ASSERT_TRUE(subscriber_service.init());
    ASSERT_TRUE(subscriber_service.start());

    std::mutex              cb_mutex;
    std::vector<mc::string> received_senders;
    std::vector<mc::string> inbound_senders;
    std::vector<std::size_t> inbound_pair_counts;
    match::match_rule       rule;
    rule.type           = "signal";
    rule.interface_name = "org.test.e2e";
    rule.member_name    = "FilteredSignal";

    // 使用 condition filter 要求 sender == "svc.desired"。
    mc::db::query::condition cond(mc::db::query::compare_op::eq, mc::string("sender"),
                                  mc::variant(mc::string("svc.desired")));
    auto                     filter = match::make_condition_filter(cond);

    auto subscription_id =
        subscriber_service.add_match(rule, filter, [&cb_mutex, &received_senders](const mc::engine::message& msg) {
        std::lock_guard lock(cb_mutex);
        received_senders.push_back(msg.header.sender);
    });
    ASSERT_NE(subscription_id, 0u);

    mq_rx_pipeline rx;
    subscriber_service.set_proto(&rx.proto);
    rx.proto.set_inbound_handler([&cb_mutex, &inbound_senders, &inbound_pair_counts, &subscriber_service](
                                     mc::engine::message request) -> mc::engine::message {
        {
            std::lock_guard lock(cb_mutex);
            inbound_senders.push_back(request.header.sender);
            inbound_pair_counts.push_back(match::get_target_match_ids(request.header).size());
        }
        return mc::engine::dispatch(subscriber_service, request);
    });
    rx.start(runtime_alias(), *subscriber_ep, subscriber_ep->instance_id);

    fork_child([this]() -> int {
        auto child_rt = open_child_shm_runtime();
        if (!child_rt) {
            return 101;
        }
        auto publisher_ep = register_running_endpoint(*child_rt, "mcengine.e2e.filter.publisher");
        if (!publisher_ep.has_value()) {
            return 102;
        }

        auto publisher_table =
            mc::make_shared<match::shared_table>(*child_rt, publisher_ep->endpoint_id, publisher_ep->instance_id);
        mc::engine::engine::set_match_table(publisher_table);
        // 注意：子进程 fork 来自父进程，registry 已在父进程 SetUp 注册过 condition
        // backend；backend_registry 是进程级 singleton，fork 之后的子进程拷贝了
        // 父进程的 backend 列表，compile 时能命中，无需再次 register。

        auto build_signal = [](const mc::string& sender) {
            mc::engine::message signal;
            signal.header.type           = mc::engine::message_type::signal;
            signal.header.sender         = sender;
            signal.header.path           = "/org/test/e2e";
            signal.header.interface_name = "org.test.e2e";
            signal.header.member_name    = "FilteredSignal";
            signal.body                  = mc::engine::make_payload<mc::engine::signal_payload>(
                "s", mc::variants{mc::variant(mc::string("payload"))});
            return signal;
        };

        auto send_one = [&](const mc::engine::message& signal) -> std::size_t {
            auto targets = mc::engine::engine::get_match_table()->find_targets(signal);
            for (const auto& target : targets) {
                auto target_info = child_rt->get_endpoint(target.endpoint_id);
                if (!target_info.has_value()) {
                    continue;
                }
                auto target_ep = to_endpoint(*target_info);
                auto routed    = signal;
                match::set_target_match_ids(routed.header, {{target.service_name, target.id}});
                send_via_mq(*child_rt, *publisher_ep, target_ep, routed);
            }
            return targets.size();
        };

        // 未命中：publisher 端 filter evaluate 应把这一条 drop。
        if (send_one(build_signal("svc.other")) != 0U) {
            // filter 下沉失败 —— publisher 本应直接过滤掉。
            return 110;
        }

        // 命中：publisher 端 filter evaluate 通过，投到 subscriber。
        if (send_one(build_signal("svc.desired")) != 1U) {
            return 111;
        }
        return 0;
    });

    // 给 subscriber 侧 mq_channel + dispatcher 足够时间把 desired 那条消息完成回调。
    // 没有强同步点：轮询检查 received_senders 到达 1 条或超时。
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard lock(cb_mutex);
            if (received_senders.size() >= 1U) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto pending = m_runtime->open_queue(*subscriber_ep).try_receive_message();

    {
        std::lock_guard lock(cb_mutex);
        ASSERT_EQ(received_senders.size(), 1U)
            << "期望 callback 恰好触发 1 次（filter 精确命中一条），实际收到 " << received_senders.size()
            << " 条 sender（第一条=" << (received_senders.empty() ? mc::string("<none>") : received_senders.front())
            << "），inbound_count=" << inbound_senders.size()
            << " inbound_first=" << (inbound_senders.empty() ? mc::string("<none>") : inbound_senders.front())
            << " inbound_pairs_first="
            << (inbound_pair_counts.empty() ? 0U : inbound_pair_counts.front())
            << " pending_msg=" << (pending.has_value() ? mc::string("<yes>") : mc::string("<no>")) << "）";
        EXPECT_EQ(received_senders.front(), "svc.desired");
    }

    // 额外再等一小段时间，确认未命中的那条 signal 不会"迟到"触发 callback。
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    {
        std::lock_guard lock(cb_mutex);
        ASSERT_EQ(received_senders.size(), 1U)
            << "filter 未命中的 signal 不应触发 callback，但观察到额外回调 " << (received_senders.size() - 1U) << " 次";
    }

    rx.stop();
    subscriber_service.set_proto(nullptr);
    subscriber_service.remove_match(subscription_id);
    subscriber_service.stop();
    mc::engine::engine::set_match_table(nullptr);
}
