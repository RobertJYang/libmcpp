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

#include <mc/shm/message_queue/mq_proto.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mc::shm {
namespace {

mc::string payload_from_buffer(const mc::proto::buffer& buffer)
{
    const auto base = buffer.payload_base();
    const auto size = buffer.length() >= base ? buffer.length() - base : 0U;
    if (size == 0) {
        return {};
    }
    return mc::string(reinterpret_cast<const char*>(buffer.data() + base), size);
}

mc::string_view payload_view_from_buffer(const mc::proto::buffer& buffer)
{
    const auto base = buffer.payload_base();
    const auto size = buffer.length() >= base ? buffer.length() - base : 0U;
    if (size == 0) {
        return {};
    }
    return mc::string_view(reinterpret_cast<const char*>(buffer.data() + base), size);
}

void replace_payload(mc::proto::buffer& buffer, mc::string_view payload)
{
    buffer.clear();
    if (!payload.empty()) {
        buffer.append_payload(payload.data(), payload.size());
    }
}

void pack_fragment_header(char* out, std::uint32_t msg_id, std::uint32_t src, std::uint16_t part)
{
    std::memcpy(out, &msg_id, sizeof(msg_id));
    std::memcpy(out + 4, &src, sizeof(src));
    std::memcpy(out + 8, &part, sizeof(part));
    out[10] = 0;
    out[11] = 0;
    out[12] = 0;
}

void unpack_fragment_header(const char* data, std::uint32_t& msg_id, std::uint32_t& src, std::uint16_t& part)
{
    std::memcpy(&msg_id, data, sizeof(msg_id));
    std::memcpy(&src, data + 4, sizeof(src));
    std::memcpy(&part, data + 8, sizeof(part));
}

std::uint32_t extract_instance_id(std::uint32_t src)
{
    return src >> 16U;
}

std::uint16_t extract_endpoint_id(std::uint32_t src)
{
    return static_cast<std::uint16_t>(src & 0xffffU);
}

std::uint32_t pack_src(std::uint32_t instance_id, std::uint16_t endpoint_id)
{
    return (instance_id << 16U) | endpoint_id;
}

struct assembly_key {
    std::uint32_t msg_id{0};
    std::uint32_t src{0};

    bool operator==(const assembly_key& other) const
    {
        return msg_id == other.msg_id && src == other.src;
    }
};

struct assembly_key_hash {
    std::size_t operator()(const assembly_key& key) const
    {
        return std::size_t(key.msg_id) * 131U + std::size_t(key.src);
    }
};

struct assembly_state {
    std::uint16_t           total_parts{0};
    std::vector<bool>       received;
    std::vector<mc::string> parts;
    std::size_t             received_count{0};
};

} // namespace

struct mq_proto_inbound_runtime {
    std::mutex                                                          mutex;
    std::unordered_map<assembly_key, assembly_state, assembly_key_hash> inflight;
};

namespace {

struct send_context : public mc::proto::proto_context {
    mc::string              original_payload;
    std::vector<mc::string> fragments;
    std::size_t             next_fragment_index{0};
};

bool source_is_live(std::uint32_t src, const mq_proto::source_liveness_probe& probe)
{
    if (!probe) {
        return true;
    }

    return probe(extract_endpoint_id(src), static_cast<std::uint64_t>(extract_instance_id(src)));
}

void purge_source_assemblies(std::unordered_map<assembly_key, assembly_state, assembly_key_hash>& inflight,
                             std::uint32_t                                                        src)
{
    for (auto it = inflight.begin(); it != inflight.end();) {
        if (it->first.src == src) {
            it = inflight.erase(it);
            continue;
        }
        ++it;
    }
}

void purge_stale_assemblies(std::unordered_map<assembly_key, assembly_state, assembly_key_hash>& inflight,
                            const mq_proto::source_liveness_probe&                               probe)
{
    if (!probe) {
        return;
    }

    for (auto it = inflight.begin(); it != inflight.end();) {
        if (!source_is_live(it->first.src, probe)) {
            it = inflight.erase(it);
            continue;
        }
        ++it;
    }
}

bool ingest_fragment(mc::string_view fragment, std::uint32_t instance_id,
                     std::unordered_map<assembly_key, assembly_state, assembly_key_hash>& inflight,
                     mc::proto::buffer&                                                   out_buffer)
{
    if (fragment.size() < mq_proto::fragment_header_size) {
        return false;
    }

    std::uint32_t msg_id = 0;
    std::uint32_t src    = 0;
    std::uint16_t part   = 0;
    unpack_fragment_header(fragment.data(), msg_id, src, part);
    if (extract_instance_id(src) != instance_id) {
        return false;
    }

    const auto   payload = fragment.substr(mq_proto::fragment_header_size);
    assembly_key key{msg_id, src};
    auto         it = inflight.find(key);
    if (it == inflight.end()) {
        if (part == 0) {
            return false;
        }

        assembly_state state;
        state.total_parts    = part;
        state.received_count = 1;
        state.received.resize(part, false);
        state.parts.resize(part);
        state.received[0] = true;
        state.parts[0].append(payload.data(), payload.size());
        it = inflight.emplace(key, std::move(state)).first;
    } else {
        auto& state = it->second;
        if (part == 0 || part >= state.total_parts || state.received[part]) {
            return false;
        }
        state.received[part] = true;
        state.parts[part].append(payload.data(), payload.size());
        ++state.received_count;
    }

    auto& state = it->second;
    if (state.received_count != state.total_parts) {
        return false;
    }

    out_buffer.clear();
    for (const auto& current : state.parts) {
        if (!current.empty()) {
            out_buffer.append_payload(current.data(), current.size());
        }
    }
    inflight.erase(it);
    return true;
}

struct fragment_build_result {
    bool                    ok{false};
    mc::string              error;
    std::vector<mc::string> fragments;
};

fragment_build_result build_fragments(mc::string_view payload, std::uint32_t msg_id, std::uint32_t src,
                                      std::size_t max_fragment_payload)
{
    if (max_fragment_payload == 0) {
        return {false, "fragment payload size is zero", {}};
    }

    const auto total_parts_raw =
        payload.empty() ? 1U : ((payload.size() + max_fragment_payload - 1U) / max_fragment_payload);
    if (total_parts_raw > std::numeric_limits<std::uint16_t>::max()) {
        return {false, "message too large for mq fragment header", {}};
    }

    const auto              total_parts = static_cast<std::uint16_t>(total_parts_raw);
    std::vector<mc::string> fragments;
    fragments.reserve(total_parts);

    std::size_t offset = 0;
    for (std::uint16_t index = 0; index < total_parts; ++index) {
        const auto chunk_size = payload.empty() ? 0U : std::min(max_fragment_payload, payload.size() - offset);
        auto       fragment   = mc::string(mq_proto::fragment_header_size + chunk_size, '\0');
        const auto part       = index == 0 ? total_parts : index;
        pack_fragment_header(fragment.data(), msg_id, src, part);
        if (chunk_size != 0) {
            std::memcpy(fragment.data() + mq_proto::fragment_header_size, payload.data() + offset, chunk_size);
            offset += chunk_size;
        }
        fragments.push_back(std::move(fragment));
    }

    return {true, {}, std::move(fragments)};
}

void reset_send_context(send_context& ctx)
{
    ctx.original_payload.clear();
    ctx.fragments.clear();
    ctx.next_fragment_index = 0;
}

} // namespace

void mq_proto::configure(std::uint32_t instance_id, std::uint16_t endpoint_id, std::size_t max_fragment_payload)
{
    m_instance_id          = instance_id;
    m_endpoint_id          = endpoint_id;
    m_max_fragment_payload = max_fragment_payload == 0 ? default_max_fragment_payload : max_fragment_payload;
    if (m_inbound_runtime == nullptr) {
        m_inbound_runtime = std::make_shared<mq_proto_inbound_runtime>();
    }
    std::lock_guard<std::mutex> lock(m_inbound_runtime->mutex);
    m_inbound_runtime->inflight.clear();
}

void mq_proto::set_source_liveness_probe(source_liveness_probe probe)
{
    m_source_liveness_probe = std::move(probe);
}

mc::proto::execution_state mq_proto::on_push(mc::proto::proto_request& req)
{
    if (!_is_configured()) {
        return fail(req, "mq_proto_not_configured", "mq proto 未配置");
    }

    auto& ctx = ensure_context<send_context>(req);
    if (ctx.fragments.empty()) {
        const auto payload = payload_from_buffer(req.buffer());
        auto       built = build_fragments(payload, m_next_msg_id.fetch_add(1, std::memory_order_relaxed),
                                           pack_src(m_instance_id, m_endpoint_id), m_max_fragment_payload);
        if (!built.ok) {
            return fail(req, "mq_proto", built.error);
        }
        ctx.original_payload    = payload;
        ctx.fragments           = std::move(built.fragments);
        ctx.next_fragment_index = 0;
    }

    while (ctx.next_fragment_index < ctx.fragments.size()) {
        replace_payload(req.buffer(), ctx.fragments[ctx.next_fragment_index]);
        const auto state = push_next(req);
        if (state == mc::proto::execution_state::completed) {
            ++ctx.next_fragment_index;
            continue;
        }
        if (state == mc::proto::execution_state::suspended) {
            // 发送侧 transport 可能在当前栈帧返回前被 space watcher 立刻恢复。
            // 若恢复路径已经把请求推进到 completed/failed，再次 suspend 会把最终状态
            // 覆盖回 suspended，导致调用方永远看不到完成态。
            if (req.state() != mc::proto::execution_state::suspended) {
                return req.state();
            }
            return suspend(req);
        }
        if (state == mc::proto::execution_state::failed) {
            reset_send_context(ctx);
            return state;
        }
        return state;
    }

    replace_payload(req.buffer(), ctx.original_payload);
    reset_send_context(ctx);
    return complete(req);
}

mc::proto::execution_state mq_proto::on_pop(mc::proto::proto_request& req)
{
    if (!_is_configured()) {
        return fail(req, "mq_proto_not_configured", "mq proto 未配置");
    }

    if (m_inbound_runtime == nullptr) {
        return fail(req, "mq_proto_runtime_not_ready", "mq proto 入站运行时未初始化");
    }

    const auto    fragment = payload_view_from_buffer(req.buffer());
    bool          ready    = false;
    bool          dropped  = false;
    std::uint32_t src      = 0;
    if (fragment.size() >= fragment_header_size) {
        std::uint32_t msg_id = 0;
        std::uint16_t part   = 0;
        unpack_fragment_header(fragment.data(), msg_id, src, part);
    }

    {
        std::lock_guard<std::mutex> lock(m_inbound_runtime->mutex);
        purge_stale_assemblies(m_inbound_runtime->inflight, m_source_liveness_probe);
        if (src != 0 && !source_is_live(src, m_source_liveness_probe)) {
            purge_source_assemblies(m_inbound_runtime->inflight, src);
            dropped = true;
        } else {
            ready = ingest_fragment(fragment, m_instance_id, m_inbound_runtime->inflight, req.buffer());
        }
    }
    if (dropped || !ready) {
        req.buffer().clear();
        return pull_next(req);
    }

    return pop_next(req);
}

bool mq_proto::_is_configured() const noexcept
{
    return m_instance_id != 0 || m_endpoint_id != 0;
}

} // namespace mc::shm
