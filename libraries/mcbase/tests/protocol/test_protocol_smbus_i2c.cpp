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
 * SMBus over I2C 示例：smbus_proto 声明 `push_headroom`，`ensure_buffer` 里调用
 * `prepare_buffer`（`stack_spec::push_headroom` 由各层汇总）；block read 结果放在 `request.buffer()`，
 * 不另建应用侧缓冲区。
 *
 * 这个测试刻意只使用 `push_next()` / `pop_next()` 往返驱动协议栈：
 * 1. SMBus 在 `on_push` 里把当前 I2C 事务描述压给下一层
 * 2. I2C 完成后经 `pop_next()` 回到 SMBus
 * 3. SMBus 若还需要下一次 I2C 事务，就在 `on_pop` 中返回 `push_next()`
 *
 * 也就是说，这里验证的是“push/pop 双向流转足以表达多步协议交互”，
 * 不是 `jump_to` 之类的眺层能力。
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

struct smbus_proto;

struct smbus_proto : public mc::proto::protocol {
    /// 与 `push` 上 `prepend(2)` 的 SMBus 头一致，供 `stack_spec::push_headroom` 聚合
    static constexpr std::size_t push_headroom = 2;

    struct packet {
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

    template <typename Req>
    static void ensure_buffer(Req& req)
    {
        req.prepare_buffer();
    }

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        auto& s      = req.template packet<smbus_proto>();
        auto& buffer = req.buffer();
        ensure_buffer(req);

        if (s.op_kind == packet::op::read_byte) {
            s.read_byte_substep = 0;
            buffer.prepend(2);
            std::memcpy(buffer.mutable_data(), "\xAA\x55", 2);
            buffer.write(&s.command, 1);
            s.xfer.slave_7bit    = s.slave_7bit;
            s.xfer.read_len      = 0;
            buffer.native_handle = &s.xfer;
            return req.push_next();
        }

        if (s.op_kind == packet::op::write_byte) {
            buffer.prepend(2);
            std::memcpy(buffer.mutable_data(), "\xAA\x55", 2);
            buffer.write(&s.command, 1);
            buffer.write(&s.write_value, 1);
            s.xfer.slave_7bit    = s.slave_7bit;
            s.xfer.read_len      = 0;
            buffer.native_handle = &s.xfer;
            return req.push_next();
        }

        if (s.op_kind == packet::op::block_read) {
            s.block_phase = 0;
            buffer.prepend(2);
            std::memcpy(buffer.mutable_data(), "\xAA\x55", 2);
            buffer.write(&s.command, 1);
            s.xfer.slave_7bit    = s.slave_7bit;
            s.xfer.read_len      = 0;
            buffer.native_handle = &s.xfer;
            return req.push_next();
        }

        return req.complete();
    }

    template <typename Req>
    static mc::proto::command on_pop(Req& req)
    {
        auto& s      = req.template packet<smbus_proto>();
        auto& buffer = req.buffer();

        if (s.op_kind == packet::op::read_byte) {
            if (s.read_byte_substep == 0) {
                s.read_byte_substep = 1;
                buffer.clear();
                s.xfer.slave_7bit    = s.slave_7bit;
                s.xfer.read_len      = 1;
                buffer.native_handle = &s.xfer;
                return req.push_next();
            }
            EXPECT_GE(buffer.length(), 1U);
            s.read_result        = buffer.data()[0];
            buffer.native_handle = nullptr;
            return req.complete();
        }

        if (s.op_kind == packet::op::write_byte) {
            buffer.native_handle = nullptr;
            return req.complete();
        }

        if (s.op_kind == packet::op::block_read) {
            if (s.block_phase == 0) {
                s.block_phase = 1;
                buffer.clear();
                s.xfer.slave_7bit    = s.slave_7bit;
                s.xfer.read_len      = 1;
                buffer.native_handle = &s.xfer;
                return req.push_next();
            }
            if (s.block_phase == 1) {
                EXPECT_GE(buffer.length(), 1U);
                s.block_count = buffer.data()[0];
                buffer.clear();
                s.block_phase        = 2;
                s.xfer.slave_7bit    = s.slave_7bit;
                s.xfer.read_len      = s.block_count;
                buffer.native_handle = &s.xfer;
                return req.push_next();
            }
            if (s.block_phase == 2) {
                EXPECT_EQ(buffer.length(), static_cast<std::size_t>(s.block_count));
                buffer.native_handle = nullptr;
                return req.complete();
            }
        }

        return req.complete();
    }
};

struct i2c_proto : public mc::proto::protocol {
    struct packet {
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

    static std::uint8_t pop_mock_byte(packet& bus)
    {
        if (bus.mock_read_cursor >= bus.mock_read_stream.size()) {
            ADD_FAILURE() << "mock_read_stream exhausted";
            return 0;
        }
        return bus.mock_read_stream[bus.mock_read_cursor++];
    }

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        auto& bus    = req.template packet<i2c_proto>();
        auto& buffer = req.buffer();
        auto* ctl    = static_cast<i2c_xfer_control*>(buffer.native_handle);
        if (ctl == nullptr) {
            return req.pop_next();
        }

        if (buffer.length() > 0) {
            typename packet::bus_op w;
            w.is_write  = true;
            w.addr_7bit = ctl->slave_7bit;
            w.bytes.assign(buffer.data(), buffer.data() + buffer.length());
            bus.trace.push_back(std::move(w));
        }

        if (ctl->read_len > 0) {
            buffer.clear();
            for (std::size_t i = 0; i < ctl->read_len; ++i) {
                const std::uint8_t b = pop_mock_byte(bus);
                buffer.write(&b, 1);
            }
            typename packet::bus_op r;
            r.is_write  = false;
            r.addr_7bit = ctl->slave_7bit;
            r.read_len  = static_cast<int>(ctl->read_len);
            bus.trace.push_back(std::move(r));
        }

        return req.pop_next();
    }
};

using smbus_i2c_proto = mc::proto::stack<smbus_proto, i2c_proto>;

static_assert(smbus_i2c_proto::spec_type::push_headroom == 2U, "SMBus 与 I2C 栈 push headroom 聚合");

} // namespace

TEST(protocol_smbus_i2c, smbus_read_byte_maps_to_i2c_write_then_read)
{
    auto proto = smbus_i2c_proto::create();
    ASSERT_TRUE(proto != nullptr);

    smbus_i2c_proto::request req;

    auto& smbus      = req.packet<smbus_proto>();
    smbus.op_kind    = smbus_proto::packet::op::read_byte;
    smbus.slave_7bit = 0x50;
    smbus.command    = 0x12;

    auto& i2c            = req.packet<i2c_proto>();
    i2c.mock_read_stream = {0xAB};
    i2c.mock_read_cursor = 0;

    auto st = proto->push(req);
    EXPECT_EQ(st, mc::proto::execution_state::completed);
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
    auto proto = smbus_i2c_proto::create();
    ASSERT_TRUE(proto != nullptr);

    auto req = smbus_i2c_proto::make_request();

    auto& smbus       = req.packet<smbus_proto>();
    smbus.op_kind     = smbus_proto::packet::op::write_byte;
    smbus.slave_7bit  = 0x50;
    smbus.command     = 0x00;
    smbus.write_value = 0x56;

    auto& i2c = req.packet<i2c_proto>();

    auto st = proto->push(req);
    EXPECT_EQ(st, mc::proto::execution_state::completed);

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
    auto proto = smbus_i2c_proto::create();
    ASSERT_TRUE(proto != nullptr);

    auto req = smbus_i2c_proto::make_request();

    auto& smbus      = req.packet<smbus_proto>();
    smbus.op_kind    = smbus_proto::packet::op::block_read;
    smbus.slave_7bit = 0x50;
    smbus.command    = 0xBD;

    auto& i2c            = req.packet<i2c_proto>();
    i2c.mock_read_stream = {3, 0x11, 0x22, 0x33};
    i2c.mock_read_cursor = 0;

    auto st = proto->push(req);
    EXPECT_EQ(st, mc::proto::execution_state::completed);

    const auto& pkt = req.buffer();
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
