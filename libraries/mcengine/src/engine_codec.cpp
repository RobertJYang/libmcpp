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

#include <mc/engine/engine_codec.h>

#include <utility>

namespace mc::engine {

engine_codec::engine_codec(message_decode_options options) noexcept
    : m_decode_options(std::move(options))
{}

mc::string engine_codec::encode(const message& msg) const
{
    return msg.to_bytes();
}

message engine_codec::decode(mc::string_view bytes) const
{
    return message::from_bytes(bytes, m_decode_options);
}

void engine_codec::set_decode_options(message_decode_options options) noexcept
{
    m_decode_options = std::move(options);
}

const message_decode_options& engine_codec::decode_options() const noexcept
{
    return m_decode_options;
}

} // namespace mc::engine
