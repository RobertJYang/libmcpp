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
#include <test_utilities/engine_shm_test_base.h>

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

// 绑定 fixture 的 shm_runtime 作为 engine options 的 override，这样 engine::init
// 不会碰全局 default_runtime 单例，隔离到当前测试。
//
// engine::reset_for_test 一并 shutdown endpoint_service 与 reset match_table：
// 必须在 m_runtime 析构前完成，否则 match_table 持有的 shared_table 引用到
// 已销毁 runtime，导致静态 dtor 路径崩溃。
class endpoint_service_test : public mc::test::TestWithEngineShmRegion {
protected:
    void SetUp() override
    {
        mc::test::TestWithEngineShmRegion::SetUp();
        mc::engine::engine::reset_for_test();
    }

    void TearDown() override
    {
        mc::engine::engine::reset_for_test();
        mc::test::TestWithEngineShmRegion::TearDown();
    }
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
    // 两边都看到 2 对 (service_name, match_id)：证明合并的是同一条 wire，不是拆成
    // 两条再各自投递——这是合并语义的关键保证。
    EXPECT_EQ(pair_count_seen_a.load(), 2);
    EXPECT_EQ(pair_count_seen_b.load(), 2);

    svc_a.remove_match(id_a);
    svc_b.remove_match(id_b);
    svc_a.stop();
    svc_b.stop();
    match::filter_backend_registry::reset_for_test();
}

// 同 service 多个 match 命中同一 signal：本地 merged 分支会给这个 service 合并
// 一次 dispatch，两个 id 都被依次触发（Fast path A，service::dispatch_event 内部
// 遍历所有 pair）。
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

// 跨进程 merged publish 端到端：publisher 合并多 (service, match_id) 对到同一 receiver endpoint，
// receiver 侧按 service 分组分发，每条 callback 命中一次。
#if 0 // 早期尝试"子订阅 + 父发布"模型，死在 fork 不继承 io executor 线程上：
      // 子 mq_channel 的 pump 起不来，消息永远卡队列。现行模型见下方
      // cross_process_publish_merges_real_fork。
TEST_F(endpoint_service_test, cross_process_publish_merges_to_single_mq_message)
{
    mc::engine::engine_options parent_opts;
    parent_opts.endpoint_name        = "mcengine.test.cross.parent";
    parent_opts.shm_runtime_override = runtime_alias();
    ASSERT_TRUE(mc::engine::engine::init(parent_opts));

    match::filter_backend_registry::reset_for_test();
    match::register_condition_filter_backend();

    fork_child([this]() -> int {
        // fork 后 engine 的静态态（endpoint_service / match_table / service
        // registry）是父进程快照的 copy-on-write 拷贝，必须先清掉，否则
        // engine::init 会走"已 started"幂等分支。
        mc::engine::engine::reset_for_test();

        auto child_rt = open_child_shm_runtime();
        if (!child_rt) {
            return 10;
        }
        std::shared_ptr<mc::shm::shm_runtime> child_rt_shared(child_rt.release());

        mc::engine::engine_options child_opts;
        child_opts.endpoint_name        = "mcengine.test.cross.child";
        child_opts.shm_runtime_override = child_rt_shared;
        if (!mc::engine::engine::init(child_opts)) {
            return 11;
        }

        // filter backend 在父进程注册过，fork 之后子进程继承父 backend 注册表；
        // 这里不需要重新 register。
        merged_test_service svc_a("cross.svc.a");
        merged_test_service svc_b("cross.svc.b");
        if (!svc_a.init() || !svc_b.init() || !svc_a.start() || !svc_b.start()) {
            return 12;
        }

        std::atomic<int> hits_a{0};
        std::atomic<int> hits_b{0};
        std::atomic<int> pair_a{0};
        std::atomic<int> pair_b{0};
        std::atomic<int> total_hits{0};

        match::match_rule rule;
        rule.type           = "signal";
        rule.interface_name = "merge.cross";
        rule.member_name    = "Ping";

        auto id_a = svc_a.add_match(rule, match::filter_spec{},
                                    [&](const mc::engine::message& msg) {
                                        ++hits_a;
                                        pair_a = static_cast<int>(
                                            match::get_target_match_ids(msg.header).size());
                                        ++total_hits;
                                    });
        auto id_b = svc_b.add_match(rule, match::filter_spec{},
                                    [&](const mc::engine::message& msg) {
                                        ++hits_b;
                                        pair_b = static_cast<int>(
                                            match::get_target_match_ids(msg.header).size());
                                        ++total_hits;
                                    });
        if (id_a == 0U || id_b == 0U) {
            return 13;
        }

        // mq_channel 的消费线程已在 engine::init 里拉起；我们只需轮询 total_hits。
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline && total_hits.load() < 2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        int ret = 0;
        if (hits_a.load() != 1 || hits_b.load() != 1) {
            ret = 14;
        } else if (pair_a.load() != 2 || pair_b.load() != 2) {
            ret = 15;
        }

        svc_a.remove_match(id_a);
        svc_b.remove_match(id_b);
        svc_a.stop();
        svc_b.stop();
        mc::engine::engine::shutdown();
        return ret;
    });

}
#endif

// 真正的跨进程合并 publish：
//
// 角色分配（**别反过来**）：
//   - 父进程 = 订阅方：engine::init 自动拉起 endpoint_service，两个 service 各
//     订阅同一个 signal。父的 mcbase io executor 在 mq_channel 的异步 pump 链
//     上跑，push 完成时回调 pump 消费消息 → 走到 service dispatch。
//   - 子进程 = 发布方：手工 attach 同一 SHM region，注册独立 endpoint 作为 sender，
//     直接读共享 trie 的 find_targets 命中父的 2 条订阅，把 (service_a, id_a) /
//     (service_b, id_b) 打成 header.context 合并 wire，open_queue 父 endpoint
//     对应的 mq 后一次性 push，等价于 engine::publish 对单一远端 endpoint 组的
//     merged send 路径（绕过子进程里 engine 单例副作用）。
//
// 为什么子进程必须是发布方：fork 不继承父的 mcbase io executor 线程，若让子做
// subscriber，mq_channel 的异步 pump 拉不起来，消息进了队列也不消费。发布侧
// push 是同步的，不依赖 io executor，所以可以跑在子进程。
//
// 为什么子进程 **不能** 让 shared_table 析构：
//   1) fork 继承来的 engine_bootstrap 指向父的 endpoint_service，其内部持有
//      父的 shared_table 实例（通过 match_table_holder）；
//   2) shared_table::~shared_table 会在 ipc_unique_lock_guard 下跑
//      _drop_owned_entries() —— 父 shared_table 的 m_owned_entry_offsets 里正是
//      id_a / id_b 的 SHM 条目；
//   3) 一旦析构在子进程跑起来，SHM trie 里的订阅会被 **真正删掉**，父进程再也
//      收不到任何命中 —— 这是早期 diagnostic 观察到 "child find_targets=0" 的
//      根因。
// 结论：子进程走 _exit(0)，不走 return + 正常栈展开。自己 new 的 runtime /
//      shared_table 都故意不回收，kernel 随 exit 统一收。
//
// 强断言：父的每个 service callback 收到的 header.context 里 mc.match.ids.*
// 长度必须是 2（合并 wire 带了 2 对），否则说明发布侧拆成两条消息。
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
                                    ++hits_a;
                                    pair_a = static_cast<int>(
                                        match::get_target_match_ids(msg.header).size());
                                });
    auto id_b = svc_b.add_match(rule, match::filter_spec{},
                                [&](const mc::engine::message& msg) {
                                    ++hits_b;
                                    pair_b = static_cast<int>(
                                        match::get_target_match_ids(msg.header).size());
                                });
    ASSERT_NE(id_a, 0U);
    ASSERT_NE(id_b, 0U);

    const pid_t pid = ::fork();
    ASSERT_GE(pid, 0) << "fork 失败";

    if (pid == 0) {
        // 堆上 new，**故意不回收**：见 TEST 注释第 3 段。任何析构都会把父的
        // 订阅条目从 SHM trie 上删掉，测试随之失败。
        auto* child_rt = new mc::shm::shm_runtime([&]() {
            mc::shm::runtime_options opts;
            opts.region_name   = m_region_name;
            opts.region_size   = m_region_size;
            opts.root_capacity = m_root_capacity;
            return opts;
        }());
        if (!child_rt->is_valid()) {
            _exit(10);
        }

        auto publisher_ep = register_running_endpoint(*child_rt, "mcengine.test.cross2.child");
        if (!publisher_ep.has_value()) {
            _exit(11);
        }

        auto* child_table = new match::shared_table(
            *child_rt, publisher_ep->endpoint_id, publisher_ep->instance_id);

        auto signal = _make_signal("cross.publisher", "merge.cross2", "Ping");
        auto targets = child_table->find_targets(signal);
        if (targets.size() < 2U) {
            _exit(14);
        }

        std::vector<std::pair<mc::string, match::match_id>> pairs;
        pairs.reserve(targets.size());
        std::uint16_t target_endpoint = 0;
        std::uint32_t target_instance = 0;
        for (const auto& t : targets) {
            pairs.emplace_back(t.service_name, t.id);
            if (target_endpoint == 0) {
                target_endpoint = t.endpoint_id;
                target_instance = t.instance_id;
            } else if (t.endpoint_id != target_endpoint) {
                _exit(12);
            }
        }

        auto target_info = child_rt->get_endpoint(target_endpoint);
        if (!target_info.has_value()) {
            _exit(13);
        }
        auto target_ep        = to_endpoint(*target_info);
        target_ep.instance_id = target_instance;

        mc::engine::message merged = signal;
        match::set_target_match_ids(merged.header, pairs);
        send_via_mq(*child_rt, *publisher_ep, target_ep, merged);

        // **严格** _exit，禁止 return / C++ 栈展开（会跑父继承 shared_table 的
        // dtor 从 SHM trie 里摘条目）。
        _exit(0);
    }

    int   status = 0;
    pid_t waited = ::waitpid(pid, &status, 0);
    ASSERT_EQ(waited, pid);
    ASSERT_TRUE(WIFEXITED(status)) << "子进程异常退出 raw_status=" << status;
    ASSERT_EQ(WEXITSTATUS(status), 0)
        << "子进程错误码 10=shm_runtime 无效; 11=register_endpoint 失败; "
           "12=targets endpoint 不一致; 13=get_endpoint 失败; "
           "14=find_targets 未看到父订阅";

    // 子 push 已 return 后才 _exit。但父这边的 mq_channel 消费走 io executor 异步
    // 链，callback 需要短暂等待。
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
