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

// mcengine SHM 业务层端到端测试。USE_SHM=OFF 时不参与编译。

#include <gtest/gtest.h>

#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

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

namespace shm_engine_business_test {

template <typename T>
using property = mc::engine::property<T>;

class biz_iface : public mc::engine::interface<biz_iface> {
public:
    MC_INTERFACE("org.test.biz_iface")

    property<int32_t>     m_counter;
    property<std::string> m_label;
    property<double>      m_score;
};

class biz_object : public mc::engine::object<biz_object> {
public:
    MC_OBJECT(biz_object, "BizObject", "/org/test/biz", (biz_iface))

    biz_iface m_iface;
};

struct svc_alpha : public mc::engine::service {
    svc_alpha() : mc::engine::service("org.openubmc.biz_test.alpha")
    {}
};

struct svc_beta : public mc::engine::service {
    svc_beta() : mc::engine::service("org.openubmc.biz_test.beta")
    {}
};

} // namespace shm_engine_business_test

MC_REFLECT(shm_engine_business_test::biz_iface, (m_counter, "counter")(m_label, "label")(m_score, "score"))
MC_REFLECT(shm_engine_business_test::biz_object, (m_iface, "iface"))

namespace shm_engine_business_test {

// 业务测试本地版打开 shm_service_map（与 service.cpp 内部同名同语义）。
// fork 子进程的 force-attach 路径需要拿到 map 句柄；
// 单进程测试的 _lookup_shm_service 也直接复用此 helper。
mc::engine::shm_service_map _open_service_map_for_test_biz()
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
    auto m = _open_service_map_for_test_biz();
    auto p = m.find(name);
    if (!p) {
        return nullptr;
    }
    return p.value->get();
}

mc::shared_ptr<biz_object> _make_obj(mc::string_view path, mc::string_view name, mc::string_view position)
{
    auto o = biz_object::create();
    o->set_object_path(path);
    o->set_object_name(name);
    o->set_position(position);
    return o;
}

class shm_engine_business_test : public mc::test::TestWithEngine {
protected:
    void SetUp() override
    {
        TestWithEngine::SetUp();
        mc::engine::engine::reset_for_test();
    }

    void TearDown() override
    {
        mc::engine::engine::reset_for_test();
        TestWithEngine::TearDown();
    }
};

// register 后 shm_object.service 指向当前进程的 shm_service POD。
// 通过全局 object_table 反查时应指向同一个 POD。

TEST_F(shm_engine_business_test, register_links_shm_object_to_owning_shm_service)
{
    auto svc = std::make_unique<svc_alpha>();
    ASSERT_TRUE(svc->start());

    auto obj = _make_obj("/org/test/biz/svc_link/one", "svc_link_one", "0001");
    svc->register_object(obj);

    auto* sh = obj->get_shm_handle();
    ASSERT_NE(sh, nullptr);

    auto* shm_svc_in_map = _lookup_shm_service("org.openubmc.biz_test.alpha");
    ASSERT_NE(shm_svc_in_map, nullptr);

    auto* shm_svc_in_obj = mc::engine::shm_object_service(*sh);
    EXPECT_EQ(shm_svc_in_obj, shm_svc_in_map) << "shm_object.service 应指向 shm_service_map 中的同名 POD";
    EXPECT_EQ(shm_svc_in_obj->pid, static_cast<std::uint32_t>(::getpid()));

    svc->stop();
}

// unregister 时把 shm_object.service 置 null，再 destroy 释放对象。
// 这里通过 register → 显式 unregister → 再 register 同 path 新对象 来观察：
//   - 旧对象释放路径里没有把别的对象的 service 字段顺带打翻
//   - 新对象的 shm_object.service 仍指向当前 shm_service POD
TEST_F(shm_engine_business_test, unregister_then_register_keeps_service_link_consistent)
{
    auto svc = std::make_unique<svc_alpha>();
    ASSERT_TRUE(svc->start());

    auto obj1 = _make_obj("/org/test/biz/recycle", "recycle_v1", "0010");
    svc->register_object(obj1);
    auto* shm_svc_pod = mc::engine::shm_object_service(*obj1->get_shm_handle());
    ASSERT_NE(shm_svc_pod, nullptr);

    svc->unregister_object("/org/test/biz/recycle");

    auto obj2 = _make_obj("/org/test/biz/recycle", "recycle_v2", "0010");
    svc->register_object(obj2);

    auto* sh2 = obj2->get_shm_handle();
    ASSERT_NE(sh2, nullptr);
    EXPECT_EQ(mc::engine::shm_object_service(*sh2), shm_svc_pod)
        << "新对象的 shm_object.service 仍应指向同一 shm_service POD（POD 不会因对象 recycle 而变）";
    EXPECT_EQ(mc::engine::shm_object_name(*sh2), "recycle_v2");

    svc->stop();
}

// 两个 service 各自一棵子树，全局 object_table 应能看到全部节点且 owner 链完整。

TEST_F(shm_engine_business_test, multi_service_each_owns_subtree_global_table_sees_all)
{
    auto a = std::make_unique<svc_alpha>();
    auto b = std::make_unique<svc_beta>();
    ASSERT_TRUE(a->start());
    ASSERT_TRUE(b->start());

    struct seed {
        std::string path;
        std::string name;
    };
    const std::vector<seed> a_tree = {
        {"/org/test/biz/alpha_root", "ar"},
        {"/org/test/biz/alpha_root/x", "arx"},
        {"/org/test/biz/alpha_root/x/y", "arxy"},
    };
    const std::vector<seed> b_tree = {
        {"/org/test/biz/beta_root", "br"},
        {"/org/test/biz/beta_root/m", "brm"},
        {"/org/test/biz/beta_root/n", "brn"},
        {"/org/test/biz/beta_root/n/p", "brnp"},
    };

    int pos = 1;
    for (const auto& s : a_tree) {
        a->register_object(_make_obj(s.path, s.name, sformat("a-{:03d}", pos++)));
    }
    pos = 1;
    for (const auto& s : b_tree) {
        b->register_object(_make_obj(s.path, s.name, sformat("b-{:03d}", pos++)));
    }

    auto& global = mc::engine::engine::get_instance().get_object_table();

    std::unordered_set<std::string> all_paths;
    for (const auto& s : a_tree) {
        all_paths.insert(s.path);
    }
    for (const auto& s : b_tree) {
        all_paths.insert(s.path);
    }

    for (const auto& p : all_paths) {
        auto it = global.find<mc::engine::by_path>(mc::string(p));
        EXPECT_FALSE(it.is_end()) << "全局表缺失 path=" << p;
    }

    // 每个 service 表内部 owner 链：根节点 owner=null，其他节点 owner 必非 null
    auto _check_subtree = [](mc::engine::service_object_table& tbl, const std::vector<seed>& tree,
                             mc::string_view root_path) {
        for (const auto& s : tree) {
            auto it = tbl.find<mc::engine::by_path>(mc::string(s.path));
            ASSERT_FALSE(it.is_end()) << "service 表缺失 path=" << s.path;
            auto& obj   = const_cast<mc::engine::abstract_object&>(*it);
            auto* owner = obj.get_owner();
            if (mc::string_view(s.path) == root_path) {
                EXPECT_EQ(owner, nullptr) << "根节点 owner 应为 null path=" << s.path;
            } else {
                EXPECT_NE(owner, nullptr) << "非根节点 owner 不应为 null path=" << s.path;
            }
        }
    };
    _check_subtree(a->get_object_table(), a_tree, "/org/test/biz/alpha_root");
    _check_subtree(b->get_object_table(), b_tree, "/org/test/biz/beta_root");

    a->stop();
    b->stop();
}

// 跨 service 父子语义负例：service B 的 find_owner 只查自己的表，
// 不应"借用" service A 的对象做 owner。

TEST_F(shm_engine_business_test, cross_service_parent_lookup_does_not_leak)
{
    auto a = std::make_unique<svc_alpha>();
    auto b = std::make_unique<svc_beta>();
    ASSERT_TRUE(a->start());
    ASSERT_TRUE(b->start());

    a->register_object(_make_obj("/org/test/biz/shared/root", "shared_root", "S001"));
    b->register_object(_make_obj("/org/test/biz/shared/root/leaf", "shared_leaf", "S002"));

    auto& tbl_b = b->get_object_table();
    auto  it    = tbl_b.find<mc::engine::by_path>(mc::string("/org/test/biz/shared/root/leaf"));
    ASSERT_FALSE(it.is_end());
    auto& leaf = const_cast<mc::engine::abstract_object&>(*it);
    EXPECT_EQ(leaf.get_owner(), nullptr)
        << "service B 的 leaf 不应把 service A 的对象当 owner（跨 service 父子不支持）";

    // 全局表层面：两条记录都看得到，业务方可以自己决定是否做 cross-service 关系
    auto& global = mc::engine::engine::get_instance().get_object_table();
    EXPECT_FALSE(global.find<mc::engine::by_path>(mc::string("/org/test/biz/shared/root")).is_end());
    EXPECT_FALSE(global.find<mc::engine::by_path>(mc::string("/org/test/biz/shared/root/leaf")).is_end());

    a->stop();
    b->stop();
}

// by_class_name 范围查询：同 class 多个对象应全部命中。

TEST_F(shm_engine_business_test, service_table_by_class_name_finds_all_same_class_objects)
{
    auto svc = std::make_unique<svc_alpha>();
    ASSERT_TRUE(svc->start());

    constexpr int kCount = 6;
    for (int i = 0; i < kCount; ++i) {
        svc->register_object(
            _make_obj(sformat("/org/test/biz/by_cls/{}", i), sformat("by_cls_{}", i), sformat("c-{:03d}", i)));
    }

    auto& tbl = svc->get_object_table();
    // 没有公开的 equal_range<Tag>，由 find<by_class_name> 拿首个迭代器后向后扫，
    // 凡 class_name 仍等于 "BizObject" 的就算命中。by_class_name 是 (class_name,
    // position) 的非唯一索引，相同 class_name 的条目在底层 map 里物理连续。
    int                             hit = 0;
    std::unordered_set<std::string> seen;
    for (auto it = tbl.find<mc::engine::by_class_name>(mc::string("BizObject"));
         !it.is_end() && it->get_class_name() == "BizObject"; ++it) {
        seen.insert(std::string(it->get_object_path()));
        ++hit;
    }
    EXPECT_EQ(hit, kCount) << "by_class_name 应命中全部 " << kCount << " 个对象";
    EXPECT_EQ(seen.size(), static_cast<std::size_t>(kCount));

    svc->stop();
}

// by_object_name 唯一查询。

TEST_F(shm_engine_business_test, service_table_by_object_name_unique_lookup)
{
    auto svc = std::make_unique<svc_alpha>();
    ASSERT_TRUE(svc->start());

    svc->register_object(_make_obj("/org/test/biz/by_name/a", "uniq_name_a", "n-001"));
    svc->register_object(_make_obj("/org/test/biz/by_name/b", "uniq_name_b", "n-002"));
    svc->register_object(_make_obj("/org/test/biz/by_name/c", "uniq_name_c", "n-003"));

    auto& tbl = svc->get_object_table();

    auto it_b = tbl.find<mc::engine::by_object_name>(mc::string("uniq_name_b"));
    ASSERT_FALSE(it_b.is_end());
    EXPECT_EQ(it_b->get_object_path(), "/org/test/biz/by_name/b");

    auto it_miss = tbl.find<mc::engine::by_object_name>(mc::string("does_not_exist"));
    EXPECT_TRUE(it_miss.is_end()) << "不存在的 object_name 应返回 end()";

    svc->stop();
}

// recover 后 by_path / by_class_name / by_object_name 三个索引同时可用。

TEST_F(shm_engine_business_test, recover_rebuilds_all_three_indices)
{
    auto svc = std::make_unique<svc_alpha>();
    ASSERT_TRUE(svc->start());

    constexpr int            kCount = 4;
    std::vector<std::string> names;
    for (int i = 0; i < kCount; ++i) {
        auto path = sformat("/org/test/biz/recover_idx/{}", i);
        auto name = sformat("recover_idx_obj_{}", i);
        svc->register_object(_make_obj(path, name, sformat("r-{:03d}", i)));
        names.push_back(name);
    }

    auto& tbl = svc->get_object_table();
    tbl.engine().recover();

    for (int i = 0; i < kCount; ++i) {
        auto by_p = tbl.find<mc::engine::by_path>(mc::string(sformat("/org/test/biz/recover_idx/{}", i)));
        EXPECT_FALSE(by_p.is_end()) << "recover 后 by_path 缺 #" << i;

        auto by_n = tbl.find<mc::engine::by_object_name>(mc::string(names[i]));
        EXPECT_FALSE(by_n.is_end()) << "recover 后 by_object_name 缺 #" << i;
    }
    int hit = 0;
    for (auto it = tbl.find<mc::engine::by_class_name>(mc::string("BizObject"));
         !it.is_end() && it->get_class_name() == "BizObject"; ++it) {
        ++hit;
    }
    EXPECT_EQ(hit, kCount) << "recover 后 by_class_name 应命中全部 " << kCount << " 个对象";

    svc->stop();
}

// 大 string property（走 byte_string slab 路径）recover 能完整回填。

TEST_F(shm_engine_business_test, large_string_property_survives_recover)
{
    auto svc = std::make_unique<svc_alpha>();
    ASSERT_TRUE(svc->start());

    auto obj = _make_obj("/org/test/biz/large_prop", "large_prop", "L001");
    svc->register_object(obj);

    // 4KB 字符串：超出任何 inline 缓冲，必然走 byte_string slab + journal
    std::string big_label;
    big_label.resize(4096);
    for (std::size_t i = 0; i < big_label.size(); ++i) {
        big_label[i] = static_cast<char>('a' + (i % 26));
    }
    obj->m_iface.m_label.set_value(big_label);
    obj->m_iface.m_counter.set_value(424242);

    auto& tbl = svc->get_object_table();
    tbl.engine().recover();

    auto it = tbl.find<mc::engine::by_path>(mc::string("/org/test/biz/large_prop"));
    ASSERT_FALSE(it.is_end());
    auto* casted = dynamic_cast<const biz_object*>(&*it);
    ASSERT_NE(casted, nullptr);
    EXPECT_EQ(casted->m_iface.m_label.value(), big_label);
    EXPECT_EQ(casted->m_iface.m_counter.value(), 424242);

    svc->stop();
}

// set_object_name / set_position 修改后 reopen 反映最新值。

TEST_F(shm_engine_business_test, identity_setter_updates_persist_through_recover)
{
    auto svc = std::make_unique<svc_alpha>();
    ASSERT_TRUE(svc->start());

    auto obj = _make_obj("/org/test/biz/identity_mut", "initial_name", "P000");
    svc->register_object(obj);

    auto* sh = obj->get_shm_handle();
    ASSERT_NE(sh, nullptr);
    auto& rt    = mc::engine::shm_runtime_provider::instance();
    auto  arena = rt.user_arena();

    ASSERT_TRUE(mc::engine::shm_object_set_name(arena, *sh, "renamed_after"));
    ASSERT_TRUE(mc::engine::shm_object_set_position(arena, *sh, "P999"));

    auto& tbl = svc->get_object_table();
    tbl.engine().recover();

    auto it = tbl.find<mc::engine::by_path>(mc::string("/org/test/biz/identity_mut"));
    ASSERT_FALSE(it.is_end());
    EXPECT_EQ(it->get_object_name(), "renamed_after");
    EXPECT_EQ(it->get_position(), "P999");

    svc->stop();
}

// register 后业务直接调 set_position：必须自动同步到 shm_object.position
// （set_object_name 由基类 set_name 阻止 register 后改名，无 SHM 同步窗口）。
TEST_F(shm_engine_business_test, business_set_position_syncs_to_shm)
{
    auto svc = std::make_unique<svc_alpha>();
    ASSERT_TRUE(svc->start());

    auto obj = _make_obj("/org/test/biz/biz_position_sync", "stable_name", "P_before");
    svc->register_object(obj);

    auto* sh = obj->get_shm_handle();
    ASSERT_NE(sh, nullptr);
    EXPECT_EQ(mc::engine::shm_object_position(*sh), "P_before");

    obj->set_position("P_after");
    EXPECT_EQ(mc::engine::shm_object_position(*sh), "P_after") << "set_position 必须同步落到 shm_object.position";

    auto& tbl = svc->get_object_table();
    tbl.engine().recover();
    auto it = tbl.find<mc::engine::by_path>(mc::string("/org/test/biz/biz_position_sync"));
    ASSERT_FALSE(it.is_end());
    EXPECT_EQ(it->get_position(), "P_after");

    svc->stop();
}

// 业务 set_owner 跨 path 建立 ownership：
// 不依赖 path-find 反推，必须由 SHM parent / children 字段自身承载，
// recover 后关系仍然成立。
TEST_F(shm_engine_business_test, business_set_owner_cross_path_persists_through_recover)
{
    auto svc = std::make_unique<svc_alpha>();
    ASSERT_TRUE(svc->start());

    // 两个互不相邻的 path：path-find 无法把它们关联起来，迫使 SHM
    // parent/children 字段成为唯一证据。
    auto parent_obj = _make_obj("/org/test/biz/owner_a", "parent_obj", "0001");
    auto child_obj  = _make_obj("/org/test/biz/owner_b", "child_obj", "0002");
    svc->register_object(parent_obj);
    svc->register_object(child_obj);

    // 确认 path-find 没把它们设成父子关系（默认应当都是根）
    ASSERT_EQ(child_obj->get_owner(), nullptr);

    child_obj->set_owner(parent_obj.get());

    auto* parent_sh = parent_obj->get_shm_handle();
    auto* child_sh  = child_obj->get_shm_handle();
    ASSERT_NE(parent_sh, nullptr);
    ASSERT_NE(child_sh, nullptr);
    EXPECT_EQ(child_sh->parent.get(), parent_sh) << "set_owner 必须写 SHM parent";

    auto* slab = parent_sh->children.get();
    ASSERT_NE(slab, nullptr) << "owner.children slab 必须被分配";
    bool found = false;
    for (std::uint16_t i = 0; i < slab->slot_count; ++i) {
        if (slab->slots[i].get() == child_sh) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "owner.children 必须包含 child_sh";

    // 重新 set_owner 到另一个对象：旧 parent 的 children 必须移除本节点
    auto third_obj = _make_obj("/org/test/biz/owner_c", "third_obj", "0003");
    svc->register_object(third_obj);
    child_obj->set_owner(third_obj.get());

    auto* third_sh = third_obj->get_shm_handle();
    EXPECT_EQ(child_sh->parent.get(), third_sh);
    {
        auto* old_slab = parent_sh->children.get();
        if (old_slab != nullptr) {
            for (std::uint16_t i = 0; i < old_slab->slot_count; ++i) {
                EXPECT_NE(old_slab->slots[i].get(), child_sh)
                    << "重新 set_owner 后旧 parent.children 必须把 child 摘掉";
            }
        }
    }

    // recover 后 SHM 真理之源驱动 owner 关系重建：
    // child_obj 的 owner 应是 third_obj，而非默认的 path-find 推断出的根。
    auto& tbl = svc->get_object_table();
    tbl.engine().recover();
    auto child_it = tbl.find<mc::engine::by_path>(mc::string("/org/test/biz/owner_b"));
    auto third_it = tbl.find<mc::engine::by_path>(mc::string("/org/test/biz/owner_c"));
    ASSERT_FALSE(child_it.is_end());
    ASSERT_FALSE(third_it.is_end());

    auto* child_after  = const_cast<mc::engine::abstract_object*>(&(*child_it));
    auto* third_after  = const_cast<mc::engine::abstract_object*>(&(*third_it));
    auto* child_sh_now = child_after->get_shm_handle();
    auto* third_sh_now = third_after->get_shm_handle();
    ASSERT_NE(child_sh_now, nullptr);
    ASSERT_NE(third_sh_now, nullptr);
    EXPECT_EQ(child_sh_now->parent.get(), third_sh_now);

    svc->stop();
}

// unregister 必须把节点从父 children slab 中拿掉，否则恢复时遗留悬空 child entry。
TEST_F(shm_engine_business_test, unregister_removes_from_parent_children_slab)
{
    auto svc = std::make_unique<svc_alpha>();
    ASSERT_TRUE(svc->start());

    auto parent_obj = _make_obj("/org/test/biz/cleanup_parent", "p", "0001");
    auto child_obj  = _make_obj("/org/test/biz/cleanup_parent/c", "c", "0002");
    svc->register_object(parent_obj);
    svc->register_object(child_obj);

    auto* parent_sh = parent_obj->get_shm_handle();
    auto* child_sh  = child_obj->get_shm_handle();
    ASSERT_NE(parent_sh, nullptr);
    ASSERT_NE(child_sh, nullptr);

    auto* slab = parent_sh->children.get();
    ASSERT_NE(slab, nullptr);
    bool found_before = false;
    for (std::uint16_t i = 0; i < slab->slot_count; ++i) {
        if (slab->slots[i].get() == child_sh) {
            found_before = true;
            break;
        }
    }
    ASSERT_TRUE(found_before) << "前置条件：register 应该已经把 child 写入 parent.children";

    svc->unregister_object(child_obj->get_object_path());

    auto* slab_after = parent_sh->children.get();
    if (slab_after != nullptr) {
        for (std::uint16_t i = 0; i < slab_after->slot_count; ++i) {
            EXPECT_NE(slab_after->slots[i].get(), child_sh) << "unregister 必须把本节点从 parent.children 摘掉";
        }
    }

    svc->stop();
}

// 旧 owner 仍存活时 attach 必须被拒，stop 后才允许接管。
TEST_F(shm_engine_business_test, attach_rejects_takeover_while_owner_alive)
{
    auto parent_svc = std::make_unique<svc_alpha>();
    ASSERT_TRUE(parent_svc->start());

    {
        // 父仍 alive：子进程默认 attach 应被拒。
        pid_t child = ::fork();
        ASSERT_GE(child, 0);
        if (child == 0) {
            int rc;
            try {
                auto child_svc = std::make_unique<svc_alpha>();
                rc             = child_svc->start() ? 50 : 31;
            } catch (...) {
                rc = 31;
            }
            std::_Exit(rc);
        }
        int status = -1;
        ASSERT_EQ(::waitpid(child, &status, 0), child);
        ASSERT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 31) << "退出码 31=attach 被拒（预期），50=居然接管成功";
    }

    parent_svc->stop();

    {
        // 父已 stop（state=detached + pid=0）：子进程可接管。
        pid_t child = ::fork();
        ASSERT_GE(child, 0);
        if (child == 0) {
            int rc;
            try {
                auto child_svc = std::make_unique<svc_alpha>();
                rc             = child_svc->start() ? 0 : 60;
            } catch (...) {
                rc = 99;
            }
            std::_Exit(rc);
        }
        int status = -1;
        ASSERT_EQ(::waitpid(child, &status, 0), child);
        ASSERT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0) << "退出码 0=子进程正常接管 detached service（预期），"
                                             "60=start 失败，99=异常";
    }
}

// service.stop() 必须释放所有 shm_object，N 轮 start/register/stop 稳态不增长。
TEST_F(shm_engine_business_test, service_stop_releases_shm_object_memory)
{
    auto& rt = mc::engine::shm_runtime_provider::instance();

    auto _arena_used = [&]() noexcept {
        auto a = rt.user_arena();
        return a.allocated_size();
    };

    constexpr int kRounds      = 4;
    constexpr int kObjPerRound = 32;
    constexpr int kBlobBytes   = 512;
    std::string   blob;
    blob.resize(kBlobBytes, 'x');

    auto _one_round = [&]() {
        auto svc = std::make_unique<svc_alpha>();
        ASSERT_TRUE(svc->start());
        for (int i = 0; i < kObjPerRound; ++i) {
            auto o = _make_obj(sformat("/org/test/biz/leak/{}", i), sformat("leak_{}", i), sformat("L-{:03d}", i));
            svc->register_object(o);
            o->m_iface.m_label.set_value(blob);
            o->m_iface.m_counter.set_value(i);
        }
        svc->stop();
    };

    std::vector<std::size_t> after_stop_samples;
    after_stop_samples.reserve(kRounds);
    for (int r = 0; r < kRounds; ++r) {
        _one_round();
        after_stop_samples.push_back(_arena_used());
    }

    // 稳态：从第 2 轮起每轮 stop 后 arena 占用不应继续增长（容差 4KB 用于
    // map 内部 free-list / 对齐 padding 的微量噪声）。
    constexpr std::size_t kTolerance = 4U * 1024U;
    for (std::size_t i = 1; i < after_stop_samples.size(); ++i) {
        EXPECT_LE(after_stop_samples[i], after_stop_samples[1] + kTolerance)
            << "第 " << i << " 轮 stop 后 arena 占用 " << after_stop_samples[i] << "B 显著超过第 1 轮稳态 "
            << after_stop_samples[1] << "B" << " → service.stop() 没有真正回收 shm_object";
    }

    // 还要验证一个绝对量：register 写入的总数据约 kObjPerRound*(192 + slab + blob) ≈
    // 32*(192+~96+512) ≈ 25KB。如果 stop 完全没回收，4 轮就泄漏 ~100KB。
    // 因此最后一轮的占用相对第 1 轮的增量必须远小于"4 轮的累加注册数据"。
    if (after_stop_samples.size() >= 2) {
        const std::size_t expected_per_round_registered =
            static_cast<std::size_t>(kObjPerRound) * (192U + 128U + kBlobBytes);
        const std::size_t leak_budget_total = expected_per_round_registered * (after_stop_samples.size() - 1U);
        const std::size_t actual_growth     = after_stop_samples.back() > after_stop_samples.front()
                                                  ? after_stop_samples.back() - after_stop_samples.front()
                                                  : 0U;
        EXPECT_LT(actual_growth, leak_budget_total / 2U)
            << "绝对增量 " << actual_growth << "B 已接近" << " 4 轮累计注册数据量 " << leak_budget_total
            << "B → 强烈怀疑泄漏";
    }
}

// 同进程辅助用例：一个 service 内部 register → unregister → 再 register 同 path 新对象，路径完全不崩。
// 版本的子进程动作的最小化复现：先把它跑通能更精确定位 fork 版用例的崩点是
// (a) 子进程 SHM/single-process 状态污染 还是 (b) 我们这套路径本身的 bug。
TEST_F(shm_engine_business_test, in_process_register_unregister_loop_smoke)
{
    auto svc = std::make_unique<svc_alpha>();
    ASSERT_TRUE(svc->start());

    auto a = _make_obj("/org/test/biz/loop/a", "loop_a", "L1");
    svc->register_object(a);
    a->m_iface.m_counter.set_value(11);

    svc->unregister_object("/org/test/biz/loop/a");
    auto b = _make_obj("/org/test/biz/loop/b", "loop_b", "L2");
    svc->register_object(b);
    b->m_iface.m_counter.set_value(22);

    auto& tbl = svc->get_object_table();
    EXPECT_TRUE(tbl.find<mc::engine::by_path>(mc::string("/org/test/biz/loop/a")).is_end());
    auto it_b = tbl.find<mc::engine::by_path>(mc::string("/org/test/biz/loop/b"));
    ASSERT_FALSE(it_b.is_end());
    auto* casted = dynamic_cast<const biz_object*>(&*it_b);
    ASSERT_NE(casted, nullptr);
    EXPECT_EQ(casted->m_iface.m_counter.value(), 22);

    svc->unregister_object("/org/test/biz/loop/b");
    EXPECT_TRUE(tbl.find<mc::engine::by_path>(mc::string("/org/test/biz/loop/b")).is_end());

    svc->stop();
}

// takeover 后业务读写继续可用：fork 子进程验证接管后能读到旧 property 并继续写入。
// 退出码：31=start失败 40=找不到旧对象 41=旧值不对 50=写后读不一致 70/71=service链路异常

TEST_F(shm_engine_business_test, takeover_can_continue_writing_old_object)
{
    auto parent_svc = std::make_unique<svc_alpha>();
    ASSERT_TRUE(parent_svc->start());

    auto pre = _make_obj("/org/test/biz/takeover_rw/pre", "pre_obj", "T001");
    parent_svc->register_object(pre);
    pre->m_iface.m_counter.set_value(1234);
    pre->m_iface.m_label.set_value("pre");

    pid_t child = ::fork();
    ASSERT_GE(child, 0);

    if (child == 0) {
        try {
            // 父 fork 后仍 alive，默认 attach 会被 liveness 拒。先 force-attach
            // 把 svc.pid 改成子进程 pid，service.start 内部即可走 "同 pid
            // 跳过 liveness" 路径，模拟 crash 后接管。
            {
                auto& rt    = mc::engine::shm_runtime_provider::instance();
                auto  arena = rt.user_arena();
                auto  smap  = _open_service_map_for_test_biz();
                (void)mc::engine::shm_service_attach(arena, smap, mc::string_view("org.openubmc.biz_test.alpha"),
                                                     static_cast<std::uint32_t>(::getpid()), /*force=*/true);
            }
            auto child_svc = std::make_unique<svc_alpha>();
            if (!child_svc->start()) {
                std::_Exit(31);
            }

            auto& tbl    = child_svc->get_object_table();
            auto  pre_it = tbl.find<mc::engine::by_path>(mc::string("/org/test/biz/takeover_rw/pre"));
            if (pre_it.is_end()) {
                std::_Exit(40);
            }
            auto* pre_casted = dynamic_cast<const biz_object*>(&*pre_it);
            if (pre_casted == nullptr || pre_casted->m_iface.m_counter.value() != 1234) {
                std::_Exit(41);
            }

            // 写路径：在被接管的对象上修改 property → 再读必须一致
            const_cast<biz_object*>(pre_casted)->m_iface.m_counter.set_value(9999);
            const_cast<biz_object*>(pre_casted)->m_iface.m_label.set_value("post-takeover");
            if (pre_casted->m_iface.m_counter.value() != 9999) {
                std::_Exit(50);
            }
            if (pre_casted->m_iface.m_label.value() != "post-takeover") {
                std::_Exit(51);
            }

            // 被接管对象的 shm_object.service 应被刷成本进程 attach 的 shm_service POD
            auto* shm_svc = _lookup_shm_service("org.openubmc.biz_test.alpha");
            if (shm_svc == nullptr) {
                std::_Exit(70);
            }
            if (mc::engine::shm_object_service(*pre_casted->get_shm_handle()) != shm_svc) {
                std::_Exit(71);
            }

            std::_Exit(0);
        } catch (...) {
            std::_Exit(99);
        }
    }

    int status = -1;
    ASSERT_EQ(::waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status)) << "子进程异常退出 raw=" << status;
    EXPECT_EQ(WEXITSTATUS(status), 0) << "子进程退出码：31=start 失败，40/41=recover 后查不到/property 不对，"
                                         "50/51=takeover 后写 property 不一致，70/71=§0 service 链路失效，99=异常";

    parent_svc->stop();
}

} // namespace shm_engine_business_test
