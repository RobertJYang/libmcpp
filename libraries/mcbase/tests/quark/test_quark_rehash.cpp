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

#include <string>
#include <unordered_map>
#include <vector>

#include "quark/include/process_quark_provider.h"
#include <mc/quark.h>

TEST(quark_rehash, ids_remain_stable_across_rehash)
{
    mc::quark_detail::process_quark_provider provider(8U, 1024U);
    EXPECT_EQ(provider.bucket_count(), 8U);

    std::unordered_map<std::string, mc::quark::id_type> ids;
    for (std::size_t i = 0U; i < 256U; ++i) {
        const std::string key = "rehash.key." + std::to_string(i);
        const auto        id  = provider.intern(mc::string_view(key.data(), key.size()));
        ids[key]              = id;
    }

    EXPECT_GT(provider.bucket_count(), 8U);

    for (const auto& [key, id] : ids) {
        const auto re_id = provider.intern(mc::string_view(key.data(), key.size()));
        EXPECT_EQ(re_id, id) << "id changed after rehash for " << key;

        const auto* record = provider.resolve(id);
        ASSERT_NE(record, nullptr);
        EXPECT_EQ(std::string(record->data_ptr(), record->size), key);
    }
}

TEST(quark_rehash, lookup_finds_all_after_rehash)
{
    mc::quark_detail::process_quark_provider provider(8U, 1024U);

    std::vector<std::string> keys;
    for (std::size_t i = 0U; i < 200U; ++i) {
        keys.emplace_back("rehash.lookup." + std::to_string(i));
        provider.intern(mc::string_view(keys.back().data(), keys.back().size()));
    }

    for (const auto& key : keys) {
        const auto id = provider.lookup(mc::string_view(key.data(), key.size()));
        EXPECT_NE(id, mc::quark::invalid_id) << key;
    }

    const auto missing = provider.lookup(mc::string_view("rehash.lookup.never"));
    EXPECT_EQ(missing, mc::quark::invalid_id);
}

TEST(quark_rehash, soft_max_does_not_reject_intern)
{
    mc::quark_detail::process_quark_provider provider(8U, 4U);

    for (std::size_t i = 0U; i < 32U; ++i) {
        const std::string key = "rehash.softmax." + std::to_string(i);
        const auto        id  = provider.intern(mc::string_view(key.data(), key.size()));
        EXPECT_NE(id, mc::quark::invalid_id);
    }

    EXPECT_EQ(provider.size(), 32U + 1U);  // +1 for empty_id placeholder
}

TEST(quark_rehash, empty_id_survives_rehash)
{
    mc::quark_detail::process_quark_provider provider(8U, 1024U);

    EXPECT_EQ(provider.intern(mc::string_view("")), mc::quark::empty_id);

    for (std::size_t i = 0U; i < 100U; ++i) {
        const std::string key = "rehash.empty.survive." + std::to_string(i);
        provider.intern(mc::string_view(key.data(), key.size()));
    }

    EXPECT_EQ(provider.lookup(mc::string_view("")), mc::quark::empty_id);
    const auto* record = provider.resolve(mc::quark::empty_id);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->size, 0U);
    EXPECT_STREQ(record->data_ptr(), "");
}
