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

#ifndef MC_PROTOCOL_CONTEXT_H
#define MC_PROTOCOL_CONTEXT_H

namespace mc::proto {

class protocol;

class proto_context {
public:
    virtual ~proto_context() = default;

    proto_context* prev() noexcept
    {
        return m_prev;
    }

    const proto_context* prev() const noexcept
    {
        return m_prev;
    }

    protocol* owner() noexcept
    {
        return m_owner;
    }

    const protocol* owner() const noexcept
    {
        return m_owner;
    }

private:
    friend class proto_request;

    proto_context* m_prev{nullptr};
    protocol*      m_owner{nullptr};
};

} // namespace mc::proto

#endif // MC_PROTOCOL_CONTEXT_H
