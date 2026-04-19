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

//
// 持久化字节布局 snapshot
//
// 目的：set_control / map_control / list_control / node_header /
// container_journal 是跨进程、跨进程重启、甚至跨版本可能需要兼容的"落盘"
// 结构。任何无心的字段重排、类型变更都可能导致已有 shm region 数据无法被新
// 版本进程识别。
//
// 本测试锁定 sizeof / alignof / 关键字段 offset 的基线值。若有合理变更需
// 同步更新基线，并在 plan 文档中声明兼容策略（migration / version bump）。
//
// 基线值（截至 2026-04）：
//   set_control       320B / 64B-aligned
//   map_control       320B / 64B-aligned
//   list_control      256B / 64B-aligned
//   node_header        16B / 16B-aligned
//   container_journal  64B / 64B-aligned
//

#include <gtest/gtest.h>

#include <cstddef>

#include <mc/shm/container/journal.h>
#include <mc/shm/container/list.h>
#include <mc/shm/container/map.h>
#include <mc/shm/container/node_header.h>
#include <mc/shm/container/set.h>

using mc::shm::container::container_journal;
using mc::shm::container::list_control;
using mc::shm::container::map_control;
using mc::shm::container::node_header;
using mc::shm::container::set_control;

// -------------------------------------------------------------
// sizeof / alignof 基线
// -------------------------------------------------------------
TEST(shm_layout_snapshot, set_control_size_and_align_are_stable)
{
    EXPECT_EQ(sizeof(set_control), 320U)
        << "set_control 的 sizeof 发生变化，可能破坏已有 region 的持久化兼容性";
    EXPECT_EQ(alignof(set_control), 64U);
}

TEST(shm_layout_snapshot, map_control_size_and_align_are_stable)
{
    EXPECT_EQ(sizeof(map_control), 320U)
        << "map_control 的 sizeof 发生变化，可能破坏已有 region 的持久化兼容性";
    EXPECT_EQ(alignof(map_control), 64U);
}

TEST(shm_layout_snapshot, list_control_size_and_align_are_stable)
{
    EXPECT_EQ(sizeof(list_control), 256U)
        << "list_control 的 sizeof 发生变化，可能破坏已有 region 的持久化兼容性";
    EXPECT_EQ(alignof(list_control), 64U);
}

TEST(shm_layout_snapshot, node_header_size_and_align_are_stable)
{
    EXPECT_EQ(sizeof(node_header), 16U)
        << "node_header 的 sizeof 发生变化：每个节点前缀尺寸变化会导致所有"
           "已经落盘的节点布局不兼容";
    EXPECT_EQ(alignof(node_header), 16U);
}

TEST(shm_layout_snapshot, container_journal_size_and_align_are_stable)
{
    EXPECT_EQ(sizeof(container_journal), 64U)
        << "container_journal 的 sizeof 发生变化：会破坏崩溃恢复的重放路径";
    EXPECT_EQ(alignof(container_journal), 64U);
}

// -------------------------------------------------------------
// 关键字段 offset：跨版本比对最敏感的 ABI 面
// 这些字段决定了 init/recover 的兼容性，发生漂移必须感知。
// -------------------------------------------------------------
TEST(shm_layout_snapshot, list_control_key_fields_offset)
{
    EXPECT_EQ(offsetof(list_control, head_offset), 128U);
    EXPECT_EQ(offsetof(list_control, tail_offset), 136U);
    EXPECT_EQ(offsetof(list_control, node_size), 144U);
    EXPECT_EQ(offsetof(list_control, self_tag), 152U);
}

TEST(shm_layout_snapshot, set_control_key_fields_offset)
{
    // set_control 头部布局：mutex(?) + journal(64B@offset?) + head_forward[]
    // 这里锁定 self_tag / key_size / key_align / max_level 这四个"init 入参"
    // 字段的相对顺序（通过 pairwise delta 校验，避免 header 尺寸改动打不准）
    EXPECT_EQ(offsetof(set_control, key_size) - offsetof(set_control, self_tag), 4U);
    EXPECT_EQ(offsetof(set_control, key_align) - offsetof(set_control, key_size), 4U);
    EXPECT_EQ(offsetof(set_control, max_level) - offsetof(set_control, key_align), 4U);
}

TEST(shm_layout_snapshot, map_control_key_fields_offset)
{
    EXPECT_EQ(offsetof(map_control, key_size) - offsetof(map_control, self_tag), 4U);
    EXPECT_EQ(offsetof(map_control, key_align) - offsetof(map_control, key_size), 4U);
    EXPECT_EQ(offsetof(map_control, value_size) - offsetof(map_control, key_align), 4U);
    EXPECT_EQ(offsetof(map_control, value_align) - offsetof(map_control, value_size), 4U);
    EXPECT_EQ(offsetof(map_control, max_level) - offsetof(map_control, value_align), 4U);
}
