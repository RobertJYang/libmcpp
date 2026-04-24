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

#ifndef MC_ENGINE_ENGINE_CODEC_H
#define MC_ENGINE_ENGINE_CODEC_H

#include <mc/common.h>
#include <mc/engine/message.h>
#include <mc/engine/message_codec.h>
#include <mc/string.h>
#include <mc/string_view.h>

namespace mc::engine {

class MC_API engine_codec {
public:
    engine_codec() = default;
    explicit engine_codec(message_decode_options options) noexcept;

    mc::string encode(const message& msg) const;
    message    decode(mc::string_view bytes) const;

    void                          set_decode_options(message_decode_options options) noexcept;
    const message_decode_options& decode_options() const noexcept;

private:
    message_decode_options m_decode_options;
};

} // namespace mc::engine

#endif // MC_ENGINE_ENGINE_CODEC_H
