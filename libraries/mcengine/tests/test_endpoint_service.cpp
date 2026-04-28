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

// endpoint_service / engine::init / engine::publish 的合并派发端到端。
//
// 覆盖场景：
//   - engine::init 在 SHM ON 下会自动创建并启动 endpoint_service，
//     get_endpoint_service() 返回的 endpoint_id != 0；
//   - 同一进程的两个 service 订阅同一个 signal：engine::publish 会把两次命中
//     合并成一次本地 dispatch，两个 callback 都被触发（验证 pair wire 合并语义）；
//   - engine::init 名字冲突：对同一 runtime 第二次 init 不同实例但同名失败。
//
// 本文件只在 use_shm=true 下编译（参见 libraries/mcengine/meson.build）。

#include <gtest/gtest.h>

#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <mc/engine/endpoint_service.h>
#include <mc/engine/engine.h>
#include <mc/engine/engine_options.h>
#include <mc/engine/match.h>
#include <mc/engine/message.h>
#include <mc/engine/service.h>
#include <test_utilities/engine_test_base.h>

#include "match/shared_table.h"
#include <mc/shm/region.h>

namespace {

namespace match = mc::engine::match;

class merged_test_service final : public mc::engine::service {
public:
    explicit merged_test_service(mc::string_view name) : mc::engine::service(name)
    {}
};

mc::engine::message _make_signal(const mc::string& sender,
                                 const mc::string& iface,
                                 const mc::string& member)
{
    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::signal;
    msg.header.sender         = sender;
    msg.header.path           = "/merge/test";
    msg.header.interface_name = iface;
    msg.header.member_name    = member;
    return msg;
}

mc::engine::message _make_properties_changed_signal(const mc::string& sender, const mc::string& iface_name,
                                                    const mc::dict& changed)
{
    auto msg                  = _make_signal(sender, "org.freedesktop.DBus.Properties", "PropertiesChanged");
    msg.body                  = mc::engine::make_payload<mc::engine::signal_payload>(
        "sa{sv}as", mc::variants{mc::variant(iface_name), mc::variant(changed), mc::variant(mc::variants{})});
    return msg;
}

mc::engine::message _make_mq_method_call(mc::string destination)
{
    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::method_call;
    msg.header.destination    = std::move(destination);
    msg.header.sender         = "mq.client";
    msg.header.path           = "/mq/test/object";
    msg.header.interface_name = "mq.test.Interface";
    msg.header.member_name    = "Ping";
    msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>("", mc::variants{});
    return msg;
}

// 绑定 fixture 的 shm_runtime 作为 engine options 的 override，这样 engine::init
// 不会碰全局 default_runtime 单例，隔离到当前测试。
//
// engine::reset_for_test 一并 shutdown endpoint_service 与 reset match_table：
// 必须在 m_runtime 析构前完成，否则 match_table 持有的 shared_table 引用到
// 已销毁 runtime，导致静态 dtor 路径崩溃。
class endpoint_service_test : public mc::test::TestWithEngine {
    // 基类 TestWithEngine::SetUp/TearDown 已负责 engine::reset_for_test +
    // install_runtime；子类不再重复 reset，否则会把 base 装好的 runtime alias
    // 撤掉，本 case SHM 路径失效。
};

}  // namespace

// engine::init 在 SHM ON 下自动创建 endpoint_service，endpoint_id != 0，名字可读。
TEST_F(endpoint_service_test, init_bootstraps_endpoint_service)
{
    mc::engine::engine_options options;
    options.endpoint_name         = "mcengine.test.bootstrap";
    options.shm_runtime_override  = runtime_alias();

    ASSERT_TRUE(mc::engine::engine::init(options));
    auto* svc = mc::engine::engine::get_endpoint_service();
    ASSERT_NE(svc, nullptr);
    EXPECT_NE(svc->endpoint_id(), 0U);
    EXPECT_NE(svc->instance_id(), 0U);
    EXPECT_EQ(svc->name(), "mcengine.test.bootstrap");
}

// 同 runtime 内二次 init 不同名字不会顶掉第一次（started 已 true，幂等返回）。
TEST_F(endpoint_service_test, init_is_idempotent)
{
    mc::engine::engine_options opt1;
    opt1.endpoint_name        = "mcengine.test.idemp";
    opt1.shm_runtime_override = runtime_alias();
    ASSERT_TRUE(mc::engine::engine::init(opt1));
    auto* svc1 = mc::engine::engine::get_endpoint_service();
    ASSERT_NE(svc1, nullptr);
    auto first_eid = svc1->endpoint_id();

    mc::engine::engine_options opt2;
    opt2.endpoint_name        = "mcengine.test.idemp.second";
    opt2.shm_runtime_override = runtime_alias();
    EXPECT_TRUE(mc::engine::engine::init(opt2));
    auto* svc2 = mc::engine::engine::get_endpoint_service();
    ASSERT_EQ(svc1, svc2);
    EXPECT_EQ(svc2->endpoint_id(), first_eid);
}

TEST_F(endpoint_service_test, mq_send_with_reply_completes_from_transport_pending)
{
    mc::engine::endpoint_service client("mcengine.test.mq.client", runtime_alias());
    mc::engine::endpoint_service server("mcengine.test.mq.server", runtime_alias());
    ASSERT_TRUE(client.init());
    ASSERT_TRUE(server.init());
    ASSERT_TRUE(client.start());
    ASSERT_TRUE(server.start());

    auto request = _make_mq_method_call("missing.mq.target");
    auto future = client.send_with_reply_to_endpoint(server.endpoint_id(), server.instance_id(), std::move(request),
                                                     mc::milliseconds(1000));

    ASSERT_EQ(future.wait_for(std::chrono::seconds(2)), mc::future_status::ready);
    auto reply = future.get();
    EXPECT_EQ(reply.header.type, mc::engine::message_type::error);
    EXPECT_EQ(reply.header.error_name, mc::string("mc.engine.unknown_object"));
    EXPECT_EQ(reply.header.reply_serial, 1U);

    server.stop();
    client.stop();
}

TEST_F(endpoint_service_test, mq_send_with_reply_times_out_and_discards_late_reply)
{
    mc::engine::endpoint_service client("mcengine.test.mq.timeout.client", runtime_alias());
    ASSERT_TRUE(client.init());
    ASSERT_TRUE(client.start());

    auto receiver = register_running_endpoint(*m_runtime, "mcengine.test.mq.timeout.receiver");
    ASSERT_TRUE(receiver.has_value());

    auto request = _make_mq_method_call("missing.mq.target");
    auto future = client.send_with_reply_to_endpoint(receiver->endpoint_id, receiver->instance_id, std::move(request),
                                                     mc::milliseconds(5));

    ASSERT_EQ(future.wait_for(std::chrono::seconds(2)), mc::future_status::ready);
    auto timeout_reply = future.get();
    EXPECT_EQ(timeout_reply.header.type, mc::engine::message_type::error);
    EXPECT_EQ(timeout_reply.header.error_name, mc::string("mc.engine.timeout"));
    EXPECT_EQ(timeout_reply.header.reply_serial, 1U);

    mc::engine::message late_reply;
    late_reply.header.type         = mc::engine::message_type::method_return;
    late_reply.header.reply_serial = 1U;
    ASSERT_TRUE(client.send_to_endpoint(client.endpoint_id(), client.instance_id(), late_reply));

    auto second = _make_mq_method_call("missing.mq.target");
    auto second_future = client.send_with_reply_to_endpoint(receiver->endpoint_id, receiver->instance_id,
                                                            std::move(second), mc::milliseconds(5));
    ASSERT_EQ(second_future.wait_for(std::chrono::seconds(2)), mc::future_status::ready);
    auto second_timeout = second_future.get();
    EXPECT_EQ(second_timeout.header.error_name, mc::string("mc.engine.timeout"));
    EXPECT_EQ(second_timeout.header.reply_serial, 2U);

    client.stop();
}

// 合并派发：同进程两个 service 订阅同一 signal，publish 一次 → 两个 callback 都命中。
// 因为命中目标都指向本进程 endpoint，合并点在 engine::publish 的本地分支
// （_dispatch_local_merged → route_inbound → 按 service 分组各 dispatch 一次）。
//
// 强断言：两个 callback 看到的消息 header.context 里 mc.match.ids.* 必须是**同
// 一条合并 wire**（size == 2，两 service 都在里面），不是被拆成两条。
TEST_F(endpoint_service_test, publish_merges_local_dispatch_across_services)
{
    mc::engine::engine_options options;
    options.endpoint_name        = "mcengine.test.publish.merged";
    options.shm_runtime_override = runtime_alias();
    ASSERT_TRUE(mc::engine::engine::init(options));

    match::filter_backend_registry::reset_for_test();
    match::register_condition_filter_backend();

    merged_test_service svc_a("merge.svc.a");
    merged_test_service svc_b("merge.svc.b");
    ASSERT_TRUE(svc_a.init());
    ASSERT_TRUE(svc_b.init());
    ASSERT_TRUE(svc_a.start());
    ASSERT_TRUE(svc_b.start());

    std::atomic<int> hits_a{0};
    std::atomic<int> hits_b{0};
    std::atomic<int> pair_count_seen_a{0};
    std::atomic<int> pair_count_seen_b{0};

    match::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = "merge.iface";
    rule.member_name    = "Ping";

    auto capture_pair_count = [](std::atomic<int>& out, const mc::engine::message& msg) {
        out = static_cast<int>(match::get_target_match_ids(msg.header).size());
    };

    auto id_a = svc_a.add_match(rule, match::filter_spec{},
                                [&](const mc::engine::message& msg) {
                                    ++hits_a;
                                    capture_pair_count(pair_count_seen_a, msg);
                                });
    auto id_b = svc_b.add_match(rule, match::filter_spec{},
                                [&](const mc::engine::message& msg) {
                                    ++hits_b;
                                    capture_pair_count(pair_count_seen_b, msg);
                                });
    ASSERT_NE(id_a, 0U);
    ASSERT_NE(id_b, 0U);

    mc::engine::engine::publish(_make_signal("publisher", "merge.iface", "Ping"));

    EXPECT_EQ(hits_a.load(), 1);
    EXPECT_EQ(hits_b.load(), 1);
    // 同一条 wire 携带两个命中 id。
    EXPECT_EQ(pair_count_seen_a.load(), 2);
    EXPECT_EQ(pair_count_seen_b.load(), 2);

    svc_a.remove_match(id_a);
    svc_b.remove_match(id_b);
    svc_a.stop();
    svc_b.stop();
    match::filter_backend_registry::reset_for_test();
}

// 同一 service 的多个 match 命中应合并为一次本地 dispatch。
TEST_F(endpoint_service_test, publish_merges_multi_match_same_service)
{
    mc::engine::engine_options options;
    options.endpoint_name        = "mcengine.test.multi.match";
    options.shm_runtime_override = runtime_alias();
    ASSERT_TRUE(mc::engine::engine::init(options));

    match::filter_backend_registry::reset_for_test();
    match::register_condition_filter_backend();

    merged_test_service svc("merge.svc.multi");
    ASSERT_TRUE(svc.init());
    ASSERT_TRUE(svc.start());

    std::atomic<int> hits_first{0};
    std::atomic<int> hits_second{0};

    match::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = "merge.iface";
    rule.member_name    = "Ping";

    auto id1 = svc.add_match(rule, match::filter_spec{},
                             [&hits_first](const mc::engine::message&) { ++hits_first; });
    auto id2 = svc.add_match(rule, match::filter_spec{},
                             [&hits_second](const mc::engine::message&) { ++hits_second; });
    ASSERT_NE(id1, 0U);
    ASSERT_NE(id2, 0U);

    mc::engine::engine::publish(_make_signal("publisher", "merge.iface", "Ping"));
    EXPECT_EQ(hits_first.load(), 1);
    EXPECT_EQ(hits_second.load(), 1);

    svc.remove_match(id1);
    svc.remove_match(id2);
    svc.stop();
    match::filter_backend_registry::reset_for_test();
}

// 跨进程 publish 应按 endpoint 合并匹配项并通过 MQ 投递。
TEST_F(endpoint_service_test, cross_process_publish_merges_real_fork)
{
    mc::engine::engine_options parent_opts;
    parent_opts.endpoint_name        = "mcengine.test.cross2.parent";
    parent_opts.shm_runtime_override = runtime_alias();
    ASSERT_TRUE(mc::engine::engine::init(parent_opts));

    match::filter_backend_registry::reset_for_test();
    match::register_condition_filter_backend();

    merged_test_service svc_a("cross2.svc.a");
    merged_test_service svc_b("cross2.svc.b");
    ASSERT_TRUE(svc_a.init());
    ASSERT_TRUE(svc_b.init());
    ASSERT_TRUE(svc_a.start());
    ASSERT_TRUE(svc_b.start());

    std::atomic<int> hits_a{0};
    std::atomic<int> hits_b{0};
    std::atomic<int> pair_a{0};
    std::atomic<int> pair_b{0};

    match::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = "merge.cross2";
    rule.member_name    = "Ping";

    auto id_a = svc_a.add_match(rule, match::filter_spec{},
                                [&](const mc::engine::message& msg) {
                                    pair_a = static_cast<int>(
                                        match::get_target_match_ids(msg.header).size());
                                    ++hits_a;
                                });
    auto id_b = svc_b.add_match(rule, match::filter_spec{},
                                [&](const mc::engine::message& msg) {
                                    pair_b = static_cast<int>(
                                        match::get_target_match_ids(msg.header).size());
                                    ++hits_b;
                                });
    ASSERT_NE(id_a, 0U);
    ASSERT_NE(id_b, 0U);

    const pid_t pid = ::fork();
    ASSERT_GE(pid, 0) << "fork 失败";

    if (pid == 0) {
        mc::engine::engine::reset_after_fork();

        // 子进程使用 _exit，避免析构继承的 SHM 状态。
        auto* child_rt_raw = new mc::shm::shm_runtime([&]() {
            mc::shm::runtime_options opts;
            opts.region_name   = m_region_name;
            opts.region_size   = m_region_size;
            opts.root_capacity = m_root_capacity;
            return opts;
        }());
        if (!child_rt_raw->is_valid()) {
            _exit(10);
        }
        std::shared_ptr<mc::shm::shm_runtime> child_rt(child_rt_raw, [](mc::shm::shm_runtime*) {});

        mc::engine::engine_options child_opts;
        child_opts.endpoint_name        = "mcengine.test.cross2.child";
        child_opts.shm_runtime_override = child_rt;
        if (!mc::engine::engine::init(child_opts)) {
            _exit(11);
        }

        match::register_condition_filter_backend();

        auto signal = _make_signal("cross.publisher", "merge.cross2", "Ping");
        auto table = mc::engine::engine::get_match_table();
        if (!table) {
            _exit(12);
        }
        auto targets = table->find_targets(signal);
        if (targets.size() < 2U) {
            _exit(13);
        }
        mc::engine::engine::publish(signal);

        _exit(0);
    }

    int   status = 0;
    pid_t waited = ::waitpid(pid, &status, 0);
    ASSERT_EQ(waited, pid);
    ASSERT_TRUE(WIFEXITED(status)) << "子进程异常退出 raw_status=" << status;
    ASSERT_EQ(WEXITSTATUS(status), 0)
        << "子进程错误码 10=shm_runtime 无效; 11=engine::init 失败; "
           "12=get_match_table 失败; 13=find_targets 未看到父订阅";

    // 父进程异步消费 MQ，需要短暂等待 callback。
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline
           && (hits_a.load() == 0 || hits_b.load() == 0)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(hits_a.load(), 1);
    EXPECT_EQ(hits_b.load(), 1);
    EXPECT_EQ(pair_a.load(), 2) << "合并 wire 必须携带 2 对 (service, match_id)";
    EXPECT_EQ(pair_b.load(), 2) << "合并 wire 必须携带 2 对 (service, match_id)";

    svc_a.remove_match(id_a);
    svc_b.remove_match(id_b);
    svc_a.stop();
    svc_b.stop();
    match::filter_backend_registry::reset_for_test();
}

TEST_F(endpoint_service_test, properties_changed_cross_process_publish_over_mq)
{
    mc::engine::engine_options parent_opts;
    parent_opts.endpoint_name        = "mcengine.test.props.parent";
    parent_opts.shm_runtime_override = runtime_alias();
    ASSERT_TRUE(mc::engine::engine::init(parent_opts));

    match::filter_backend_registry::reset_for_test();
    match::register_condition_filter_backend();

    merged_test_service svc("props.svc.receiver");
    ASSERT_TRUE(svc.init());
    ASSERT_TRUE(svc.start());

    std::atomic<int> hits{0};
    std::atomic<int> pair_count{0};
    std::atomic<int> body_ok{0};

    match::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = "org.freedesktop.DBus.Properties";
    rule.member_name    = "PropertiesChanged";

    auto id = svc.add_match(rule, match::filter_spec{}, [&](const mc::engine::message& msg) {
        pair_count = static_cast<int>(match::get_target_match_ids(msg.header).size());
        if (const auto* payload = msg.try_as<mc::engine::signal_payload>()) {
            if (payload->signature == "sa{sv}as" && payload->args.size() == 3 &&
                payload->args[0] == mc::variant(mc::string("org.test.Properties")) && payload->args[1].is_dict() &&
                payload->args[2].is_array()) {
                auto changed = payload->args[1].as<mc::dict>();
                if (changed.find("Counter") != changed.end() && changed["Counter"] == int32_t{42}) {
                    body_ok = 1;
                }
            }
        }
        ++hits;
    });
    ASSERT_NE(id, 0U);

    const pid_t pid = ::fork();
    ASSERT_GE(pid, 0) << "fork 失败";

    if (pid == 0) {
        mc::engine::engine::reset_after_fork();

        auto* child_rt_raw = new mc::shm::shm_runtime([&]() {
            mc::shm::runtime_options opts;
            opts.region_name   = m_region_name;
            opts.region_size   = m_region_size;
            opts.root_capacity = m_root_capacity;
            return opts;
        }());
        if (!child_rt_raw->is_valid()) {
            _exit(10);
        }
        std::shared_ptr<mc::shm::shm_runtime> child_rt(child_rt_raw, [](mc::shm::shm_runtime*) {});

        mc::engine::engine_options child_opts;
        child_opts.endpoint_name        = "mcengine.test.props.child";
        child_opts.shm_runtime_override = child_rt;
        if (!mc::engine::engine::init(child_opts)) {
            _exit(11);
        }

        match::register_condition_filter_backend();

        auto signal = _make_properties_changed_signal(
            "props.publisher", "org.test.Properties", mc::dict{{"Counter", mc::variant(int32_t{42})}});
        auto table = mc::engine::engine::get_match_table();
        if (!table) {
            _exit(12);
        }
        auto targets = table->find_targets(signal);
        if (targets.size() != 1U) {
            _exit(13);
        }
        mc::engine::engine::publish(signal);
        _exit(0);
    }

    int   status = 0;
    pid_t waited = ::waitpid(pid, &status, 0);
    ASSERT_EQ(waited, pid);
    ASSERT_TRUE(WIFEXITED(status)) << "子进程异常退出 raw_status=" << status;
    ASSERT_EQ(WEXITSTATUS(status), 0)
        << "子进程错误码 10=shm_runtime 无效; 11=engine::init 失败; "
           "12=get_match_table 失败; 13=find_targets 数量不符合预期";

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline && hits.load() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(hits.load(), 1);
    EXPECT_EQ(pair_count.load(), 1);
    EXPECT_EQ(body_ok.load(), 1);

    svc.remove_match(id);
    svc.stop();
    match::filter_backend_registry::reset_for_test();
}

// endpoint_name 冲突策略：register_endpoint / find_endpoint_by_name / is_endpoint_alive 三件套
// 决定拒启或改名。三个测试分别覆盖三种分支。

// 策略 refuse：第二个同名实例在活进程冲突下 on_start 返回 false。
TEST_F(endpoint_service_test, name_conflict_refuse_blocks_second_live_endpoint)
{
    auto rt = runtime_alias();

    mc::engine::endpoint_service first("mcengine.test.conflict.refuse", rt);
    first.set_name_conflict_policy(
        mc::engine::endpoint_name_conflict_policy::refuse);
    ASSERT_TRUE(first.init());
    ASSERT_TRUE(first.start());
    ASSERT_NE(first.endpoint_id(), 0U);

    mc::engine::endpoint_service second("mcengine.test.conflict.refuse", rt);
    second.set_name_conflict_policy(
        mc::engine::endpoint_name_conflict_policy::refuse);
    ASSERT_TRUE(second.init());
    EXPECT_FALSE(second.start())
        << "活实例同名，refuse 策略下 on_start 必须失败";
    EXPECT_EQ(second.endpoint_id(), 0U);

    first.stop();
}

// 策略 append_pid：第二个同名实例改名为 "${name}-${pid}" 后启动成功，
// effective_endpoint_name 反映实际落表的名字。
TEST_F(endpoint_service_test, name_conflict_append_pid_starts_with_suffix)
{
    auto rt = runtime_alias();

    mc::engine::endpoint_service first("mcengine.test.conflict.appendpid", rt);
    ASSERT_TRUE(first.init());
    ASSERT_TRUE(first.start());
    ASSERT_NE(first.endpoint_id(), 0U);
    EXPECT_EQ(first.effective_endpoint_name(),
              mc::string_view("mcengine.test.conflict.appendpid"));

    mc::engine::endpoint_service second("mcengine.test.conflict.appendpid", rt);
    second.set_name_conflict_policy(
        mc::engine::endpoint_name_conflict_policy::append_pid);
    ASSERT_TRUE(second.init());
    ASSERT_TRUE(second.start())
        << "append_pid 策略下，改名后应能独立启动";
    EXPECT_NE(second.endpoint_id(), 0U);
    EXPECT_NE(second.endpoint_id(), first.endpoint_id());

    const auto expected_suffix =
        mc::string("-") + mc::to_string(static_cast<std::int64_t>(::getpid()));
    const auto effective = mc::string(second.effective_endpoint_name());
    ASSERT_GE(effective.size(), expected_suffix.size());
    EXPECT_EQ(effective.substr(effective.size() - expected_suffix.size()),
              expected_suffix);
    EXPECT_NE(effective, mc::string_view("mcengine.test.conflict.appendpid"));

    second.stop();
    first.stop();
}

// 死 slot 自动接管：第一个 stop 后（abort_endpoint 把 slot 标死），第二个
// 同名实例 refuse 策略也能直接启动（复用 endpoint_id）。崩溃恢复路径。
TEST_F(endpoint_service_test, dead_slot_is_reused_across_restart)
{
    auto rt = runtime_alias();

    std::uint16_t slot_id = 0U;
    {
        mc::engine::endpoint_service first("mcengine.test.conflict.takeover", rt);
        ASSERT_TRUE(first.init());
        ASSERT_TRUE(first.start());
        slot_id = first.endpoint_id();
        ASSERT_NE(slot_id, 0U);
        first.stop();
    }

    mc::engine::endpoint_service second("mcengine.test.conflict.takeover", rt);
    second.set_name_conflict_policy(
        mc::engine::endpoint_name_conflict_policy::refuse);
    ASSERT_TRUE(second.init());
    ASSERT_TRUE(second.start())
        << "前一实例已 abort，第二个实例应接管同名 slot";
    EXPECT_EQ(second.endpoint_id(), slot_id)
        << "崩溃恢复路径必须复用同一 endpoint_id";
    EXPECT_NE(second.instance_id(), 0U);

    second.stop();
}
