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

// dbus_proto 骨架单元测试：当前实现仅保留构造 + outbound_count 计数；真实
// DBus 编解码和根 filter 挂接待后续迭代。这里只做 API 合约校验：
// (1) 能以默认 connection 构造、(2) 单独 push 一次会自增计数、(3) 多次 push 累加。

#include <gtest/gtest.h>
#include <mc/app/dbus_proto.h>
#include <mc/dbus/connection.h>
#include <mc/protocol/request.h>

namespace test_dbus_proto {

struct dummy_context : public mc::proto::proto_context {};

void _push_once(mc::app::dbus_proto& proto)
{
    mc::proto::proto_request req;
    proto.push(req);
}

TEST(dbus_proto_test, default_outbound_count_is_zero)
{
    mc::app::dbus_proto proto("mc.test.dbus_proto.zero", mc::dbus::connection{});

    EXPECT_EQ(proto.outbound_count(), 0U);
    EXPECT_EQ(proto.service_name(), mc::string_view("mc.test.dbus_proto.zero"));
}

TEST(dbus_proto_test, single_push_increments_outbound_count)
{
    mc::app::dbus_proto proto("mc.test.dbus_proto.one", mc::dbus::connection{});

    _push_once(proto);

    EXPECT_EQ(proto.outbound_count(), 1U);
}

TEST(dbus_proto_test, repeated_push_accumulates_counter)
{
    mc::app::dbus_proto proto("mc.test.dbus_proto.many", mc::dbus::connection{});

    for (int i = 0; i < 5; ++i) {
        _push_once(proto);
    }

    EXPECT_EQ(proto.outbound_count(), 5U);
}

} // namespace test_dbus_proto
