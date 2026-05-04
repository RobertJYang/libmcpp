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

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <unistd.h>
#include <vector>

#include <mc/db/shm_storage_engine.h>
#include <mc/db/table.h>
#include <mc/db/transaction.h>
#include <mc/shm/allocator.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/shm_runtime.h>

namespace test_table_shm_engine {

namespace mdb = mc::db;

// 与 test_shm_storage_engine 同样的 fake POD：仅持 object_id；
// 由测试夹具在 SHM arena 中分配/回收，模拟 mcengine::shm_object 角色。
struct fake_shm_record {
    std::uint64_t object_id;
};

class user : public mdb::object_base {
public:
    MC_REFLECTABLE("mc.test.shm_table.User")

    user() = default;
    user(mc::string name, int age) : m_name(std::move(name)), m_age(age)
    {}

    const mc::string& name() const
    {
        return m_name;
    }
    int get_age() const
    {
        return m_age;
    }

    void set_handle(fake_shm_record* sh) noexcept
    {
        m_handle = sh;
    }
    fake_shm_record* handle() const noexcept
    {
        return m_handle;
    }

    mc::string       m_name;
    int              m_age    = 0;
    fake_shm_record* m_handle = nullptr;
};

} // namespace test_table_shm_engine

MC_REFLECT(test_table_shm_engine::user, ((m_name, "name"))((m_age, "age")))

namespace test_table_shm_engine {

using indices_def = mdb::indexed_by<mdb::ordered_unique<&user::m_name>, mdb::ordered_non_unique<&user::get_age>>;

// IndexCount = 用户索引数(2) + object_id 默认索引(1) = 3
using shm_engine_t   = mdb::shm_storage_engine<user, fake_shm_record, 3U, mdb::shm_engine_alloc>;
using shm_user_table = mdb::table<user, indices_def, mdb::shm_engine_alloc, shm_engine_t>;

class table_shm_engine_fixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::random_device rd;
        std::mt19937       rng(rd());
        char               nm[128];
        std::snprintf(nm, sizeof(nm), "mc_table_shm_%d_%lu", ::getpid(), static_cast<unsigned long>(rng()));
        m_region_name = mc::string(nm);

        mc::shm::runtime_options opts;
        opts.region_name     = m_region_name;
        opts.region_size     = 4 * 1024 * 1024;
        opts.root_capacity   = 32;
        opts.manager_process = true;
        m_runtime            = std::make_unique<mc::shm::shm_runtime>(opts);
        ASSERT_TRUE(m_runtime->is_valid());

        m_alloc = mdb::shm_engine_alloc{*m_runtime, std::string(nm) + "_users"};
    }

    void TearDown() override
    {
        mc::singleton<mdb::transaction, mdb::default_transaction_tag>::reset_for_test();
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
        if (!m_region_name.empty()) {
            mc::shm::detail::shared_memory_backend::remove(mc::string_view(m_region_name));
        }
    }

    // 在 SHM 中分配 fake_shm_record 并填入 object_id，绑定到 user。
    // 必须在 table.add() 之前调用，使 extractor 能拿到非空 ShmRecord*。
    void bind_handle(user& u, std::uint64_t fallback_id)
    {
        auto  arena = m_runtime->user_arena();
        void* mem   = arena.allocate(sizeof(fake_shm_record), alignof(fake_shm_record));
        ASSERT_NE(mem, nullptr);
        std::uint64_t id = u.has_valid_id() ? u.get_object_id() : fallback_id;
        if (!u.has_valid_id()) {
            u.set_object_id(id);
        }
        auto* sh = new (mem) fake_shm_record{id};
        u.set_handle(sh);
        m_records.push_back(sh);
    }

    // 设置 table-engine 的 shm_handle_extractor：从 user 中读出已绑定的 ShmRecord*
    void install_extractor(shm_user_table& t)
    {
        t.engine().set_shm_handle_extractor([](const user& u) -> fake_shm_record* {
            return u.handle();
        });
    }

    // add 助手：先分配 ShmRecord 绑定到 obj，再调用 table.add。
    auto add_user(shm_user_table& t, user obj, mdb::transaction* txn = nullptr)
    {
        bind_handle(obj, _next_fake_id());
        return t.add(std::move(obj), txn);
    }

    std::uint64_t _next_fake_id() noexcept
    {
        return ++m_next_fake_id;
    }

    std::unique_ptr<mc::shm::shm_runtime> m_runtime;
    mdb::shm_engine_alloc                 m_alloc;
    std::vector<fake_shm_record*>         m_records;
    std::uint64_t                         m_next_fake_id = 100000U;
    mc::string                            m_region_name;
};

TEST_F(table_shm_engine_fixture, add_and_lookup_via_indices)
{
    shm_user_table users(mc::string_view("users"), 0, m_alloc);
    install_extractor(users);

    ASSERT_TRUE(add_user(users, user("alice", 30)));
    ASSERT_TRUE(add_user(users, user("bob", 25)));
    ASSERT_TRUE(add_user(users, user("carol", 30)));

    auto by_name = users.template get<1>().find("bob");
    ASSERT_FALSE(by_name.is_end());
    EXPECT_EQ(25, by_name->get_age());

    auto missing = users.template get<1>().find("zzz");
    EXPECT_TRUE(missing.is_end());
}

TEST_F(table_shm_engine_fixture, remove_object_drops_from_all_indices)
{
    shm_user_table users(mc::string_view("users"), 0, m_alloc);
    install_extractor(users);

    auto p_alice = add_user(users, user("alice", 30));
    ASSERT_TRUE(p_alice);
    ASSERT_TRUE(add_user(users, user("bob", 25)));

    EXPECT_TRUE(users.remove(p_alice));
    auto by_name = users.template get<1>().find("alice");
    EXPECT_TRUE(by_name.is_end());

    auto by_id = users.template get<0>().find(p_alice->get_object_id());
    EXPECT_TRUE(by_id.is_end());
}

TEST_F(table_shm_engine_fixture, transaction_commit_persists_changes)
{
    shm_user_table users(mc::string_view("users"), 0, m_alloc);
    install_extractor(users);

    auto& txn = mdb::transaction::get_instance();
    ASSERT_TRUE(add_user(users, user("alice", 30), &txn));
    txn.commit();

    auto by_name = users.template get<1>().find("alice");
    ASSERT_FALSE(by_name.is_end());
    EXPECT_EQ(30, by_name->get_age());
}

} // namespace test_table_shm_engine
