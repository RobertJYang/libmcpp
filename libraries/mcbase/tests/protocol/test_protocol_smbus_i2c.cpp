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

/*
 * SMBus over I2C 示例：smbus_layer 声明 `push_headroom`，`ensure_packet` 里调用
 * `prepare_packet`（`stack_spec::push_headroom` 由各层汇总）；block read 结果放在 `request.packet()`，
 * 不另建应用侧缓冲区。
 */

#include <gtest/gtest.h>

#include <mc/protocol.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

// 仅描述「7bit 从机 + 写缓冲区 / 读长度」，I2C 层不解析 SMBus 命令或 Block 格式。
struct i2c_xfer_control {
    uint8_t     slave_7bit{0};
    std::size_t read_len{0};
};

struct smbus_layer;

struct smbus_layer {
    /// 与 `push` 上 `prepend(2)` 的 SMBus 头一致，供 `stack_spec::push_headroom` 聚合
    static constexpr std::size_t push_headroom = 2;

    struct state {
        enum class op {
            read_byte,
            write_byte,
            block_read,
        };

        op      op_kind{op::read_byte};
        uint8_t slave_7bit{0x50};
        uint8_t command{0x12};
        uint8_t write_value{0x34};
        uint8_t read_result{0};

        int              block_phase{0};
        std::uint8_t     block_count{0};
        i2c_xfer_control xfer{};
        int              read_byte_substep{0};
    };

    template <typename Ctx>
    static void ensure_packet(Ctx& ctx)
    {
        ctx.prepare_packet();
    }

    template <typename Ctx>
    static mc::protocol::command on_push(Ctx& ctx, mc::protocol::packet& packet)
    {
        auto& s = ctx.self();
        ensure_packet(ctx);

        if (s.op_kind == state::op::read_byte) {
            s.read_byte_substep = 0;
            packet.prepend(2);
            std::memcpy(packet.mutable_data(), "\xAA\x55", 2);
            packet.write(&s.command, 1);
            s.xfer.slave_7bit    = s.slave_7bit;
            s.xfer.read_len      = 0;
            packet.native_handle = &s.xfer;
            return ctx.push_next();
        }

        if (s.op_kind == state::op::write_byte) {
            packet.prepend(2);
            std::memcpy(packet.mutable_data(), "\xAA\x55", 2);
            packet.write(&s.command, 1);
            packet.write(&s.write_value, 1);
            s.xfer.slave_7bit    = s.slave_7bit;
            s.xfer.read_len      = 0;
            packet.native_handle = &s.xfer;
            return ctx.push_next();
        }

        if (s.op_kind == state::op::block_read) {
            s.block_phase = 0;
            packet.prepend(2);
            std::memcpy(packet.mutable_data(), "\xAA\x55", 2);
            packet.write(&s.command, 1);
            s.xfer.slave_7bit    = s.slave_7bit;
            s.xfer.read_len      = 0;
            packet.native_handle = &s.xfer;
            return ctx.push_next();
        }

        return ctx.complete();
    }

    template <typename Ctx>
    static mc::protocol::command on_pop(Ctx& ctx, mc::protocol::packet& packet)
    {
        auto& s = ctx.self();

        if (s.op_kind == state::op::read_byte) {
            if (s.read_byte_substep == 0) {
                s.read_byte_substep = 1;
                packet.clear();
                s.xfer.slave_7bit    = s.slave_7bit;
                s.xfer.read_len      = 1;
                packet.native_handle = &s.xfer;
                return ctx.push_next();
            }
            EXPECT_GE(packet.length(), 1U);
            s.read_result        = packet.data()[0];
            packet.native_handle = nullptr;
            return ctx.complete();
        }

        if (s.op_kind == state::op::write_byte) {
            packet.native_handle = nullptr;
            return ctx.complete();
        }

        if (s.op_kind == state::op::block_read) {
            if (s.block_phase == 0) {
                s.block_phase = 1;
                packet.clear();
                s.xfer.slave_7bit    = s.slave_7bit;
                s.xfer.read_len      = 1;
                packet.native_handle = &s.xfer;
                return ctx.push_next();
            }
            if (s.block_phase == 1) {
                EXPECT_GE(packet.length(), 1U);
                s.block_count = packet.data()[0];
                packet.clear();
                s.block_phase        = 2;
                s.xfer.slave_7bit    = s.slave_7bit;
                s.xfer.read_len      = s.block_count;
                packet.native_handle = &s.xfer;
                return ctx.push_next();
            }
            if (s.block_phase == 2) {
                EXPECT_EQ(packet.length(), static_cast<std::size_t>(s.block_count));
                packet.native_handle = nullptr;
                return ctx.complete();
            }
        }

        return ctx.complete();
    }
};

struct i2c_layer {
    struct state {
        struct bus_op {
            bool                      is_write{true};
            uint8_t                   addr_7bit{0};
            std::vector<std::uint8_t> bytes;
            int                       read_len{0};
        };

        std::vector<bus_op>       trace;
        std::vector<std::uint8_t> mock_read_stream;
        std::size_t               mock_read_cursor{0};
    };

    static std::uint8_t pop_mock_byte(state& bus)
    {
        if (bus.mock_read_cursor >= bus.mock_read_stream.size()) {
            ADD_FAILURE() << "mock_read_stream exhausted";
            return 0;
        }
        return bus.mock_read_stream[bus.mock_read_cursor++];
    }

    template <typename Ctx>
    static mc::protocol::command on_push(Ctx& ctx, mc::protocol::packet& packet)
    {
        auto* ctl = static_cast<i2c_xfer_control*>(packet.native_handle);
        if (ctl == nullptr) {
            return ctx.pop_next();
        }

        auto& bus = ctx.self();

        if (packet.length() > 0) {
            typename state::bus_op w;
            w.is_write  = true;
            w.addr_7bit = ctl->slave_7bit;
            w.bytes.assign(packet.data(), packet.data() + packet.length());
            bus.trace.push_back(std::move(w));
        }

        if (ctl->read_len > 0) {
            packet.clear();
            for (std::size_t i = 0; i < ctl->read_len; ++i) {
                const std::uint8_t b = pop_mock_byte(bus);
                packet.write(&b, 1);
            }
            typename state::bus_op r;
            r.is_write  = false;
            r.addr_7bit = ctl->slave_7bit;
            r.read_len  = static_cast<int>(ctl->read_len);
            bus.trace.push_back(std::move(r));
        }

        return ctx.pop_next();
    }
};

using smbus_i2c_stack = mc::protocol::stack_spec<smbus_layer, i2c_layer>;

static_assert(smbus_i2c_stack::push_headroom == 2U, "SMBus 与 I2C 栈 push headroom 聚合");

} // namespace

TEST(protocol_smbus_i2c, smbus_read_byte_maps_to_i2c_write_then_read)
{
    mc::protocol::request<smbus_i2c_stack> request;

    auto& smbus      = request.get<smbus_layer>();
    smbus.op_kind    = smbus_layer::state::op::read_byte;
    smbus.slave_7bit = 0x50;
    smbus.command    = 0x12;

    auto& i2c            = request.get<i2c_layer>();
    i2c.mock_read_stream = {0xAB};
    i2c.mock_read_cursor = 0;

    auto st = mc::protocol::runtime<smbus_i2c_stack>::push(request);
    EXPECT_EQ(st, mc::protocol::execution_state::completed);
    EXPECT_EQ(smbus.read_result, 0xAB);

    const auto& trace = i2c.trace;
    ASSERT_EQ(trace.size(), 2U);
    EXPECT_TRUE(trace[0].is_write);
    EXPECT_EQ(trace[0].addr_7bit, 0x50);
    ASSERT_EQ(trace[0].bytes.size(), 3U);
    EXPECT_EQ(trace[0].bytes[0], 0xAA);
    EXPECT_EQ(trace[0].bytes[1], 0x55);
    EXPECT_EQ(trace[0].bytes[2], 0x12);

    EXPECT_FALSE(trace[1].is_write);
    EXPECT_EQ(trace[1].read_len, 1);
}

TEST(protocol_smbus_i2c, smbus_write_byte_maps_to_single_i2c_write)
{
    mc::protocol::request<smbus_i2c_stack> request;

    auto& smbus       = request.get<smbus_layer>();
    smbus.op_kind     = smbus_layer::state::op::write_byte;
    smbus.slave_7bit  = 0x50;
    smbus.command     = 0x00;
    smbus.write_value = 0x56;

    auto& i2c = request.get<i2c_layer>();

    auto st = mc::protocol::runtime<smbus_i2c_stack>::push(request);
    EXPECT_EQ(st, mc::protocol::execution_state::completed);

    const auto& trace = i2c.trace;
    ASSERT_EQ(trace.size(), 1U);
    EXPECT_TRUE(trace[0].is_write);
    ASSERT_EQ(trace[0].bytes.size(), 4U);
    EXPECT_EQ(trace[0].bytes[0], 0xAA);
    EXPECT_EQ(trace[0].bytes[1], 0x55);
    EXPECT_EQ(trace[0].bytes[2], 0x00);
    EXPECT_EQ(trace[0].bytes[3], 0x56);
}

TEST(protocol_smbus_i2c, smbus_block_read_strips_count_and_exposes_packet)
{
    mc::protocol::request<smbus_i2c_stack> request;

    auto& smbus      = request.get<smbus_layer>();
    smbus.op_kind    = smbus_layer::state::op::block_read;
    smbus.slave_7bit = 0x50;
    smbus.command    = 0xBD;

    auto& i2c            = request.get<i2c_layer>();
    i2c.mock_read_stream = {3, 0x11, 0x22, 0x33};
    i2c.mock_read_cursor = 0;

    auto st = mc::protocol::runtime<smbus_i2c_stack>::push(request);
    EXPECT_EQ(st, mc::protocol::execution_state::completed);

    const auto& pkt = request.packet();
    ASSERT_EQ(pkt.length(), 3U);
    EXPECT_EQ(pkt.data()[0], 0x11);
    EXPECT_EQ(pkt.data()[1], 0x22);
    EXPECT_EQ(pkt.data()[2], 0x33);

    const auto& trace = i2c.trace;
    ASSERT_EQ(trace.size(), 3U);
    EXPECT_TRUE(trace[0].is_write);
    ASSERT_EQ(trace[0].bytes.size(), 3U);
    EXPECT_EQ(trace[0].bytes[0], 0xAA);
    EXPECT_EQ(trace[0].bytes[1], 0x55);
    EXPECT_EQ(trace[0].bytes[2], 0xBD);

    EXPECT_FALSE(trace[1].is_write);
    EXPECT_EQ(trace[1].read_len, 1);

    EXPECT_FALSE(trace[2].is_write);
    EXPECT_EQ(trace[2].read_len, 3);

    EXPECT_EQ(i2c.mock_read_cursor, i2c.mock_read_stream.size());
}
