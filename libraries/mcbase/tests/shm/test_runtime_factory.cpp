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

#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <unistd.h>

#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/shm_runtime.h>
#include <mc/shm/string.h>

namespace {

std::string make_unique_region_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" + std::to_string(now_ns);
}

class shm_runtime_factory_test : public ::testing::Test {
protected:
    void SetUp() override { m_region_name = make_unique_region_name("mc_shm_factory"); }

    void TearDown() override { mc::shm::detail::shared_memory_backend::remove(m_region_name); }

    mc::shm::runtime_options make_opts() const
    {
        mc::shm::runtime_options opts;
        opts.region_name = m_region_name;
        opts.region_size = 4 * 1024 * 1024;
        return opts;
    }

    std::string m_region_name;
};

// ---------------------------------------------------------------------------
// list 工厂
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_test, list_create_then_reopen_sees_same_data)
{
    {
        mc::shm::shm_runtime rt(make_opts());
        ASSERT_TRUE(rt.is_valid());
        auto lst = rt.get_or_create_list<int>("numbers");
        ASSERT_TRUE(lst.has_value());
        lst->push_back(1);
        lst->push_back(2);
        lst->push_back(3);
        EXPECT_EQ(lst->size(), 3u);
    }

    {
        mc::shm::shm_runtime rt(make_opts());
        ASSERT_TRUE(rt.is_valid());
        auto lst = rt.get_or_create_list<int>("numbers");
        ASSERT_TRUE(lst.has_value());
        EXPECT_EQ(lst->size(), 3u);
        std::vector<int> values;
        for (auto& v : *lst) {
            values.push_back(v);
        }
        EXPECT_EQ(values, (std::vector<int>{1, 2, 3}));
    }
}

TEST_F(shm_runtime_factory_test, set_create_then_reopen_sees_same_data)
{
    {
        mc::shm::shm_runtime rt(make_opts());
        ASSERT_TRUE(rt.is_valid());
        auto s = rt.get_or_create_set<int>("ints");
        ASSERT_TRUE(s.has_value());
        EXPECT_TRUE(s->insert(10).second);
        EXPECT_TRUE(s->insert(20).second);
        EXPECT_TRUE(s->insert(5).second);
        EXPECT_EQ(s->size(), 3u);
    }

    {
        mc::shm::shm_runtime rt(make_opts());
        ASSERT_TRUE(rt.is_valid());
        auto s = rt.get_or_create_set<int>("ints");
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->size(), 3u);
        std::vector<int> ordered;
        for (const auto& v : *s) {
            ordered.push_back(v);
        }
        EXPECT_EQ(ordered, (std::vector<int>{5, 10, 20}));
    }
}

TEST_F(shm_runtime_factory_test, map_create_then_reopen_sees_same_data)
{
    {
        mc::shm::shm_runtime rt(make_opts());
        ASSERT_TRUE(rt.is_valid());
        auto m = rt.get_or_create_map<int, int>("by_key");
        ASSERT_TRUE(m.has_value());
        EXPECT_TRUE(m->try_emplace(1, 100).second);
        EXPECT_TRUE(m->try_emplace(2, 200).second);
        EXPECT_TRUE(m->try_emplace(3, 300).second);
        EXPECT_EQ(m->size(), 3u);
    }

    {
        mc::shm::shm_runtime rt(make_opts());
        ASSERT_TRUE(rt.is_valid());
        auto m = rt.get_or_create_map<int, int>("by_key");
        ASSERT_TRUE(m.has_value());
        EXPECT_EQ(m->size(), 3u);
        auto entry = m->find(2);
        ASSERT_TRUE(entry);
        EXPECT_EQ(*entry.value, 200);
    }
}

// ---------------------------------------------------------------------------
// 同 runtime 内重复获取等效 wrapper
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_test, multiple_calls_return_equivalent_wrappers)
{
    mc::shm::shm_runtime rt(make_opts());
    ASSERT_TRUE(rt.is_valid());

    auto a = rt.get_or_create_set<int>("shared");
    auto b = rt.get_or_create_set<int>("shared");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(a->control(), b->control());

    a->insert(42);
    EXPECT_TRUE(b->find(42));
    EXPECT_EQ(a->size(), b->size());
}

// ---------------------------------------------------------------------------
// 类型签名校验：同名不同类型必须拒绝
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_test, type_mismatch_on_reopen_rejected)
{
    {
        mc::shm::shm_runtime rt(make_opts());
        ASSERT_TRUE(rt.is_valid());
        auto s = rt.get_or_create_set<int>("typed");
        ASSERT_TRUE(s.has_value());
        s->insert(1);
    }

    {
        mc::shm::shm_runtime rt(make_opts());
        ASSERT_TRUE(rt.is_valid());
        // 不同 Key 类型（sizeof/align 不同 → signature 不匹配）
        auto bad = rt.get_or_create_set<std::int64_t>("typed");
        EXPECT_FALSE(bad.has_value());

        // 不同容器 kind（set vs list）属于不同 name-space，互不冲突
        auto l = rt.get_or_create_list<int>("typed");
        EXPECT_TRUE(l.has_value());
    }
}

// ---------------------------------------------------------------------------
// list / set / map 同 name 可共存（不同 kind 命名空间）
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_test, same_name_across_kinds_are_independent)
{
    mc::shm::shm_runtime rt(make_opts());
    ASSERT_TRUE(rt.is_valid());

    auto l = rt.get_or_create_list<int>("store");
    auto s = rt.get_or_create_set<int>("store");
    auto m = rt.get_or_create_map<int, int>("store");
    ASSERT_TRUE(l.has_value());
    ASSERT_TRUE(s.has_value());
    ASSERT_TRUE(m.has_value());

    l->push_back(10);
    s->insert(20);
    m->try_emplace(30, 300);

    EXPECT_EQ(l->size(), 1u);
    EXPECT_EQ(s->size(), 1u);
    EXPECT_EQ(m->size(), 1u);

    EXPECT_FALSE(s->find(10));   // list 的数据不污染 set
    EXPECT_FALSE(m->find(20));   // set 的数据不污染 map
}

// ---------------------------------------------------------------------------
// 空名 / 超长名 拒绝
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_test, invalid_names_rejected)
{
    mc::shm::shm_runtime rt(make_opts());
    ASSERT_TRUE(rt.is_valid());

    EXPECT_FALSE((rt.get_or_create_list<int>("").has_value()));
    EXPECT_FALSE((rt.get_or_create_set<int>("").has_value()));
    EXPECT_FALSE((rt.get_or_create_map<int, int>("").has_value()));

    // prefix 占 8 字节，root_name 上限 192，用户名长度需 < 184
    std::string too_long(200, 'x');
    EXPECT_FALSE((rt.get_or_create_list<int>(too_long).has_value()));
}

// ---------------------------------------------------------------------------
// drop_named_container：仅允许在空容器上执行
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_test, drop_requires_empty_container)
{
    mc::shm::shm_runtime rt(make_opts());
    ASSERT_TRUE(rt.is_valid());

    auto l = rt.get_or_create_list<int>("to_drop");
    ASSERT_TRUE(l.has_value());
    l->push_back(99);

    // 非空，drop 拒绝
    EXPECT_FALSE(rt.drop_named_container("to_drop"));
    EXPECT_TRUE(l->pop_front());
    EXPECT_EQ(l->size(), 0u);

    EXPECT_TRUE(rt.drop_named_container("to_drop"));
    // drop 之后再 create 是一个全新初始化的 control（allocator 可能复用同一块地址，
    // 关键是内容被重置且 size=0、可正常操作）
    auto l2 = rt.get_or_create_list<int>("to_drop");
    ASSERT_TRUE(l2.has_value());
    EXPECT_EQ(l2->size(), 0u);
    l2->push_back(7);
    EXPECT_EQ(l2->size(), 1u);
}

TEST_F(shm_runtime_factory_test, drop_unknown_name_returns_false)
{
    mc::shm::shm_runtime rt(make_opts());
    ASSERT_TRUE(rt.is_valid());
    EXPECT_FALSE(rt.drop_named_container("never_created"));
}

// ---------------------------------------------------------------------------
// 同进程多线程并发创建同名容器，必须收敛到同一控制块
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_test, concurrent_create_converges_to_single_control)
{
    mc::shm::shm_runtime rt(make_opts());
    ASSERT_TRUE(rt.is_valid());

    constexpr int                    thread_count = 8;
    std::vector<std::thread>         workers;
    std::vector<mc::shm::container::set_control*> ctrls(thread_count, nullptr);

    for (int i = 0; i < thread_count; ++i) {
        workers.emplace_back([&, i]() {
            auto s = rt.get_or_create_set<int>("race");
            if (s.has_value()) {
                ctrls[i] = s->control();
                s->insert(i);
            }
        });
    }
    for (auto& t : workers) t.join();

    ASSERT_NE(ctrls[0], nullptr);
    for (int i = 1; i < thread_count; ++i) {
        EXPECT_EQ(ctrls[i], ctrls[0]);
    }
    auto s = rt.get_or_create_set<int>("race");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->size(), static_cast<std::size_t>(thread_count));
}

// ---------------------------------------------------------------------------
// shm::string 作 Key 的 set 也能通过工厂创建并持久化
// ---------------------------------------------------------------------------

TEST_F(shm_runtime_factory_test, set_of_shm_string_works_via_factory)
{
    {
        mc::shm::shm_runtime rt(make_opts());
        ASSERT_TRUE(rt.is_valid());
        auto s = rt.get_or_create_set<mc::shm::string, mc::shm::string_less>("paths");
        ASSERT_TRUE(s.has_value());

        auto arena = rt.user_arena();
        auto a     = mc::shm::string::create(arena, "/a");
        auto b     = mc::shm::string::create(arena, "/b");
        ASSERT_FALSE(a.empty());
        ASSERT_FALSE(b.empty());
        EXPECT_TRUE(s->insert(std::move(a)).second);
        EXPECT_TRUE(s->insert(std::move(b)).second);
    }

    {
        mc::shm::shm_runtime rt(make_opts());
        ASSERT_TRUE(rt.is_valid());
        auto s = rt.get_or_create_set<mc::shm::string, mc::shm::string_less>("paths");
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->size(), 2u);
        EXPECT_TRUE(s->find(std::string_view("/a")));
        EXPECT_TRUE(s->find(std::string_view("/b")));

        // 清理：先清空才能安全 drop
        s->clear();
        EXPECT_EQ(s->size(), 0u);
        EXPECT_TRUE(rt.drop_named_container("paths"));
    }
}

} // namespace
