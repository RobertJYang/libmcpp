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

// shm_service POD 布局测试
//
// 范围：
//   1. sizeof / alignof / abi_version 锁定
//   2. 字段 offset 锁定（防止 reorder 破坏 ABI）
//   3. POD / triviality 性质
//   4. CRC32 helper：覆盖范围正确 + 篡改检测 + abi_version 校验

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "shm_service.h"

using mc::engine::shm_service;
using mc::engine::shm_service_abi_version;
using mc::engine::shm_service_check;
using mc::engine::shm_service_compute_crc;
using mc::engine::shm_service_state;

namespace {

// sizeof / alignof / abi_version

TEST(shm_service_layout, sizeof_is_64)
{
    EXPECT_EQ(sizeof(shm_service), 64U);
    EXPECT_EQ(alignof(shm_service), 8U);
}

TEST(shm_service_layout, abi_version_is_v1)
{
    EXPECT_EQ(shm_service_abi_version, 1U);
}

// 字段 offset

TEST(shm_service_layout, field_offsets_locked)
{
    EXPECT_EQ(offsetof(shm_service, abi_version), 0U);
    EXPECT_EQ(offsetof(shm_service, state), 2U);
    EXPECT_EQ(offsetof(shm_service, _hdr_pad), 3U);
    EXPECT_EQ(offsetof(shm_service, crc32_self), 4U);
    EXPECT_EQ(offsetof(shm_service, pid), 8U);
    EXPECT_EQ(offsetof(shm_service, _pad32), 12U);
    EXPECT_EQ(offsetof(shm_service, epoch), 16U);
    EXPECT_EQ(offsetof(shm_service, service_name), 24U);
    EXPECT_EQ(offsetof(shm_service, _reserved), 32U);
}

// POD 性质

TEST(shm_service_layout, is_trivially_destructible_and_standard_layout)
{
    EXPECT_TRUE(std::is_trivially_destructible_v<shm_service>);
    EXPECT_TRUE(std::is_standard_layout_v<shm_service>);
}

// CRC32 helper

TEST(shm_service_crc, compute_and_check)
{
    shm_service svc{};
    svc.abi_version = shm_service_abi_version;
    svc.state       = static_cast<std::uint8_t>(shm_service_state::alive);
    svc.pid         = 0xC0FFEE12U;
    svc.epoch       = 1U;
    svc.crc32_self  = shm_service_compute_crc(svc);
    EXPECT_TRUE(shm_service_check(svc));

    svc.pid = 0xCAFEBABEU;
    EXPECT_FALSE(shm_service_check(svc));
}

TEST(shm_service_crc, compute_skips_crc_field_itself)
{
    shm_service svc{};
    svc.abi_version    = shm_service_abi_version;
    svc.pid            = 100U;
    svc.epoch          = 7U;
    auto crc_before    = shm_service_compute_crc(svc);
    svc.crc32_self     = 0xFFFFFFFFU;
    auto crc_after     = shm_service_compute_crc(svc);
    EXPECT_EQ(crc_before, crc_after);
}

TEST(shm_service_crc, check_rejects_wrong_abi_version)
{
    shm_service svc{};
    svc.abi_version = shm_service_abi_version + 1;
    svc.crc32_self  = shm_service_compute_crc(svc);
    EXPECT_FALSE(shm_service_check(svc));
}

}  // namespace
