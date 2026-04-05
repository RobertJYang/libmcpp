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
 * MCTP over I2C 示例（简化帧格式，演示 `mc::protocol::request` 与 `mc::protocol::runtime` 用法）：
 *
 * - 各层 `push_headroom` 汇总为 `stack_spec::push_headroom`；`prepare_packet` 设置 `payload_base`，
 *   可用 `request.prepare_packet().append_payload(...)` 写应用层明文。
 * - 出站：`append_payload` 写入后，MCTP 层把首字节写成 [分段标记][数据]；I2C 写只取前 `write_len` 字节。
 * - 入站：`multi_read_rx` 在 MCTP `on_push` 里调用 `ctx.prepare_inbound()`（按各层 `pop_*` 预留），不再裸 `clear`。
 */

#include <gtest/gtest.h>

#include <mc/protocol.h>

#include <cstdint>
#include <limits>
#include <vector>

namespace {

struct i2c_xfer_control {
    uint8_t slave_7bit{0};

    std::size_t read_len{0};
    std::size_t write_len{0};

    /// 读路径下 mock 字节写入 `packet` 的起始偏移；`read_dest_replace` 表示先 `clear` 再从头写入。
    static constexpr std::size_t read_dest_replace = std::numeric_limits<std::size_t>::max();
    std::size_t                  read_dest_offset{read_dest_replace};
};

struct mctp_layer;

struct mctp_layer {
    static constexpr std::size_t push_headroom = 4;
    static constexpr std::size_t pop_tailroom  = 64;

    enum class scenario {
        fragmented_tx,
        multi_read_rx,
    };

    struct state {
        scenario         which{scenario::fragmented_tx};
        uint8_t          slave_7bit{0x1D};
        int              tx_segments_done{0};
        i2c_xfer_control xfer{};
    };

    static constexpr std::uint8_t marker_sof = 0x81;
    static constexpr std::uint8_t marker_mid = 0x82;
    static constexpr std::uint8_t marker_eof = 0x83;

    static void build_tx_segment(state& s, mc::protocol::packet& packet)
    {
        const std::size_t total = packet.length();
        const std::size_t base  = packet.payload_base();
        if (total < base) {
            return;
        }
        const std::size_t plain_len = total - base;
        const int         idx       = s.tx_segments_done;
        if (idx < 0 || static_cast<std::size_t>(idx) >= plain_len) {
            return;
        }
        std::uint8_t m = marker_mid;
        if (idx == 0) {
            m = marker_sof;
        } else if (idx == static_cast<int>(plain_len) - 1) {
            m = marker_eof;
        }
        const std::uint8_t b = packet.data()[base + static_cast<std::size_t>(idx)];
        packet.write_at_offset(0, &m, 1);
        packet.write_at_offset(1, &b, 1);
        s.xfer.slave_7bit       = s.slave_7bit;
        s.xfer.read_len         = 0;
        s.xfer.write_len        = 2;
        s.xfer.read_dest_offset = i2c_xfer_control::read_dest_replace;
        packet.native_handle    = &s.xfer;
    }

    template <typename Ctx>
    static mc::protocol::command on_push(Ctx& ctx, mc::protocol::packet& packet)
    {
        auto& s = ctx.self();

        if (s.which == scenario::fragmented_tx) {
            s.tx_segments_done = 0;
            build_tx_segment(s, packet);
            return ctx.push_next();
        }

        if (s.which == scenario::multi_read_rx) {
            ctx.prepare_inbound();
            s.xfer.slave_7bit       = s.slave_7bit;
            s.xfer.read_len         = 2;
            s.xfer.write_len        = 0;
            s.xfer.read_dest_offset = 0;
            packet.native_handle    = &s.xfer;
            return ctx.push_next();
        }

        return ctx.complete();
    }

    template <typename Ctx>
    static mc::protocol::command on_pop(Ctx& ctx, mc::protocol::packet& packet)
    {
        auto& s = ctx.self();

        if (s.which == scenario::fragmented_tx) {
            ++s.tx_segments_done;
            const std::size_t total     = packet.length();
            const std::size_t base      = packet.payload_base();
            const std::size_t plain_len = total >= base ? total - base : 0;
            if (plain_len == 0 || s.tx_segments_done >= static_cast<int>(plain_len)) {
                packet.native_handle = nullptr;
                return ctx.complete();
            }
            build_tx_segment(s, packet);
            return ctx.push_next();
        }

        if (s.which == scenario::multi_read_rx) {
            EXPECT_GE(packet.length(), 2U);
            if (packet.length() < 2U) {
                return ctx.complete();
            }
            const std::uint8_t* d      = packet.data();
            const std::size_t   off    = packet.length() - 2;
            const std::uint8_t  marker = d[off];
            const std::uint8_t  byte   = d[off + 1];
            packet.strip_suffix(2);
            packet.write(&byte, 1);
            if (marker == marker_eof) {
                packet.native_handle = nullptr;
                return ctx.complete();
            }
            s.xfer.slave_7bit       = s.slave_7bit;
            s.xfer.read_len         = 2;
            s.xfer.write_len        = 0;
            s.xfer.read_dest_offset = packet.length();
            packet.native_handle    = &s.xfer;
            return ctx.push_next();
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

        // 读路径上 packet 可能已有累积数据，不能仅凭「有字节」就当成一次写操作。
        if (ctl->read_len == 0 && packet.length() > 0) {
            typename state::bus_op w;
            w.is_write          = true;
            w.addr_7bit         = ctl->slave_7bit;
            const std::size_t n = ctl->write_len > 0 ? ctl->write_len : packet.length();
            w.bytes.assign(packet.data(), packet.data() + n);
            bus.trace.push_back(std::move(w));
        }

        if (ctl->read_len > 0) {
            if (ctl->read_dest_offset == i2c_xfer_control::read_dest_replace) {
                packet.clear();
                for (std::size_t i = 0; i < ctl->read_len; ++i) {
                    const std::uint8_t b = pop_mock_byte(bus);
                    packet.write_at_offset(i, &b, 1);
                }
            } else {
                for (std::size_t i = 0; i < ctl->read_len; ++i) {
                    const std::uint8_t b = pop_mock_byte(bus);
                    packet.write_at_offset(ctl->read_dest_offset + i, &b, 1);
                }
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

using mctp_i2c_stack = mc::protocol::stack_spec<mctp_layer, i2c_layer>;

static_assert(mctp_i2c_stack::push_headroom == 4U, "MCTP 与 I2C 栈 push headroom 聚合");
static_assert(mctp_i2c_stack::pop_tailroom == 64U, "MCTP 层 pop tailroom 聚合");

} // namespace

TEST(protocol_mctp_i2c, push_splits_into_three_tagged_i2c_writes)
{
    mc::protocol::request<mctp_i2c_stack> request;

    auto& m      = request.get<mctp_layer>();
    m.which      = mctp_layer::scenario::fragmented_tx;
    m.slave_7bit = 0x1D;

    auto& i2c = request.get<i2c_layer>();

    const std::uint8_t plain[] = {0x10, 0x20, 0x30};
    request.prepare_packet().append_payload(plain);

    auto st = mc::protocol::runtime<mctp_i2c_stack>::push(request);
    EXPECT_EQ(st, mc::protocol::execution_state::completed);

    const auto& trace = i2c.trace;
    ASSERT_EQ(trace.size(), 3U);
    for (const auto& op : trace) {
        EXPECT_TRUE(op.is_write);
        EXPECT_EQ(op.addr_7bit, 0x1D);
        ASSERT_EQ(op.bytes.size(), 2U);
    }
    EXPECT_EQ(trace[0].bytes[0], mctp_layer::marker_sof);
    EXPECT_EQ(trace[0].bytes[1], 0x10);
    EXPECT_EQ(trace[1].bytes[0], mctp_layer::marker_mid);
    EXPECT_EQ(trace[1].bytes[1], 0x20);
    EXPECT_EQ(trace[2].bytes[0], mctp_layer::marker_eof);
    EXPECT_EQ(trace[2].bytes[1], 0x30);
}

TEST(protocol_mctp_i2c, multi_read_until_eof_reassembles_payload)
{
    mc::protocol::request<mctp_i2c_stack> request;

    auto& m      = request.get<mctp_layer>();
    m.which      = mctp_layer::scenario::multi_read_rx;
    m.slave_7bit = 0x1D;

    auto& i2c            = request.get<i2c_layer>();
    i2c.mock_read_stream = {mctp_layer::marker_sof, 0x01, mctp_layer::marker_mid, 0x02, mctp_layer::marker_eof, 0x03};
    i2c.mock_read_cursor = 0;

    auto st = mc::protocol::runtime<mctp_i2c_stack>::push(request);
    EXPECT_EQ(st, mc::protocol::execution_state::completed);

    const auto& pkt = request.packet();
    ASSERT_EQ(pkt.length(), 3U);
    EXPECT_EQ(pkt.data()[0], 0x01);
    EXPECT_EQ(pkt.data()[1], 0x02);
    EXPECT_EQ(pkt.data()[2], 0x03);

    const auto& trace = i2c.trace;
    ASSERT_EQ(trace.size(), 3U);
    for (const auto& op : trace) {
        EXPECT_FALSE(op.is_write);
        EXPECT_EQ(op.read_len, 2);
    }
    EXPECT_EQ(i2c.mock_read_cursor, i2c.mock_read_stream.size());
}
