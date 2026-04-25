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
#include <vector>

#include <mc/db/shm_storage_engine_index_only.h>
#include <mc/shm/allocator.h>
#include <mc/shm/shm_runtime.h>
#include <test_utilities/base.h>

namespace test_shm_engine_index_only {

struct fake_shm_record {
    std::uint64_t object_id;
};

class index_only_fixture : public mc::test::TestBase {
protected:
    using engine_t = mc::db::shm_storage_engine_index_only<fake_shm_record, 2U>;

    void SetUp() override
    {
        std::random_device rd;
        std::mt19937       rng(rd());
        char               nm[128];
        std::snprintf(nm, sizeof(nm), "mc_shm_idx_only_%d_%lu", ::getpid(), static_cast<unsigned long>(rng()));

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
        if (m_runtime) {
            auto arena = m_runtime->user_arena();
            for (auto* sh : m_records) {
                if (sh != nullptr) {
                    arena.deallocate(sh);
                }
            }
        }
        m_records.clear();
        m_runtime.reset();
    }

    fake_shm_record* make_record(std::uint64_t id)
    {
        auto  arena = m_runtime->user_arena();
        void* mem   = arena.allocate(sizeof(fake_shm_record), alignof(fake_shm_record));
        EXPECT_NE(mem, nullptr);
        auto* sh = new (mem) fake_shm_record{id};
        m_records.push_back(sh);
        return sh;
    }

    std::unique_ptr<mc::shm::shm_runtime> m_runtime;
    mc::db::shm_engine_alloc              m_alloc;
    std::vector<fake_shm_record*>         m_records;
};

TEST_F(index_only_fixture, default_construct_is_unbound_and_throws_on_use)
{
    engine_t e{};
    EXPECT_THROW(e.empty(0U), mc::invalid_arg_exception);
}

TEST_F(index_only_fixture, put_and_find_returns_pointer)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    auto*    sh          = make_record(42U);
    auto [old, replaced] = e.put(0U, std::string_view("k:1"), sh);
    EXPECT_EQ(nullptr, old);
    EXPECT_FALSE(replaced);

    auto* got = e.find(0U, std::string_view("k:1"));
    ASSERT_NE(nullptr, got);
    EXPECT_EQ(42U, got->object_id);

    EXPECT_EQ(nullptr, e.find(0U, std::string_view("k:404")));
    EXPECT_EQ(1U, e.size(0U));
}

TEST_F(index_only_fixture, put_overwrite_returns_old_pointer)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    auto*    a = make_record(1U);
    auto*    b = make_record(2U);

    e.put(0U, std::string_view("k"), a);
    auto [old, replaced] = e.put(0U, std::string_view("k"), b);
    EXPECT_EQ(a, old);
    EXPECT_TRUE(replaced);

    auto* got = e.find(0U, std::string_view("k"));
    ASSERT_NE(nullptr, got);
    EXPECT_EQ(2U, got->object_id);
    EXPECT_EQ(1U, e.size(0U));
}

TEST_F(index_only_fixture, erase_removes_entry_and_returns_pointer)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    auto*    sh = make_record(7U);
    e.put(0U, std::string_view("k:7"), sh);

    auto* removed = e.erase(0U, std::string_view("k:7"));
    EXPECT_EQ(sh, removed);
    EXPECT_TRUE(e.empty(0U));
    EXPECT_EQ(nullptr, e.erase(0U, std::string_view("k:7")));
}

TEST_F(index_only_fixture, indices_are_isolated)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    e.put(0U, std::string_view("a"), make_record(1U));
    e.put(1U, std::string_view("b"), make_record(2U));

    EXPECT_EQ(1U, e.size(0U));
    EXPECT_EQ(1U, e.size(1U));
    EXPECT_NE(nullptr, e.find(0U, std::string_view("a")));
    EXPECT_EQ(nullptr, e.find(0U, std::string_view("b")));
    EXPECT_NE(nullptr, e.find(1U, std::string_view("b")));
    EXPECT_THROW(e.size(2U), mc::invalid_arg_exception);
}

TEST_F(index_only_fixture, clear_empties_all_indices)
{
    engine_t e{m_alloc, mc::string_view("tbl")};
    e.put(0U, std::string_view("a"), make_record(1U));
    e.put(1U, std::string_view("b"), make_record(2U));

    e.clear();
    EXPECT_TRUE(e.empty(0U));
    EXPECT_TRUE(e.empty(1U));
}

} // namespace test_shm_engine_index_only
