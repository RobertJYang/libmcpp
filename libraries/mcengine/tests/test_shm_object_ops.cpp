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

// B2.2.b.2/3：shm_object_ops 单元测试
//
// 覆盖：
//   - shm_byte_string_create/destroy
//   - property_slab_create/destroy/find/set_int64/set_double/set_blob/remove
//   - property_slab copy-on-grow（offset_ptr 重映射正确性）
//   - child_slab_create/destroy/find/add/remove + 幂等性
//   - shm_object_create/destroy（identity 字段完整性 + 全字段 CRC）
//   - shm_object set/get property/child 高层 API
//   - reader：identity 与 property 的回读

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

#include "shm_object_ops.h"
#include <mc/shm/allocator.h>
#include <mc/shm/region.h>

using mc::engine::child_slab;
using mc::engine::child_slab_add;
using mc::engine::child_slab_create;
using mc::engine::child_slab_destroy;
using mc::engine::child_slab_find;
using mc::engine::child_slab_remove;
using mc::engine::shm_object;
using mc::engine::shm_object_add_child;
using mc::engine::shm_object_check;
using mc::engine::shm_object_child_count;
using mc::engine::shm_object_class_name;
using mc::engine::shm_object_create;
using mc::engine::shm_object_destroy;
using mc::engine::shm_object_get_property_blob;
using mc::engine::shm_object_get_property_double;
using mc::engine::shm_object_get_property_int64;
using mc::engine::shm_object_name;
using mc::engine::shm_object_path;
using mc::engine::shm_object_position;
using mc::engine::shm_object_property_count;
using mc::engine::shm_object_remove_child;
using mc::engine::shm_object_remove_property;
using mc::engine::shm_object_set_class_name;
using mc::engine::shm_object_parent;
using mc::engine::shm_object_service;
using mc::engine::shm_object_set_parent;
using mc::engine::shm_object_set_service;
using mc::engine::shm_object_set_property_blob;
using mc::engine::shm_object_set_property_double;
using mc::engine::shm_object_set_property_int64;
using mc::engine::property_slab;
using mc::engine::property_slab_check;
using mc::engine::property_slab_create;
using mc::engine::property_slab_destroy;
using mc::engine::property_slab_find;
using mc::engine::property_slab_remove;
using mc::engine::property_slab_set_blob;
using mc::engine::property_slab_set_double;
using mc::engine::property_slab_set_int64;
using mc::engine::property_type_tag;
using mc::engine::shm_byte_string_create;
using mc::engine::shm_byte_string_destroy;
using mc::engine::slab_grow_capacity;
using mc::engine::shm_allocator;
using mc::shm::shm_region;
using mc::shm::shm_region_options;

namespace {

class shadow_ops_fixture : public ::testing::Test {
protected:
    static constexpr std::size_t region_size = 4 * 1024 * 1024;

    void SetUp() override
    {
        std::random_device rd;
        std::mt19937       rng(rd());
        char               buf[128];
        std::snprintf(buf, sizeof(buf), "mc_shadow_ops_test_%d_%u", ::getpid(), rng());

        shm_region_options opts;
        opts.segment_name  = mc::string(buf);
        opts.total_size    = region_size;
        opts.root_capacity = 16;

        m_region = shm_region(opts);
        ASSERT_TRUE(m_region.is_valid());
        m_alloc = m_region.user_arena();
        ASSERT_TRUE(m_alloc.is_valid());
    }

    shm_region    m_region;
    shm_allocator m_alloc;
};

}  // namespace

// ============================================================================
// slab_grow_capacity
// ============================================================================

TEST(slab_grow_capacity_test, doubles_until_meets_needed)
{
    EXPECT_EQ(slab_grow_capacity(0, 1), 4);
    EXPECT_EQ(slab_grow_capacity(0, 4), 4);
    EXPECT_EQ(slab_grow_capacity(0, 5), 8);
    EXPECT_EQ(slab_grow_capacity(4, 5), 8);
    EXPECT_EQ(slab_grow_capacity(4, 9), 16);
    EXPECT_EQ(slab_grow_capacity(16, 100), 128);
}

TEST(slab_grow_capacity_test, large_values_clamp_to_needed)
{
    EXPECT_EQ(slab_grow_capacity(0xC000, 0xD000), 0xD000);
}

// ============================================================================
// shm_byte_string_create / destroy
// ============================================================================

TEST_F(shadow_ops_fixture, byte_string_create_round_trip)
{
    auto* bs = shm_byte_string_create(m_alloc, "hello");
    ASSERT_NE(bs, nullptr);
    EXPECT_EQ(bs->as_string_view(), "hello");
    EXPECT_TRUE(bs->buffer_intact());
    shm_byte_string_destroy(m_alloc, bs);
}

TEST_F(shadow_ops_fixture, byte_string_create_empty_is_valid)
{
    auto* bs = shm_byte_string_create(m_alloc, "");
    ASSERT_NE(bs, nullptr);
    EXPECT_TRUE(bs->empty());
    shm_byte_string_destroy(m_alloc, bs);
}

TEST_F(shadow_ops_fixture, byte_string_destroy_null_is_noop)
{
    shm_byte_string_destroy(m_alloc, nullptr);
}

// ============================================================================
// property_slab basic ops
// ============================================================================

TEST_F(shadow_ops_fixture, property_slab_create_default_state)
{
    auto* slab = property_slab_create(m_alloc, 4);
    ASSERT_NE(slab, nullptr);
    EXPECT_EQ(slab->slot_capacity, 4);
    EXPECT_EQ(slab->slot_count, 0);
    EXPECT_TRUE(property_slab_check(*slab));
    property_slab_destroy(m_alloc, slab);
}

TEST_F(shadow_ops_fixture, property_slab_set_int64_then_find)
{
    auto* slab = property_slab_create(m_alloc, 4);
    ASSERT_TRUE(property_slab_set_int64(m_alloc, slab, "a", 42));
    ASSERT_TRUE(property_slab_set_int64(m_alloc, slab, "b", -7));
    EXPECT_EQ(slab->slot_count, 2);
    EXPECT_TRUE(property_slab_check(*slab));

    const int idx_a = property_slab_find(*slab, "a");
    const int idx_b = property_slab_find(*slab, "b");
    ASSERT_GE(idx_a, 0);
    ASSERT_GE(idx_b, 0);
    EXPECT_EQ(slab->slots[idx_a].type, property_type_tag::int64);
    EXPECT_EQ(slab->slots[idx_a].v_int64, 42);
    EXPECT_EQ(slab->slots[idx_b].v_int64, -7);
    EXPECT_EQ(property_slab_find(*slab, "missing"), -1);

    property_slab_destroy(m_alloc, slab);
}

TEST_F(shadow_ops_fixture, property_slab_overwrite_keeps_count)
{
    auto* slab = property_slab_create(m_alloc, 4);
    ASSERT_TRUE(property_slab_set_int64(m_alloc, slab, "k", 1));
    ASSERT_TRUE(property_slab_set_int64(m_alloc, slab, "k", 999));
    EXPECT_EQ(slab->slot_count, 1);
    const int idx = property_slab_find(*slab, "k");
    ASSERT_GE(idx, 0);
    EXPECT_EQ(slab->slots[idx].v_int64, 999);
    EXPECT_TRUE(property_slab_check(*slab));
    property_slab_destroy(m_alloc, slab);
}

TEST_F(shadow_ops_fixture, property_slab_set_double_blob_mix)
{
    auto* slab = property_slab_create(m_alloc, 4);
    ASSERT_TRUE(property_slab_set_double(m_alloc, slab, "pi", 3.1415));
    ASSERT_TRUE(property_slab_set_blob(m_alloc, slab, "name", "alice", property_type_tag::string));
    ASSERT_TRUE(property_slab_set_blob(m_alloc, slab, "blob", "\x00\x01\x02", property_type_tag::bytes));

    const int idx_pi = property_slab_find(*slab, "pi");
    ASSERT_GE(idx_pi, 0);
    EXPECT_EQ(slab->slots[idx_pi].type, property_type_tag::double_);
    EXPECT_DOUBLE_EQ(slab->slots[idx_pi].v_double, 3.1415);

    const int idx_name = property_slab_find(*slab, "name");
    ASSERT_GE(idx_name, 0);
    EXPECT_EQ(slab->slots[idx_name].type, property_type_tag::string);
    auto* name_bs = slab->slots[idx_name].v_blob.get();
    ASSERT_NE(name_bs, nullptr);
    EXPECT_EQ(name_bs->as_string_view(), "alice");

    EXPECT_TRUE(property_slab_check(*slab));
    property_slab_destroy(m_alloc, slab);
}

TEST_F(shadow_ops_fixture, property_slab_overwrite_blob_with_int_releases_old_blob)
{
    auto* slab = property_slab_create(m_alloc, 4);
    ASSERT_TRUE(property_slab_set_blob(m_alloc, slab, "k", "longvalue", property_type_tag::string));
    const std::size_t after_blob = m_alloc.allocated_size();
    ASSERT_TRUE(property_slab_set_int64(m_alloc, slab, "k", 7));
    EXPECT_LT(m_alloc.allocated_size(), after_blob);  // 旧 blob 已释放
    property_slab_destroy(m_alloc, slab);
}

TEST_F(shadow_ops_fixture, property_slab_remove_swap_pop)
{
    auto* slab = property_slab_create(m_alloc, 4);
    ASSERT_TRUE(property_slab_set_int64(m_alloc, slab, "a", 1));
    ASSERT_TRUE(property_slab_set_int64(m_alloc, slab, "b", 2));
    ASSERT_TRUE(property_slab_set_int64(m_alloc, slab, "c", 3));

    EXPECT_TRUE(property_slab_remove(m_alloc, *slab, "a"));
    EXPECT_EQ(slab->slot_count, 2);
    EXPECT_EQ(property_slab_find(*slab, "a"), -1);
    EXPECT_GE(property_slab_find(*slab, "b"), 0);
    EXPECT_GE(property_slab_find(*slab, "c"), 0);
    EXPECT_TRUE(property_slab_check(*slab));

    EXPECT_FALSE(property_slab_remove(m_alloc, *slab, "missing"));
    property_slab_destroy(m_alloc, slab);
}

// ============================================================================
// property_slab copy-on-grow（offset_ptr 重新基址正确性）
// ============================================================================

TEST_F(shadow_ops_fixture, property_slab_grows_on_capacity_exhausted)
{
    auto* slab = property_slab_create(m_alloc, 4);
    auto* original = slab;

    for (int i = 0; i < 20; ++i) {
        const std::string key = "k" + std::to_string(i);
        ASSERT_TRUE(property_slab_set_int64(m_alloc, slab, key, i * 1000));
    }

    EXPECT_NE(slab, original);
    EXPECT_GE(slab->slot_capacity, 20);
    EXPECT_EQ(slab->slot_count, 20);
    EXPECT_TRUE(property_slab_check(*slab));

    for (int i = 0; i < 20; ++i) {
        const std::string key = "k" + std::to_string(i);
        const int         idx = property_slab_find(*slab, key);
        ASSERT_GE(idx, 0) << "key=" << key;
        EXPECT_EQ(slab->slots[idx].v_int64, i * 1000);
    }

    property_slab_destroy(m_alloc, slab);
}

TEST_F(shadow_ops_fixture, property_slab_grow_preserves_blob_offset_ptr)
{
    auto* slab = property_slab_create(m_alloc, 4);
    for (int i = 0; i < 4; ++i) {
        const std::string key = "k" + std::to_string(i);
        const std::string val = "blob_" + std::to_string(i) + std::string(20, 'x');
        ASSERT_TRUE(property_slab_set_blob(m_alloc, slab, key, val, property_type_tag::string));
    }
    // 触发扩容
    ASSERT_TRUE(property_slab_set_blob(m_alloc, slab, "k4", "trigger", property_type_tag::string));
    EXPECT_GE(slab->slot_capacity, 5);

    for (int i = 0; i < 4; ++i) {
        const std::string key = "k" + std::to_string(i);
        const int         idx = property_slab_find(*slab, key);
        ASSERT_GE(idx, 0);
        auto* bs = slab->slots[idx].v_blob.get();
        ASSERT_NE(bs, nullptr);
        EXPECT_EQ(bs->as_string_view(), "blob_" + std::to_string(i) + std::string(20, 'x'));
    }
    property_slab_destroy(m_alloc, slab);
}

// ============================================================================
// child_slab ops
// ============================================================================

TEST_F(shadow_ops_fixture, child_slab_add_find_remove)
{
    auto* a = shm_object_create(m_alloc, 1, "C", "a", "/a", "");
    auto* b = shm_object_create(m_alloc, 2, "C", "b", "/b", "");
    auto* c = shm_object_create(m_alloc, 3, "C", "c", "/c", "");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    child_slab* slab = nullptr;
    ASSERT_TRUE(child_slab_add(m_alloc, slab, a));
    ASSERT_TRUE(child_slab_add(m_alloc, slab, b));
    ASSERT_TRUE(child_slab_add(m_alloc, slab, c));
    EXPECT_EQ(slab->slot_count, 3);
    EXPECT_GE(child_slab_find(*slab, b), 0);

    // 幂等
    ASSERT_TRUE(child_slab_add(m_alloc, slab, b));
    EXPECT_EQ(slab->slot_count, 3);

    EXPECT_TRUE(child_slab_remove(*slab, b));
    EXPECT_EQ(slab->slot_count, 2);
    EXPECT_EQ(child_slab_find(*slab, b), -1);
    EXPECT_FALSE(child_slab_remove(*slab, b));

    child_slab_destroy(m_alloc, slab);
    shm_object_destroy(m_alloc, a);
    shm_object_destroy(m_alloc, b);
    shm_object_destroy(m_alloc, c);
}

TEST_F(shadow_ops_fixture, child_slab_grows_and_preserves_offsets)
{
    std::vector<shm_object*> kids;
    for (int i = 0; i < 20; ++i) {
        kids.push_back(shm_object_create(m_alloc, static_cast<std::uint64_t>(i), "C",
                                            "n" + std::to_string(i), "/p" + std::to_string(i), ""));
        ASSERT_NE(kids.back(), nullptr);
    }
    child_slab* slab = nullptr;
    for (auto* k : kids) {
        ASSERT_TRUE(child_slab_add(m_alloc, slab, k));
    }
    EXPECT_EQ(slab->slot_count, 20);
    EXPECT_GE(slab->slot_capacity, 20);

    for (auto* k : kids) {
        EXPECT_GE(child_slab_find(*slab, k), 0);
    }

    child_slab_destroy(m_alloc, slab);
    for (auto* k : kids) {
        shm_object_destroy(m_alloc, k);
    }
}

// ============================================================================
// shm_object_create / destroy（identity + CRC）
// ============================================================================

TEST_F(shadow_ops_fixture, shm_object_create_default_state)
{
    auto* sh = shm_object_create(m_alloc, 42, "mc.bmc.MemberInfo", "info0",
                                    "/redfish/v1/MemberInfo/0", "0");
    ASSERT_NE(sh, nullptr);
    EXPECT_EQ(sh->object_id, 42U);
    EXPECT_EQ(shm_object_class_name(*sh), "mc.bmc.MemberInfo");
    EXPECT_EQ(shm_object_name(*sh), "info0");
    EXPECT_EQ(shm_object_path(*sh), "/redfish/v1/MemberInfo/0");
    EXPECT_EQ(shm_object_position(*sh), "0");
    EXPECT_EQ(sh->service.get(), nullptr);
    EXPECT_EQ(sh->parent.get(), nullptr);
    EXPECT_EQ(sh->properties.get(), nullptr);
    EXPECT_EQ(sh->children.get(), nullptr);
    EXPECT_TRUE(shm_object_check(*sh));
    shm_object_destroy(m_alloc, sh);
}

TEST_F(shadow_ops_fixture, shm_object_destroy_releases_all)
{
    const std::size_t before = m_alloc.allocated_size();
    auto*             sh     = shm_object_create(m_alloc, 1, "C", "n", "/p", "pos");
    ASSERT_NE(sh, nullptr);
    ASSERT_TRUE(shm_object_set_property_int64(m_alloc, *sh, "k", 1));
    ASSERT_TRUE(shm_object_set_property_blob(m_alloc, *sh, "blob", "longstring", property_type_tag::string));

    auto* child = shm_object_create(m_alloc, 2, "C", "c", "/p/c", "");
    ASSERT_NE(child, nullptr);
    ASSERT_TRUE(shm_object_add_child(m_alloc, *sh, child));

    shm_object_destroy(m_alloc, sh);
    shm_object_destroy(m_alloc, child);
    EXPECT_EQ(m_alloc.allocated_size(), before);
}

TEST_F(shadow_ops_fixture, shm_object_set_class_name_replaces_byte_string)
{
    auto* sh = shm_object_create(m_alloc, 1, "old.cls", "n", "/p", "");
    ASSERT_NE(sh, nullptr);
    ASSERT_TRUE(shm_object_set_class_name(m_alloc, *sh, "new.cls"));
    EXPECT_EQ(shm_object_class_name(*sh), "new.cls");
    EXPECT_TRUE(shm_object_check(*sh));
    shm_object_destroy(m_alloc, sh);
}

// ============================================================================
// shm_object 高层 property/child setter
// ============================================================================

TEST_F(shadow_ops_fixture, shm_object_set_property_lazy_alloc_slab)
{
    auto* sh = shm_object_create(m_alloc, 1, "C", "n", "/p", "");
    ASSERT_NE(sh, nullptr);
    EXPECT_EQ(sh->properties.get(), nullptr);

    ASSERT_TRUE(shm_object_set_property_int64(m_alloc, *sh, "k1", 100));
    ASSERT_NE(sh->properties.get(), nullptr);
    EXPECT_TRUE(shm_object_check(*sh));
    EXPECT_TRUE(property_slab_check(*sh->properties.get()));

    std::int64_t v = 0;
    EXPECT_TRUE(shm_object_get_property_int64(*sh, "k1", v));
    EXPECT_EQ(v, 100);

    EXPECT_FALSE(shm_object_get_property_int64(*sh, "missing", v));

    shm_object_destroy(m_alloc, sh);
}

TEST_F(shadow_ops_fixture, shm_object_get_property_blob_round_trip)
{
    auto* sh = shm_object_create(m_alloc, 1, "C", "n", "/p", "");
    ASSERT_TRUE(shm_object_set_property_blob(m_alloc, *sh, "name", "alice", property_type_tag::string));
    ASSERT_TRUE(shm_object_set_property_blob(m_alloc, *sh, "raw", std::string_view("\x00\x01\x02", 3),
                                                property_type_tag::bytes));

    std::string_view  view;
    property_type_tag tag;
    ASSERT_TRUE(shm_object_get_property_blob(*sh, "name", view, tag));
    EXPECT_EQ(view, "alice");
    EXPECT_EQ(tag, property_type_tag::string);

    ASSERT_TRUE(shm_object_get_property_blob(*sh, "raw", view, tag));
    EXPECT_EQ(view.size(), 3U);
    EXPECT_EQ(tag, property_type_tag::bytes);

    shm_object_destroy(m_alloc, sh);
}

TEST_F(shadow_ops_fixture, shm_object_set_property_double)
{
    auto*  sh = shm_object_create(m_alloc, 1, "C", "n", "/p", "");
    ASSERT_TRUE(shm_object_set_property_double(m_alloc, *sh, "pi", 3.14));
    double v = 0.0;
    EXPECT_TRUE(shm_object_get_property_double(*sh, "pi", v));
    EXPECT_DOUBLE_EQ(v, 3.14);
    shm_object_destroy(m_alloc, sh);
}

TEST_F(shadow_ops_fixture, shm_object_remove_property)
{
    auto* sh = shm_object_create(m_alloc, 1, "C", "n", "/p", "");
    ASSERT_TRUE(shm_object_set_property_int64(m_alloc, *sh, "k1", 1));
    ASSERT_TRUE(shm_object_set_property_int64(m_alloc, *sh, "k2", 2));
    EXPECT_EQ(shm_object_property_count(*sh), 2);
    EXPECT_TRUE(shm_object_remove_property(m_alloc, *sh, "k1"));
    EXPECT_EQ(shm_object_property_count(*sh), 1);
    std::int64_t v = 0;
    EXPECT_FALSE(shm_object_get_property_int64(*sh, "k1", v));
    EXPECT_TRUE(shm_object_get_property_int64(*sh, "k2", v));
    shm_object_destroy(m_alloc, sh);
}

TEST_F(shadow_ops_fixture, shm_object_grow_property_slab_keeps_crc)
{
    auto* sh = shm_object_create(m_alloc, 1, "C", "n", "/p", "");
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(shm_object_set_property_int64(m_alloc, *sh, "k" + std::to_string(i), i));
    }
    EXPECT_EQ(shm_object_property_count(*sh), 10);
    EXPECT_TRUE(shm_object_check(*sh));
    EXPECT_TRUE(property_slab_check(*sh->properties.get()));
    shm_object_destroy(m_alloc, sh);
}

TEST_F(shadow_ops_fixture, shm_object_add_remove_child_round_trip)
{
    auto* parent = shm_object_create(m_alloc, 1, "C", "p", "/p", "");
    auto* c1     = shm_object_create(m_alloc, 2, "C", "c1", "/c1", "");
    auto* c2     = shm_object_create(m_alloc, 3, "C", "c2", "/c2", "");

    ASSERT_TRUE(shm_object_add_child(m_alloc, *parent, c1));
    ASSERT_TRUE(shm_object_add_child(m_alloc, *parent, c2));
    EXPECT_EQ(shm_object_child_count(*parent), 2);
    EXPECT_TRUE(shm_object_check(*parent));

    EXPECT_TRUE(shm_object_remove_child(*parent, c1));
    EXPECT_EQ(shm_object_child_count(*parent), 1);
    EXPECT_FALSE(shm_object_remove_child(*parent, c1));

    shm_object_destroy(m_alloc, parent);
    shm_object_destroy(m_alloc, c1);
    shm_object_destroy(m_alloc, c2);
}

TEST_F(shadow_ops_fixture, shm_object_set_parent_updates_offset_and_crc)
{
    auto* parent = shm_object_create(m_alloc, 1, "C", "p", "/p", "");
    auto* child  = shm_object_create(m_alloc, 2, "C", "c", "/c", "");
    ASSERT_NE(parent, nullptr);
    ASSERT_NE(child, nullptr);

    shm_object_set_parent(*child, parent);
    EXPECT_EQ(child->parent.get(), parent);
    EXPECT_EQ(shm_object_parent(*child), parent);
    EXPECT_TRUE(shm_object_check(*child));

    shm_object_set_parent(*child, nullptr);
    EXPECT_EQ(child->parent.get(), nullptr);
    EXPECT_EQ(shm_object_parent(*child), nullptr);
    EXPECT_TRUE(shm_object_check(*child));

    shm_object_destroy(m_alloc, parent);
    shm_object_destroy(m_alloc, child);
}

TEST_F(shadow_ops_fixture, shm_object_set_service_updates_offset_and_crc)
{
    auto* sh = shm_object_create(m_alloc, 7, "C", "n", "/p", "");
    ASSERT_NE(sh, nullptr);
    EXPECT_EQ(shm_object_service(*sh), nullptr);

    // 用一个虚拟地址模拟 shm_service*（offset_ptr 是 self-relative，不需要真实对象）
    alignas(8) std::byte fake_service_storage[64]{};
    auto* fake_service = reinterpret_cast<mc::engine::shm_service*>(&fake_service_storage[0]);

    shm_object_set_service(*sh, fake_service);
    EXPECT_EQ(sh->service.get(), fake_service);
    EXPECT_EQ(shm_object_service(*sh), fake_service);
    EXPECT_TRUE(shm_object_check(*sh));

    shm_object_set_service(*sh, nullptr);
    EXPECT_EQ(sh->service.get(), nullptr);
    EXPECT_TRUE(shm_object_check(*sh));

    shm_object_destroy(m_alloc, sh);
}

// ============================================================================
// 综合：写入 + reopen 模拟（重新读取 region 内同一 shadow 指针）
// ============================================================================

TEST_F(shadow_ops_fixture, shm_object_full_field_round_trip)
{
    auto* sh = shm_object_create(m_alloc, 0xDEADBEEFULL, "mc.test.A", "obj0",
                                    "/svc/A/0", "0");
    ASSERT_TRUE(shm_object_set_property_int64(m_alloc, *sh, "n", 42));
    ASSERT_TRUE(shm_object_set_property_double(m_alloc, *sh, "pi", 3.14));
    ASSERT_TRUE(shm_object_set_property_blob(m_alloc, *sh, "name", "alice",
                                                property_type_tag::string));

    EXPECT_TRUE(shm_object_check(*sh));
    EXPECT_EQ(shm_object_property_count(*sh), 3);

    std::int64_t      i64 = 0;
    double            d   = 0.0;
    std::string_view  sv;
    property_type_tag tag;
    EXPECT_TRUE(shm_object_get_property_int64(*sh, "n", i64));
    EXPECT_TRUE(shm_object_get_property_double(*sh, "pi", d));
    EXPECT_TRUE(shm_object_get_property_blob(*sh, "name", sv, tag));
    EXPECT_EQ(i64, 42);
    EXPECT_DOUBLE_EQ(d, 3.14);
    EXPECT_EQ(sv, "alice");
    EXPECT_EQ(tag, property_type_tag::string);

    shm_object_destroy(m_alloc, sh);
}
