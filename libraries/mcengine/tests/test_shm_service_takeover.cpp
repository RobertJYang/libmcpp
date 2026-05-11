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

// service takeover + 跨 service 全局表查询。USE_SHM=OFF 时不参与编译。

#include <gtest/gtest.h>

#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include <mc/engine.h>
#include <mc/engine/shm_object.h>

#include "shm_object_ops.h"
#include "shm_runtime_provider.h"
#include "shm_service.h"
#include "shm_service_ops.h"
#include <mc/exception.h>
#include <mc/format.h>
#include <mc/intrusive/offset_ptr.h>
#include <mc/shm/byte_string.h>
#include <mc/shm/container/map.h>
#include <mc/shm/shm_runtime.h>
#include <test_utilities/engine_test_base.h>

namespace shm_service_takeover_test {

template <typename T>
using property = mc::engine::property<T>;

class takeover_iface : public mc::engine::interface<takeover_iface> {
public:
    MC_INTERFACE("org.test.takeover_iface")

    property<int32_t>     m_counter;
    property<std::string> m_label;
};

class takeover_object : public mc::engine::object<takeover_object> {
public:
    MC_OBJECT(takeover_object, "TakeoverObject", "/org/test/takeover", (takeover_iface))

    takeover_iface m_iface;
};

struct service_a : public mc::engine::service {
    service_a() : mc::engine::service("org.openubmc.takeover_test.svc_a")
    {}
};

struct service_b : public mc::engine::service {
    service_b() : mc::engine::service("org.openubmc.takeover_test.svc_b")
    {}
};

} // namespace shm_service_takeover_test

MC_REFLECT(shm_service_takeover_test::takeover_iface, (m_counter, "counter")(m_label, "label"))
MC_REFLECT(shm_service_takeover_test::takeover_object, (m_iface, "iface"))

namespace shm_service_takeover_test {

// 直接打开 shm_service_map（与 service.cpp 内 _open_service_map 同名同语义）
// 用于在测试里读取 SHM 中 shm_service POD 验证 pid/epoch。
mc::engine::shm_service_map _open_service_map_for_test()
{
    using mc::engine::shm_byte_string;
    using mc::engine::shm_byte_string_less;
    using mc::engine::shm_ptr;
    using mc::engine::shm_service;
    auto& rt = mc::engine::shm_runtime_provider::instance();
    auto  opt =
        rt.get_or_create_map<shm_byte_string, shm_ptr<shm_service>, shm_byte_string_less>("mcengine.service_map");
    if (!opt) {
        ADD_FAILURE() << "shm_service_map 不可用";
    }
    return std::move(*opt);
}

mc::engine::shm_service* _lookup_shm_service(mc::string_view name)
{
    auto m = _open_service_map_for_test();
    auto p = m.find(name);
    if (!p) {
        return nullptr;
    }
    return p.value->get();
}

// fork 测试 helper：父进程 fork 后仍 alive，子进程默认 attach 会被 liveness 拒。
// 先 force-attach 把 svc.pid 改为子进程 pid，service.start 内部的 attach 即可
// 通过 "同 pid 跳过 liveness" 路径走正常 takeover，模拟 crash 后接管。
void _force_takeover_for_fork_child(mc::string_view service_name)
{
    auto& rt    = mc::engine::shm_runtime_provider::instance();
    auto  arena = rt.user_arena();
    auto  smap  = _open_service_map_for_test();
    (void)mc::engine::shm_service_attach(arena, smap, service_name, static_cast<std::uint32_t>(::getpid()),
                                         /*force=*/true);
}

class shm_service_takeover_test : public mc::test::TestWithEngine {
    // 基类 TestWithEngine 已管理 engine::reset_for_test + install_runtime；
    // 子类不再重复，避免 reset 把 base 装好的 runtime alias 撤掉。
};

// 同进程两次 attach：epoch++、pid 覆盖、视图一致。
TEST_F(shm_service_takeover_test, attach_twice_increments_epoch_and_overwrites_pid)
{
    auto svc = std::make_unique<service_a>();
    ASSERT_TRUE(svc->start());

    auto* svc_pod = _lookup_shm_service(svc->name());
    ASSERT_NE(svc_pod, nullptr) << "service.start 后 shm_service 应已落到 SHM 表中";
    EXPECT_EQ(svc_pod->state, static_cast<std::uint8_t>(mc::engine::shm_service_state::alive));
    EXPECT_EQ(svc_pod->pid, static_cast<std::uint32_t>(::getpid()));
    EXPECT_EQ(svc_pod->epoch, 1U);

    // 模拟"另一个进程"第二次 attach：直接调 shm_service_attach 用一个不同 pid
    auto&                   rt          = mc::engine::shm_runtime_provider::instance();
    auto                    arena       = rt.user_arena();
    auto                    smap        = _open_service_map_for_test();
    constexpr std::uint32_t k_other_pid = 999999U;
    // 本用例验证底层 takeover 写入语义（pid 覆盖 / epoch++），与 liveness
    // 拒绝行为正交，因此走 force=true 跳过检查。
    auto* takeover =
        mc::engine::shm_service_attach(arena, smap, mc::string_view(svc->name()), k_other_pid, /*force=*/true);
    ASSERT_NE(takeover, nullptr);
    EXPECT_EQ(takeover, svc_pod) << "同名 service 必须接管同一 shm_service POD";
    EXPECT_EQ(svc_pod->pid, k_other_pid) << "pid 必须被覆盖（前任进程在共享内存中能看到）";
    EXPECT_EQ(svc_pod->epoch, 2U) << "epoch 必须 ++";
    EXPECT_TRUE(mc::engine::shm_service_check(*svc_pod)) << "CRC 必须自动刷新";

    // service 实例自身保留指针（fork 复制后子进程可以走相同 POD），
    // 但 stop() 会触发主动 detach（state=detached + pid=0），不影响 takeover 测试主体。
    svc->stop();
}

// 两次同名 attach，第二次 pid 不同会静默接管，前一个持有者通过 epoch 比对可感知。

TEST_F(shm_service_takeover_test, second_attach_with_different_pid_takes_over_silently)
{
    auto&                     rt      = mc::engine::shm_runtime_provider::instance();
    auto                      arena   = rt.user_arena();
    auto                      smap    = _open_service_map_for_test();
    constexpr mc::string_view k_name  = "org.openubmc.takeover_test.dup_attach";
    constexpr std::uint32_t   k_pid_a = 11111U;
    constexpr std::uint32_t   k_pid_b = 22222U;

    // 测试两次直接 attach 的 POD 复用语义（不经过 service.start）。
    // pid_a / pid_b 都是任意虚构数；在本机 _pid_alive 多半返回 false 故此处也能跑过，
    // 但为了语义清晰 + 不依赖运行环境 pid 分布，显式 force=true。
    auto* svc_a = mc::engine::shm_service_attach(arena, smap, k_name, k_pid_a, /*force=*/true);
    ASSERT_NE(svc_a, nullptr);
    EXPECT_EQ(svc_a->pid, k_pid_a);
    EXPECT_EQ(svc_a->epoch, 1U);
    auto epoch_observed_by_a = svc_a->epoch;

    auto* svc_b = mc::engine::shm_service_attach(arena, smap, k_name, k_pid_b, /*force=*/true);
    ASSERT_NE(svc_b, nullptr);
    EXPECT_EQ(svc_b, svc_a) << "误用场景下两次 attach 必须返回同一 POD";
    EXPECT_EQ(svc_b->pid, k_pid_b) << "pid 被后者覆盖";
    EXPECT_EQ(svc_b->epoch, 2U) << "epoch 必须 ++";

    // A 视角：通过原指针读到的 pid/epoch 已是 B 写入的最新值（共享内存语义）
    EXPECT_EQ(svc_a->pid, k_pid_b);
    EXPECT_GT(svc_a->epoch, epoch_observed_by_a) << "A 应当能通过 epoch 漂移检测到自己已被接管";
}

// fork takeover 完整链路：父注册对象→子 attach→通过 service 表读对象。
// 子进程通过退出码反馈结果，父进程 wait + 断言。
TEST_F(shm_service_takeover_test, fork_child_takes_over_and_recovers_objects)
{
    auto svc = std::make_unique<service_a>();
    ASSERT_TRUE(svc->start());

    constexpr int k_count = 3;
    for (int i = 0; i < k_count; ++i) {
        auto obj = takeover_object::create();
        obj->set_object_path(sformat("/org/test/takeover/obj_{}", i));
        obj->set_object_name(sformat("obj_{}", i));
        obj->set_position(sformat("{:04d}", 7000 + i));
        svc->register_object(obj);
        obj->m_iface.m_counter.set_value(100 + i);
        obj->m_iface.m_label.set_value(sformat("parent-wrote-{}", i));
    }

    pid_t child = ::fork();
    ASSERT_GE(child, 0);

    if (child == 0) {
        // 子进程：构造同名 service 实例，触发 attach + recover。
        // 注意：子进程不能 reset_for_test（会 unlink 整个 region），
        // 也不能销毁父持有的 svc。直接新建一个同名 service 实例就够了：
        // ensure_registered 自带 attach + recover。
        // 父进程在 fork 后实际仍活，需要先用 force-attach 模拟 crash 接管。
        try {
            _force_takeover_for_fork_child("org.openubmc.takeover_test.svc_a");
            auto child_svc = std::make_unique<service_a>();
            if (!child_svc->start()) {
                std::_Exit(31);
            }

            auto& tbl = child_svc->get_object_table();
            for (int i = 0; i < k_count; ++i) {
                auto path = sformat("/org/test/takeover/obj_{}", i);
                auto it   = tbl.find<mc::engine::by_path>(mc::string(path));
                if (it.is_end()) {
                    std::_Exit(40 + i);
                }
                auto* casted = dynamic_cast<const takeover_object*>(&*it);
                if (casted == nullptr) {
                    std::_Exit(50 + i);
                }
                if (casted->m_iface.m_counter.value() != 100 + i) {
                    std::_Exit(60 + i);
                }
                if (casted->m_iface.m_label.value() != sformat("parent-wrote-{}", i)) {
                    std::_Exit(70 + i);
                }
            }

            // 验证 takeover 写入：pid=child_pid, epoch>=2
            auto* pod = _lookup_shm_service("org.openubmc.takeover_test.svc_a");
            if (pod == nullptr) {
                std::_Exit(80);
            }
            if (pod->pid != static_cast<std::uint32_t>(::getpid())) {
                std::_Exit(81);
            }
            if (pod->epoch < 2U) {
                std::_Exit(82);
            }

            std::_Exit(0);
        } catch (...) {
            std::_Exit(99);
        }
    }

    int status = -1;
    ASSERT_EQ(::waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status)) << "子进程异常退出 raw status=" << status;
    EXPECT_EQ(WEXITSTATUS(status), 0) << "子进程退出码：31=start 失败，40~42=path 找不到，50~52=类型不匹配，"
                                         "60~62=counter 不一致，70~72=label 不一致，80=service_pod 缺失，"
                                         "81=pid 未 takeover，82=epoch 未自增，99=异常";

    // 父进程关停（detach 路径）
    svc->stop();
}

// parent/children 关系 reopen：父注册对象树→子 attach→BFS 看到完整关系。
TEST_F(shm_service_takeover_test, fork_child_recovers_parent_children_relations)
{
    auto svc = std::make_unique<service_a>();
    ASSERT_TRUE(svc->start());

    struct node {
        std::string path;
        std::string parent_path; // "" 表示无 parent
    };
    const std::vector<node> tree = {
        {"/org/test/takeover/tree", ""},
        {"/org/test/takeover/tree/a", "/org/test/takeover/tree"},
        {"/org/test/takeover/tree/a/a1", "/org/test/takeover/tree/a"},
        {"/org/test/takeover/tree/a/a2", "/org/test/takeover/tree/a"},
        {"/org/test/takeover/tree/b", "/org/test/takeover/tree"},
    };

    for (const auto& n : tree) {
        auto obj = takeover_object::create();
        obj->set_object_path(n.path);
        obj->set_object_name(n.path);
        obj->set_position("9999");
        svc->register_object(obj);
    }

    pid_t child = ::fork();
    ASSERT_GE(child, 0);

    if (child == 0) {
        try {
            _force_takeover_for_fork_child("org.openubmc.takeover_test.svc_a");
            auto child_svc = std::make_unique<service_a>();
            if (!child_svc->start()) {
                std::_Exit(31);
            }

            auto& tbl = child_svc->get_object_table();

            int idx = 0;
            for (const auto& n : tree) {
                auto it = tbl.find<mc::engine::by_path>(mc::string(n.path));
                if (it.is_end()) {
                    std::_Exit(40 + idx);
                }
                auto& obj      = const_cast<mc::engine::abstract_object&>(*it);
                auto* owner    = obj.get_owner();
                auto  expected = n.parent_path;
                if (expected.empty()) {
                    if (owner != nullptr) {
                        std::_Exit(50 + idx);
                    }
                } else {
                    if (owner == nullptr) {
                        std::_Exit(60 + idx);
                    }
                    if (owner->get_object_path() != mc::string_view(expected)) {
                        std::_Exit(70 + idx);
                    }
                }
                ++idx;
            }
            std::_Exit(0);
        } catch (...) {
            std::_Exit(99);
        }
    }

    int status = -1;
    ASSERT_EQ(::waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status)) << "子进程异常退出 raw status=" << status;
    EXPECT_EQ(WEXITSTATUS(status), 0) << "子进程退出码：31=start 失败，40+=path 找不到，50+=root 应当无 owner，"
                                         "60+=应有 owner 实际为空，70+=owner 路径不匹配，99=异常";

    svc->stop();
}

// 跨 service 全局表查询：service A 注册的对象能从全局 object_table 被读到。
TEST_F(shm_service_takeover_test, cross_service_lookup_via_global_object_table)
{
    auto svc_a_inst = std::make_unique<service_a>();
    auto svc_b_inst = std::make_unique<service_b>();
    ASSERT_TRUE(svc_a_inst->start());
    ASSERT_TRUE(svc_b_inst->start());

    constexpr mc::string_view k_path_a = "/org/test/takeover/global_query_a";
    constexpr mc::string_view k_path_b = "/org/test/takeover/global_query_b";

    auto obj_a = takeover_object::create();
    obj_a->set_object_path(mc::string(k_path_a));
    obj_a->set_object_name("global_query_a");
    obj_a->set_position("8001");
    svc_a_inst->register_object(obj_a);
    obj_a->m_iface.m_counter.set_value(1001);

    auto obj_b = takeover_object::create();
    obj_b->set_object_path(mc::string(k_path_b));
    obj_b->set_object_name("global_query_b");
    obj_b->set_position("8002");
    svc_b_inst->register_object(obj_b);
    obj_b->m_iface.m_counter.set_value(1002);

    auto& global = mc::engine::engine::get_instance().get_object_table();

    // A 通过全局表查到 B 拥有的对象
    {
        auto it = global.find<mc::engine::by_path>(mc::string(k_path_b));
        ASSERT_FALSE(it.is_end()) << "全局 object_table 里应该能查到 service_b 的对象";
        EXPECT_EQ(it->get_class_name(), "TakeoverObject");
        auto* casted = dynamic_cast<const takeover_object*>(&*it);
        ASSERT_NE(casted, nullptr);
        EXPECT_EQ(casted->m_iface.m_counter.value(), 1002);
    }
    // B 通过全局表查到 A 拥有的对象
    {
        auto it = global.find<mc::engine::by_path>(mc::string(k_path_a));
        ASSERT_FALSE(it.is_end());
        auto* casted = dynamic_cast<const takeover_object*>(&*it);
        ASSERT_NE(casted, nullptr);
        EXPECT_EQ(casted->m_iface.m_counter.value(), 1001);
    }

    // 同时验证：每个 service 自己的表里只看得到自己注册的对象
    EXPECT_FALSE(svc_a_inst->get_object_table().find<mc::engine::by_path>(mc::string(k_path_a)).is_end());
    EXPECT_TRUE(svc_a_inst->get_object_table().find<mc::engine::by_path>(mc::string(k_path_b)).is_end())
        << "service_a 自己的表里不应看到 service_b 注册的对象";

    svc_a_inst->stop();
    svc_b_inst->stop();
}

} // namespace shm_service_takeover_test
