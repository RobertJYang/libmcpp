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
#include <random>
#include <string>

#include <mc/quark.h>
#include <mc/shm/default_runtime.h>
#include <mc/shm/shm_runtime.h>

#include "quark/include/shm_quark_provider.h"

namespace {

mc::string _unique_region_name(const char* prefix)
{
    const auto         now    = std::chrono::steady_clock::now().time_since_epoch().count();
    static std::mt19937_64 rng{static_cast<std::uint64_t>(now)};
    const auto         suffix = rng();
    return mc::string(prefix) + std::to_string(suffix).c_str();
}

mc::shm::runtime_options _ephemeral_options(const char* prefix)
{
    mc::shm::runtime_options opts;
    opts.region_name     = _unique_region_name(prefix);
    opts.region_size     = 4U * 1024U * 1024U;
    opts.manager_process = true;
    return opts;
}

} // namespace

TEST(quark_shm, basic_intern_and_resolve)
{
    mc::shm::shm_runtime runtime(_ephemeral_options("/mc.quark.shm.basic."));
    ASSERT_TRUE(runtime.is_valid());

    mc::quark_detail::shm_quark_provider provider(runtime, 16U, 64U, 16U * 1024U);
    ASSERT_TRUE(provider.is_valid());

    const auto id_a = provider.intern(mc::string_view("alpha.shm"));
    EXPECT_NE(id_a, mc::quark::invalid_id);
    EXPECT_NE(id_a, mc::quark::empty_id);

    const auto id_a_again = provider.intern(mc::string_view("alpha.shm"));
    EXPECT_EQ(id_a, id_a_again);

    const auto* record = provider.resolve(id_a);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->view(), mc::string_view("alpha.shm"));
}

TEST(quark_shm, empty_string_returns_empty_id)
{
    mc::shm::shm_runtime runtime(_ephemeral_options("/mc.quark.shm.empty."));
    ASSERT_TRUE(runtime.is_valid());

    mc::quark_detail::shm_quark_provider provider(runtime, 8U, 16U, 4U * 1024U);
    ASSERT_TRUE(provider.is_valid());

    EXPECT_EQ(provider.intern(mc::string_view()), mc::quark::empty_id);
    EXPECT_EQ(provider.lookup(mc::string_view()), mc::quark::empty_id);

    const auto* record = provider.resolve(mc::quark::empty_id);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->size, 0U);
    EXPECT_STREQ(record->data_ptr(), "");
}

TEST(quark_shm, lookup_finds_only_existing)
{
    mc::shm::shm_runtime runtime(_ephemeral_options("/mc.quark.shm.lookup."));
    ASSERT_TRUE(runtime.is_valid());

    mc::quark_detail::shm_quark_provider provider(runtime, 8U, 16U, 4U * 1024U);
    ASSERT_TRUE(provider.is_valid());

    EXPECT_EQ(provider.lookup(mc::string_view("never.seen")), mc::quark::invalid_id);

    const auto id = provider.intern(mc::string_view("seen.shm"));
    EXPECT_NE(id, mc::quark::invalid_id);
    EXPECT_EQ(provider.lookup(mc::string_view("seen.shm")), id);
}

TEST(quark_shm, attach_existing_region_yields_same_ids)
{
    auto opts = _ephemeral_options("/mc.quark.shm.attach.");

    mc::quark::id_type id_alpha = mc::quark::invalid_id;
    mc::quark::id_type id_beta  = mc::quark::invalid_id;

    {
        mc::shm::shm_runtime runtime(opts);
        ASSERT_TRUE(runtime.is_valid());
        mc::quark_detail::shm_quark_provider provider(runtime, 16U, 64U, 16U * 1024U);
        ASSERT_TRUE(provider.is_valid());

        id_alpha = provider.intern(mc::string_view("attach.alpha"));
        id_beta  = provider.intern(mc::string_view("attach.beta"));
        EXPECT_NE(id_alpha, mc::quark::invalid_id);
        EXPECT_NE(id_beta, mc::quark::invalid_id);
        EXPECT_NE(id_alpha, id_beta);
    }

    auto attach_opts = opts;
    attach_opts.manager_process = false;

    mc::shm::shm_runtime runtime2(attach_opts);
    ASSERT_TRUE(runtime2.is_valid());
    mc::quark_detail::shm_quark_provider provider2(runtime2, 16U, 64U, 16U * 1024U);
    ASSERT_TRUE(provider2.is_valid());

    EXPECT_EQ(provider2.lookup(mc::string_view("attach.alpha")), id_alpha);
    EXPECT_EQ(provider2.lookup(mc::string_view("attach.beta")), id_beta);

    EXPECT_EQ(provider2.intern(mc::string_view("attach.alpha")), id_alpha);

    const auto* record = provider2.resolve(id_alpha);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->view(), mc::string_view("attach.alpha"));
}

TEST(quark_shm, capacity_overflow_falls_back_to_invalid)
{
    mc::shm::shm_runtime runtime(_ephemeral_options("/mc.quark.shm.cap."));
    ASSERT_TRUE(runtime.is_valid());

    mc::quark_detail::shm_quark_provider provider(runtime, 8U, 4U, 16U * 1024U);
    ASSERT_TRUE(provider.is_valid());

    for (int i = 0; i < 6; ++i) {
        std::string s   = "shm.cap.fits." + std::to_string(i);
        const auto  id  = provider.intern(mc::string_view(s.data(), s.size()));
        EXPECT_NE(id, mc::quark::invalid_id);
        EXPECT_NE(id, mc::quark::empty_id);
    }

    const auto overflow = provider.intern(mc::string_view("shm.cap.over"));
    EXPECT_EQ(overflow, mc::quark::invalid_id);
}

TEST(default_runtime, lazy_init_returns_same_instance)
{
    setenv("MC_SHM_DEFAULT_REGION",
           _unique_region_name("/mc.default.runtime.").data(), 1);
    setenv("MC_SHM_DEFAULT_SIZE", "1048576", 1);
    setenv("MC_SHM_DEFAULT_MANAGER", "1", 1);
    mc::shm::shutdown_default_runtime();
    EXPECT_FALSE(mc::shm::default_runtime_initialized());

    auto& a = mc::shm::default_runtime();
    auto& b = mc::shm::default_runtime();
    EXPECT_TRUE(mc::shm::default_runtime_initialized());
    EXPECT_EQ(&a, &b);
    EXPECT_TRUE(a.is_valid());

    mc::shm::shutdown_default_runtime();
    EXPECT_FALSE(mc::shm::default_runtime_initialized());
    unsetenv("MC_SHM_DEFAULT_REGION");
    unsetenv("MC_SHM_DEFAULT_SIZE");
    unsetenv("MC_SHM_DEFAULT_MANAGER");
}

TEST(default_runtime, explicit_init_wins_over_env)
{
    mc::shm::shutdown_default_runtime();

    mc::shm::runtime_options opts;
    opts.region_name     = _unique_region_name("/mc.default.runtime.explicit.");
    opts.region_size     = 2U * 1024U * 1024U;
    opts.manager_process = true;

    EXPECT_TRUE(mc::shm::init_default_runtime(opts));
    EXPECT_TRUE(mc::shm::default_runtime_initialized());

    auto& runtime = mc::shm::default_runtime();
    EXPECT_TRUE(runtime.is_valid());

    EXPECT_FALSE(mc::shm::init_default_runtime(opts));

    mc::shm::shutdown_default_runtime();
}
