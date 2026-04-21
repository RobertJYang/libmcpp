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

#ifndef MC_PROTOCOL_PROTOCOL_H
#define MC_PROTOCOL_PROTOCOL_H

#include <mc/protocol/request.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace mc::proto {

class MC_API protocol {
public:
    virtual ~protocol();

    void add_child(protocol& child);

    protocol_list_view children() const noexcept;

    execution_state push(proto_request& req);

    execution_state pop(proto_request& req);

    execution_state resume(proto_request& req);

protected:
    virtual execution_state on_push(proto_request& req) = 0;
    virtual execution_state on_pop(proto_request& req)  = 0;

    execution_state push_next(proto_request& req);

    execution_state push_to(proto_request& req, protocol& child);

    execution_state pop_next(proto_request& req);

    execution_state pop_to(proto_request& req, protocol& parent);

    execution_state pull_next(proto_request& req);

    execution_state pull_from(proto_request& req, protocol& child);

    execution_state complete(proto_request& req);

    execution_state suspend(proto_request& req);

    execution_state fail(proto_request& req, mc::string_view name, mc::string_view message);

    template <typename Context, typename... Args>
    Context& ensure_context(proto_request& req, Args&&... args)
    {
        return req.ensure_context<Context>(this, std::forward<Args>(args)...);
    }

    template <typename Context>
    Context& require_context(proto_request& req)
    {
        return req.require_context<Context>(this);
    }

private:
    static bool _contains(const std::vector<protocol*>& items, const protocol& target);

    std::vector<protocol*> m_children;
    std::vector<protocol*> m_parents;
};

} // namespace mc::proto

#endif // MC_PROTOCOL_PROTOCOL_H
