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

// mcengine USE_SHM=ON 下 abstract_object 与 shm_object 的端到端校验：
//   - 单对象 register / set_property 后 SHM payload 可读
//   - property 多次修改取到最新值
//   - unregister 释放 shm_object handle
//   - fork() 子进程通过共享 mmap 读到父进程写入的 property
//   - engine.recover() 重建 heap 对象并回填 property
//   - 未注册 class 与 CRC 损坏的 shm_object 被 isolate，活对象不受影响
//
// USE_SHM=OFF 时本文件不进编译单元（由 meson 按 use_shm 开关控制），
// 因此无需再在源码里加 #if MCENGINE_USE_SHM 守卫。

#include <gtest/gtest.h>

#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>

#include <mc/engine.h>
#include <mc/engine/shm_object.h>

#include "shm_object_ops.h"
#include "shm_property_sync.h"
#include "shm_runtime_provider.h"
#include <mc/exception.h>
#include <mc/format.h>
#include <mc/shm/shm_runtime.h>
#include <test_utilities/engine_test_base.h>

namespace shm_engine_lifecycle_test {

template <typename T>
using property = mc::engine::property<T>;

class lifecycle_iface : public mc::engine::interface<lifecycle_iface> {
public:
    MC_INTERFACE("org.test.lifecycle_iface")

    property<int32_t>     m_counter;
    property<std::string> m_label;
    property<double>      m_score;
};

class lifecycle_object : public mc::engine::object<lifecycle_object> {
public:
    MC_OBJECT(lifecycle_object, "ShmLifecycleObject", "/org/test/shm_lifecycle", (lifecycle_iface))

    lifecycle_iface m_iface;
};

struct lifecycle_service : public mc::engine::service {
    lifecycle_service() : mc::engine::service("org.openubmc.shm_lifecycle_test")
    {}
};

} // namespace shm_engine_lifecycle_test

MC_REFLECT(shm_engine_lifecycle_test::lifecycle_iface, (m_counter, "counter")(m_label, "label")(m_score, "score"))
MC_REFLECT(shm_engine_lifecycle_test::lifecycle_object, (m_iface, "iface"))

namespace shm_engine_lifecycle_test {

class shm_engine_lifecycle_test : public mc::test::TestWithEngine {
protected:
    void SetUp() override
    {
        TestWithEngine::SetUp();
        // 每用例都 reset：unlink SHM region + 销毁 engine 单例，确保
        // recover/isolate/CRC 类用例之间不会通过 SHM 残留互相污染。
        mc::engine::engine::reset_for_test();
        m_service = std::make_unique<lifecycle_service>();
        m_service->init();
        ASSERT_TRUE(m_service->start());
    }

    void TearDown() override
    {
        if (m_service) {
            m_service->stop();
            m_service.reset();
        }
        mc::engine::engine::reset_for_test();
        TestWithEngine::TearDown();
    }

    std::unique_ptr<lifecycle_service> m_service;
};

TEST_F(shm_engine_lifecycle_test, register_object_writes_identity_and_properties_to_shm)
{
    auto obj = lifecycle_object::create();
    obj->set_object_path("/org/test/lifecycle/one");
    obj->set_object_name("lifecycle_one");
    obj->set_position("0001");
    m_service->register_object(obj);

    // m_shm_handle 已由 service::register_object 路径分配
    auto* shadow = obj->get_shm_handle();
    ASSERT_NE(shadow, nullptr);
    EXPECT_EQ(mc::engine::shm_object_class_name(*shadow), "ShmLifecycleObject");
    EXPECT_EQ(mc::engine::shm_object_name(*shadow), "lifecycle_one");
    EXPECT_EQ(mc::engine::shm_object_path(*shadow), "/org/test/lifecycle/one");
    EXPECT_EQ(mc::engine::shm_object_position(*shadow), "0001");

    // property 写路径 → SHM
    obj->m_iface.m_counter.set_value(42);
    obj->m_iface.m_label.set_value("hello-shm");
    obj->m_iface.m_score.set_value(3.14);

    int64_t                       int_out = 0;
    double                        dbl_out = 0.0;
    std::string_view              str_out;
    mc::engine::property_type_tag tag_out{};

    auto k_counter = mc::engine::shm_property_compose_key("org.test.lifecycle_iface", "counter");
    auto k_score   = mc::engine::shm_property_compose_key("org.test.lifecycle_iface", "score");
    auto k_label   = mc::engine::shm_property_compose_key("org.test.lifecycle_iface", "label");

    EXPECT_TRUE(mc::engine::shm_object_get_property_int64(*shadow, k_counter, int_out));
    EXPECT_EQ(int_out, 42);

    EXPECT_TRUE(mc::engine::shm_object_get_property_double(*shadow, k_score, dbl_out));
    EXPECT_DOUBLE_EQ(dbl_out, 3.14);

    EXPECT_TRUE(mc::engine::shm_object_get_property_blob(*shadow, k_label, str_out, tag_out));
    EXPECT_EQ(tag_out, mc::engine::property_type_tag::string);
    EXPECT_EQ(str_out, "hello-shm");
}

TEST_F(shm_engine_lifecycle_test, property_mutation_reflected_in_shm)
{
    auto obj = lifecycle_object::create();
    obj->set_object_path("/org/test/lifecycle/two");
    obj->set_object_name("lifecycle_two");
    obj->set_position("0002");
    m_service->register_object(obj);

    auto* shadow = obj->get_shm_handle();
    ASSERT_NE(shadow, nullptr);

    obj->m_iface.m_counter.set_value(1);
    obj->m_iface.m_counter.set_value(2);
    obj->m_iface.m_counter.set_value(99);
    obj->m_iface.m_label.set_value("first");
    obj->m_iface.m_label.set_value("second");
    obj->m_iface.m_label.set_value("final-value");

    int64_t                       int_out = 0;
    std::string_view              str_out;
    mc::engine::property_type_tag tag_out{};

    auto k_counter = mc::engine::shm_property_compose_key("org.test.lifecycle_iface", "counter");
    auto k_label   = mc::engine::shm_property_compose_key("org.test.lifecycle_iface", "label");

    EXPECT_TRUE(mc::engine::shm_object_get_property_int64(*shadow, k_counter, int_out));
    EXPECT_EQ(int_out, 99);

    EXPECT_TRUE(mc::engine::shm_object_get_property_blob(*shadow, k_label, str_out, tag_out));
    EXPECT_EQ(str_out, "final-value");
}

TEST_F(shm_engine_lifecycle_test, unregister_drops_shm_handle)
{
    auto obj = lifecycle_object::create();
    obj->set_object_path("/org/test/lifecycle/three");
    obj->set_object_name("lifecycle_three");
    obj->set_position("0003");
    m_service->register_object(obj);

    ASSERT_NE(obj->get_shm_handle(), nullptr);

    m_service->unregister_object(obj->get_object_path());

    // unregister 必须释放 SHM 体 + 清空 m_shm_handle
    EXPECT_EQ(obj->get_shm_handle(), nullptr);
}

// fork 跨进程基础校验：父进程写 property，fork 后子进程通过共享 SHM mmap
// 直接读 shm_object payload，验证 SHM 写入对其他进程可见。
//
// 子进程不调任何 mcengine 接口，只用 shm_object_ops 读 SHM payload；
// shadow 裸指针在子进程里仍然指向同一物理页（fork 复制 mmap 视图）。
TEST_F(shm_engine_lifecycle_test, fork_child_reads_parent_property)
{
    auto obj = lifecycle_object::create();
    obj->set_object_path("/org/test/lifecycle/forked");
    obj->set_object_name("lifecycle_forked");
    obj->set_position("0007");
    m_service->register_object(obj);

    obj->m_iface.m_counter.set_value(7);
    obj->m_iface.m_label.set_value("parent-wrote-this");

    auto* shadow = obj->get_shm_handle();
    ASSERT_NE(shadow, nullptr);

    pid_t child = ::fork();
    ASSERT_GE(child, 0);

    if (child == 0) {
        int64_t                       counter = 0;
        std::string_view              label;
        mc::engine::property_type_tag tag{};

        auto k_counter = mc::engine::shm_property_compose_key("org.test.lifecycle_iface", "counter");
        auto k_label   = mc::engine::shm_property_compose_key("org.test.lifecycle_iface", "label");

        if (mc::engine::shm_object_class_name(*shadow) != "ShmLifecycleObject") {
            std::_Exit(20);
        }
        if (mc::engine::shm_object_path(*shadow) != "/org/test/lifecycle/forked") {
            std::_Exit(21);
        }
        if (!mc::engine::shm_object_get_property_int64(*shadow, k_counter, counter)) {
            std::_Exit(22);
        }
        if (counter != 7) {
            std::_Exit(23);
        }
        if (!mc::engine::shm_object_get_property_blob(*shadow, k_label, label, tag)) {
            std::_Exit(24);
        }
        if (tag != mc::engine::property_type_tag::string || label != "parent-wrote-this") {
            std::_Exit(25);
        }

        std::_Exit(0);
    }

    int status = -1;
    ASSERT_EQ(::waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status)) << "子进程异常退出 raw status=" << status;
    EXPECT_EQ(WEXITSTATUS(status), 0) << "子进程退出码：20=class_name 不匹配，21=path，22/24=property 缺失，"
                                         "23=counter 值错，25=label 值错";
}

// recover() 重建路径：
// register 多个对象后调用 engine.recover() 强制清空 heap pool 并通过
// reconstruct_fn 从 SHM 重建。重建出来的新实例必须可经 table.find<by_path>()
// 命中，且 identity + property 与原值一致。
TEST_F(shm_engine_lifecycle_test, recover_rebuilds_all_objects_with_properties)
{
    constexpr int kCount = 5;

    struct seed {
        std::string path;
        std::string name;
        std::string position;
        int32_t     counter;
        std::string label;
        double      score;
    };

    std::vector<seed> seeds;
    seeds.reserve(kCount);
    for (int i = 0; i < kCount; ++i) {
        seeds.push_back({
            sformat("/org/test/lifecycle/recover_{}", i),
            sformat("recover_{}", i),
            sformat("{:04d}", 1000 + i),
            100 + i,
            sformat("label-{}", i),
            1.5 * (i + 1),
        });
    }

    for (const auto& s : seeds) {
        auto obj = lifecycle_object::create();
        obj->set_object_path(s.path);
        obj->set_object_name(s.name);
        obj->set_position(s.position);
        m_service->register_object(obj);
        obj->m_iface.m_counter.set_value(s.counter);
        obj->m_iface.m_label.set_value(s.label);
        obj->m_iface.m_score.set_value(s.score);
    }

    auto& tbl = m_service->get_object_table();
    tbl.engine().recover();

    for (const auto& s : seeds) {
        auto it = tbl.find<mc::engine::by_path>(mc::string(s.path));
        ASSERT_FALSE(it.is_end()) << "recover 后 path 找不到：" << s.path;

        EXPECT_EQ(it->get_object_name(), s.name);
        EXPECT_EQ(it->get_position(), s.position);
        EXPECT_EQ(it->get_class_name(), "ShmLifecycleObject");

        // identity 在 reconstruct_fn 中已对齐，property 来自 shm_load_properties_into
        auto* casted = dynamic_cast<const lifecycle_object*>(&*it);
        ASSERT_NE(casted, nullptr) << "reconstruct 出来的不是 lifecycle_object";
        EXPECT_EQ(casted->m_iface.m_counter.value(), s.counter);
        EXPECT_EQ(casted->m_iface.m_label.value(), s.label);
        EXPECT_DOUBLE_EQ(casted->m_iface.m_score.value(), s.score);
    }
}

// shm_object 的 class_name 被改成未注册类名，recover 后该条目应被标 isolated
// 且 reconstruct_fn 返回空，其他正常对象不受影响。
TEST_F(shm_engine_lifecycle_test, unregistered_class_isolated_on_recover)
{
    auto good = lifecycle_object::create();
    good->set_object_path("/org/test/lifecycle/iso_class_good");
    good->set_object_name("iso_class_good");
    good->set_position("5001");
    m_service->register_object(good);
    good->m_iface.m_counter.set_value(123);

    auto bad = lifecycle_object::create();
    bad->set_object_path("/org/test/lifecycle/iso_class_bad");
    bad->set_object_name("iso_class_bad");
    bad->set_position("5002");
    m_service->register_object(bad);

    auto* bad_shadow = bad->get_shm_handle();
    ASSERT_NE(bad_shadow, nullptr);

    // 把 class_name 改成永远不会被注册的字符串（CRC 由 ops 自动刷新）
    auto& rt    = mc::engine::shm_runtime_provider::instance();
    auto  arena = rt.user_arena();
    ASSERT_TRUE(mc::engine::shm_object_set_class_name(arena, *bad_shadow, "NeverRegisteredClass"));

    auto& tbl = m_service->get_object_table();
    tbl.engine().recover();

    // 好对象仍然可达，property 已回填
    {
        auto it = tbl.find<mc::engine::by_path>(mc::string("/org/test/lifecycle/iso_class_good"));
        ASSERT_FALSE(it.is_end());
        auto* casted = dynamic_cast<const lifecycle_object*>(&*it);
        ASSERT_NE(casted, nullptr);
        EXPECT_EQ(casted->m_iface.m_counter.value(), 123);
    }

    // 坏对象的 SHM 体仍在 SHM 中，但 isolated 位已被置上；
    // raw_iterator 在 clear/find 中会自动跳过它，避免空 shared_ptr 解引用。
    EXPECT_NE((bad_shadow->flags & mc::engine::shm_object_flags::isolated), 0U)
        << "未注册 class 的 shm_object 应该被 reconstruct_fn 隔离";
}

// shm_object 的 crc32_self 被人为破坏，recover 后该条目被 isolate。
TEST_F(shm_engine_lifecycle_test, corrupted_crc_isolated_on_recover)
{
    auto good = lifecycle_object::create();
    good->set_object_path("/org/test/lifecycle/iso_crc_good");
    good->set_object_name("iso_crc_good");
    good->set_position("6001");
    m_service->register_object(good);
    good->m_iface.m_label.set_value("survives");

    auto bad = lifecycle_object::create();
    bad->set_object_path("/org/test/lifecycle/iso_crc_bad");
    bad->set_object_name("iso_crc_bad");
    bad->set_position("6002");
    m_service->register_object(bad);

    auto* bad_shadow = bad->get_shm_handle();
    ASSERT_NE(bad_shadow, nullptr);

    // 直接把主体 CRC 翻 bit，模拟存储介质损坏 / kill 中断写入
    bad_shadow->crc32_self ^= 0xDEADBEEFu;
    EXPECT_FALSE(mc::engine::shm_object_check(*bad_shadow));

    auto& tbl = m_service->get_object_table();
    tbl.engine().recover();

    // 好对象仍可达
    {
        auto it = tbl.find<mc::engine::by_path>(mc::string("/org/test/lifecycle/iso_crc_good"));
        ASSERT_FALSE(it.is_end());
        auto* casted = dynamic_cast<const lifecycle_object*>(&*it);
        ASSERT_NE(casted, nullptr);
        EXPECT_EQ(casted->m_iface.m_label.value(), "survives");
    }

    EXPECT_NE((bad_shadow->flags & mc::engine::shm_object_flags::isolated), 0U)
        << "CRC 损坏的 shm_object 应该被 reconstruct_fn 隔离";
}

// gc_isolated 把 isolated shm_object 从所有索引清掉 + 释放 SHM 占用。
TEST_F(shm_engine_lifecycle_test, gc_isolated_purges_orphans_from_indices_and_arena)
{
    auto& rt    = mc::engine::shm_runtime_provider::instance();
    auto  arena = rt.user_arena();

    constexpr int kCount = 4;
    for (int i = 0; i < kCount; ++i) {
        auto good = lifecycle_object::create();
        good->set_object_path(sformat("/org/test/lifecycle/gc_good/{}", i));
        good->set_object_name(sformat("gc_good_{}", i));
        good->set_position(sformat("g-{:03d}", i));
        m_service->register_object(good);

        auto bad = lifecycle_object::create();
        bad->set_object_path(sformat("/org/test/lifecycle/gc_bad/{}", i));
        bad->set_object_name(sformat("gc_bad_{}", i));
        bad->set_position(sformat("b-{:03d}", i));
        m_service->register_object(bad);

        auto* sh = bad->get_shm_handle();
        ASSERT_NE(sh, nullptr);
        ASSERT_TRUE(mc::engine::shm_object_set_class_name(arena, *sh, "NeverRegisteredClass"));
    }

    auto& tbl = m_service->get_object_table();
    tbl.engine().recover();

    const std::size_t arena_before_gc = arena.allocated_size();
    const std::size_t reclaimed       = m_service->gc_isolated();
    EXPECT_EQ(reclaimed, static_cast<std::size_t>(kCount))
        << "gc_isolated 必须回收恰好 " << kCount << " 个 isolated 对象";

    // 索引层验证：bad path 应当在所有 3 个 idx 都消失；good 仍可达。
    for (int i = 0; i < kCount; ++i) {
        EXPECT_TRUE(tbl.find<mc::engine::by_path>(mc::string(sformat("/org/test/lifecycle/gc_bad/{}", i))).is_end())
            << "isolated 对象 path 仍残留在 by_path 索引";
        EXPECT_TRUE(tbl.find<mc::engine::by_object_name>(mc::string(sformat("gc_bad_{}", i))).is_end())
            << "isolated 对象 name 仍残留在 by_object_name 索引";

        EXPECT_FALSE(tbl.find<mc::engine::by_path>(mc::string(sformat("/org/test/lifecycle/gc_good/{}", i))).is_end())
            << "正常对象不应被误删";
    }

    // 内存层验证：每个 isolated shm_object 至少占 192B 自身 + 4 个 byte_string + slab 元数据。
    // 用一个保守下限（kCount * 200B）确认 arena 真的回吐了空间。
    const std::size_t arena_after_gc = arena.allocated_size();
    EXPECT_LT(arena_after_gc, arena_before_gc) << "GC 后 arena 占用应该下降";
    EXPECT_GT(arena_before_gc - arena_after_gc, static_cast<std::size_t>(kCount) * 200U)
        << "GC 释放的字节数太少，怀疑 ShmRecord 自身没有 destroy";

    // 二次调用应是 no-op（已无 isolated 对象）。
    EXPECT_EQ(m_service->gc_isolated(), 0U);
}

} // namespace shm_engine_lifecycle_test
