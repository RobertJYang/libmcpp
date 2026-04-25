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

#include <cstdio>
#include <random>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include <mc/db/shm_storage_engine.h>
#include <mc/object_base.h>
#include <mc/shm/allocator.h>
#include <mc/shm/shm_runtime.h>
#include <test_utilities/base.h>

namespace test_shm_storage_engine {

// 简单 SHM POD：仅一个 object_id 字段，模拟 mcengine::shm_object 角色
struct fake_shm_record {
    std::uint64_t object_id;
};

// 持有 ShmRecord* 的测试 Object：mcengine 真实场景里 abstract_object 会持
// shm_object*；本测试用一个轻量 m_handle 字段模拟该锚点。
struct user : mc::object_base {
    user() = default;
    user(mc::object_id_type id, std::string name) : m_name(std::move(name))
    {
        set_object_id(id);
    }
    void set_handle(fake_shm_record* sh) noexcept
    {
        m_handle = sh;
    }
    fake_shm_record* handle() const noexcept
    {
        return m_handle;
    }

    std::string      m_name;
    fake_shm_record* m_handle = nullptr;
};

class shm_engine_fixture : public mc::test::TestBase {
protected:
    using engine_t = mc::db::shm_storage_engine<user, fake_shm_record, 2U>;

    void SetUp() override
    {
        std::random_device rd;
        std::mt19937       rng(rd());
        char               nm[128];
        std::snprintf(nm, sizeof(nm), "mc_shm_engine_%d_%lu", ::getpid(), static_cast<unsigned long>(rng()));

        mc::shm::runtime_options opts;
        opts.region_name     = mc::string(nm);
        opts.region_size     = 4 * 1024 * 1024;
        opts.root_capacity   = 32;
        opts.manager_process = true;
        m_runtime            = std::make_unique<mc::shm::shm_runtime>(opts);
        ASSERT_TRUE(m_runtime->is_valid());

        m_alloc = mc::db::shm_engine_alloc{*m_runtime, std::string(nm) + "_table"};
    }

    void TearDown() override
    {
        auto arena = _shm_arena();
        for (auto* sh : m_records) {
            if (sh != nullptr) {
                arena.deallocate(sh);
            }
        }
        m_records.clear();
        m_runtime.reset();
    }

    // 在 SHM 中分配一个 fake_shm_record，记录 object_id；测试结束统一回收。
    fake_shm_record* make_record(std::uint64_t id)
    {
        auto  arena = _shm_arena();
        void* mem   = arena.allocate(sizeof(fake_shm_record), alignof(fake_shm_record));
        EXPECT_NE(mem, nullptr);
        auto* sh = new (mem) fake_shm_record{id};
        m_records.push_back(sh);
        return sh;
    }

    // 创建带 ShmRecord* 锚点的 user
    mc::shared_ptr<user> make_user(std::uint64_t id, const std::string& name)
    {
        auto u  = mc::make_shared<user>(id, name);
        auto sh = make_record(id);
        u->set_handle(sh);
        return u;
    }

    void install_default_extractor(engine_t& e)
    {
        e.set_shm_handle_extractor([](const user& u) -> fake_shm_record* {
            return u.handle();
        });
    }

    mc::shm::shm_allocator _shm_arena() const
    {
        return m_runtime->user_arena();
    }

    std::unique_ptr<mc::shm::shm_runtime> m_runtime;
    mc::db::shm_engine_alloc              m_alloc;
    std::vector<fake_shm_record*>         m_records;
};

TEST_F(shm_engine_fixture, derive_per_index_alloc_appends_idx_suffix)
{
    auto a0 = mc::db::derive_per_index_alloc(m_alloc, 0U, mc::string_view("tbl"));
    auto a1 = mc::db::derive_per_index_alloc(m_alloc, 1U, mc::string_view("tbl"));
    EXPECT_NE(a0.name, a1.name);
    EXPECT_EQ(a0.runtime, m_alloc.runtime);
    EXPECT_NE(std::string::npos, a0.name.find(".idx0"));
    EXPECT_NE(std::string::npos, a1.name.find(".idx1"));
}

TEST_F(shm_engine_fixture, default_construct_is_unbound_and_throws_on_use)
{
    engine_t e{};
    EXPECT_THROW(e.empty(0U), mc::invalid_arg_exception);
}

TEST_F(shm_engine_fixture, insert_without_handle_extractor_throws)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    auto     u = make_user(1U, "alice");
    EXPECT_THROW(e.insert(0U, std::string_view("k:1"), u), mc::invalid_arg_exception);
}

TEST_F(shm_engine_fixture, insert_and_get_single_index)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    install_default_extractor(e);
    auto u = make_user(1U, "alice");

    auto [old, updated] = e.insert(0U, std::string_view("k:1"), u);
    EXPECT_FALSE(updated);
    EXPECT_FALSE(old.has_value());
    EXPECT_EQ(1U, e.size(0U));

    auto leaf = e.get(0U, std::string_view("k:1"));
    ASSERT_TRUE(leaf.has_value());
    ASSERT_TRUE(*leaf);
    EXPECT_EQ(1U, (*leaf)->get_object_id());

    auto miss = e.get(0U, std::string_view("k:404"));
    EXPECT_FALSE(miss.has_value());
}

TEST_F(shm_engine_fixture, insert_upsert_returns_old_leaf)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    install_default_extractor(e);
    auto u1 = make_user(1U, "alice");
    auto u2 = make_user(2U, "bob");

    e.insert(0U, std::string_view("k"), u1);
    auto [old, updated] = e.insert(0U, std::string_view("k"), u2);
    EXPECT_TRUE(updated);
    ASSERT_TRUE(old.has_value());
    ASSERT_TRUE(*old);
    EXPECT_EQ(1U, (*old)->get_object_id());

    auto leaf = e.get(0U, std::string_view("k"));
    ASSERT_TRUE(leaf.has_value());
    EXPECT_EQ(2U, (*leaf)->get_object_id());
    EXPECT_EQ(1U, e.size(0U));
}

TEST_F(shm_engine_fixture, remove_returns_old_leaf_and_misses_after)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    install_default_extractor(e);
    auto u = make_user(7U, "carol");
    e.insert(0U, std::string_view("k:7"), u);

    auto removed = e.remove(0U, std::string_view("k:7"));
    ASSERT_TRUE(removed.has_value());
    ASSERT_TRUE(*removed);
    EXPECT_EQ(7U, (*removed)->get_object_id());
    EXPECT_TRUE(e.empty(0U));

    auto miss = e.remove(0U, std::string_view("k:7"));
    EXPECT_FALSE(miss.has_value());
}

TEST_F(shm_engine_fixture, indices_are_isolated)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    install_default_extractor(e);
    e.insert(0U, std::string_view("a"), make_user(1U, "n1"));
    e.insert(1U, std::string_view("b"), make_user(2U, "n2"));

    EXPECT_EQ(1U, e.size(0U));
    EXPECT_EQ(1U, e.size(1U));
    EXPECT_TRUE(e.get(0U, std::string_view("a")).has_value());
    EXPECT_FALSE(e.get(0U, std::string_view("b")).has_value());
    EXPECT_THROW(e.size(2U), mc::invalid_arg_exception);
}

TEST_F(shm_engine_fixture, iteration_in_byte_order)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    install_default_extractor(e);
    e.insert(0U, std::string_view("z"), make_user(26U, "z"));
    e.insert(0U, std::string_view("a"), make_user(1U, "a"));
    e.insert(0U, std::string_view("m"), make_user(13U, "m"));

    std::vector<std::string> seen;
    for (auto it = e.begin(0U); it != e.end(0U); ++it) {
        seen.emplace_back(it.key());
    }
    ASSERT_EQ(3U, seen.size());
    EXPECT_EQ("a", seen[0]);
    EXPECT_EQ("m", seen[1]);
    EXPECT_EQ("z", seen[2]);
}

TEST_F(shm_engine_fixture, lower_upper_bound_and_proxy_access)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    install_default_extractor(e);
    e.insert(0U, std::string_view("a"), make_user(1U, "a"));
    e.insert(0U, std::string_view("m"), make_user(13U, "m"));
    e.insert(0U, std::string_view("z"), make_user(26U, "z"));

    auto lb = e.lower_bound(0U, std::string_view("k"));
    ASSERT_NE(lb, e.end(0U));
    EXPECT_EQ("m", lb.key());
    EXPECT_TRUE(lb->second);
    EXPECT_EQ(13U, lb->second->get_object_id());

    auto ub = e.upper_bound(0U, std::string_view("m"));
    ASSERT_NE(ub, e.end(0U));
    EXPECT_EQ("z", ub.key());

    auto miss = e.find(0U, std::string_view("not_present"));
    EXPECT_TRUE(miss.is_end());
}

TEST_F(shm_engine_fixture, clear_empties_all_indices_and_pool)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    install_default_extractor(e);
    e.insert(0U, std::string_view("a"), make_user(1U, "a"));
    e.insert(1U, std::string_view("b"), make_user(2U, "b"));
    EXPECT_FALSE(e.empty(0U));
    EXPECT_FALSE(e.empty(1U));

    e.clear();
    EXPECT_TRUE(e.empty(0U));
    EXPECT_TRUE(e.empty(1U));
    EXPECT_FALSE(e.get(0U, std::string_view("a")).has_value());
}

TEST_F(shm_engine_fixture, recover_uses_reconstruct_callback_with_shm_record)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    install_default_extractor(e);
    auto u = make_user(42U, "answer");
    e.insert(0U, std::string_view("k:42"), u);

    // 模拟"重启"：把 heap pool 清掉但保留 shm 索引（这里用 recover 流程模拟）
    e.set_reconstruct([](fake_shm_record* sh) {
        auto rebuilt = mc::make_shared<user>(sh->object_id, "rebuilt");
        rebuilt->set_handle(sh);
        return rebuilt;
    });
    e.recover();

    auto leaf = e.get(0U, std::string_view("k:42"));
    ASSERT_TRUE(leaf.has_value());
    ASSERT_TRUE(*leaf);
    EXPECT_EQ(42U, (*leaf)->get_object_id());
    EXPECT_EQ("rebuilt", (*leaf)->m_name);
}

TEST_F(shm_engine_fixture, transaction_apis_are_noop_safe)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    EXPECT_NO_THROW(e.commit());
    EXPECT_NO_THROW(e.rollback());
    EXPECT_NO_THROW(e.save_point());
    EXPECT_NO_THROW(e.rollback_to(0));
    EXPECT_NO_THROW(e.lock_db());
    EXPECT_NO_THROW(e.unlock_db());
    EXPECT_EQ(-1, e.current_save_point());
}

TEST_F(shm_engine_fixture, raw_iterator_to_next_prefix_skips_prefix_block)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    install_default_extractor(e);
    e.insert(0U, std::string_view("p:1"), make_user(1U, "x"));
    e.insert(0U, std::string_view("p:2"), make_user(2U, "y"));
    e.insert(0U, std::string_view("q:0"), make_user(3U, "z"));

    auto it = e.lower_bound(0U, std::string_view("p:"));
    ASSERT_NE(it, e.end(0U));
    EXPECT_EQ("p:1", it.key());

    it.to_next_prefix(std::string_view("p:"));
    ASSERT_NE(it, e.end(0U));
    EXPECT_EQ("q:0", it.key());
}

} // namespace test_shm_storage_engine
