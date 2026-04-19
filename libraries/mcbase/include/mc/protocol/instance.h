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

#ifndef MC_PROTOCOL_INSTANCE_H
#define MC_PROTOCOL_INSTANCE_H

#include <mc/common.h>
#include <mc/exception.h>
#include <mc/memory.h>
#include <mc/protocol/common.h>
#include <mc/protocol/request.h>
#include <mc/protocol/runtime.h>

#include <tuple>
#include <typeinfo>

namespace mc::proto {

namespace detail {

class request_session : public mc::shared_base {
public:
    virtual ~request_session() = default;

    virtual mc::proto::packet&       packet() noexcept       = 0;
    virtual const mc::proto::packet& packet() const noexcept = 0;

    virtual request_session& prepare_packet() noexcept  = 0;
    virtual request_session& prepare_inbound() noexcept = 0;

    mc::proto::buffer& buffer() noexcept
    {
        return packet();
    }

    const mc::proto::buffer& buffer() const noexcept
    {
        return packet();
    }

    request_session& prepare_buffer() noexcept
    {
        return prepare_packet();
    }

    request_session& prepare_inbound_buffer() noexcept
    {
        return prepare_inbound();
    }

    virtual execution_state push()   = 0;
    virtual execution_state pop()    = 0;
    virtual execution_state resume() = 0;

    virtual execution_state       state() const noexcept = 0;
    virtual const protocol_error& error() const noexcept = 0;

protected:
    virtual void*       unsafe_request_state(const std::type_info& protocol_type) noexcept       = 0;
    virtual const void* unsafe_request_state(const std::type_info& protocol_type) const noexcept = 0;

private:
    friend struct session_access;
};

struct session_access {
    template <typename Protocol>
    static detail::protocol_request_t<Protocol>* try_get(request_session& session) noexcept
    {
        return static_cast<detail::protocol_request_t<Protocol>*>(session.unsafe_request_state(typeid(Protocol)));
    }

    template <typename Protocol>
    static const detail::protocol_request_t<Protocol>* try_get(const request_session& session) noexcept
    {
        return static_cast<const detail::protocol_request_t<Protocol>*>(session.unsafe_request_state(typeid(Protocol)));
    }
};

struct instance_access;

} // namespace detail

class MC_API instance : public mc::shared_base {
public:
    virtual ~instance() = default;

    void start()
    {
        start_protocols();
    }

    void stop()
    {
        stop_protocols();
    }

    [[nodiscard]] std::size_t layer_count() const noexcept
    {
        return stack_descriptor().layer_count;
    }

    template <typename... Protocols>
    [[nodiscard]] bool accepts() const noexcept
    {
        using spec_type = stack_spec<Protocols...>;
        return &stack_descriptor() == &detail::stack_descriptor_for<spec_type>();
    }

    template <typename... Protocols>
    [[nodiscard]] execution_state push(request_for_t<Protocols...>& req)
    {
        using spec_type = stack_spec<Protocols...>;
        ensure_compatible<spec_type>();
        auto* protocols = static_cast<typename runtime<spec_type>::protocol_tuple_type*>(protocol_storage());
        MC_ASSERT_THROW(protocols != nullptr, mc::invalid_arg_exception, "proto instance runtime storage is null");
        return runtime<spec_type>::push(req, *protocols);
    }

    template <typename... Protocols>
    [[nodiscard]] mc::future<void> push_async(request_for_t<Protocols...>& req)
    {
        using spec_type = stack_spec<Protocols...>;
        ensure_compatible<spec_type>();
        auto* protocols = static_cast<typename runtime<spec_type>::protocol_tuple_type*>(protocol_storage());
        MC_ASSERT_THROW(protocols != nullptr, mc::invalid_arg_exception, "proto instance runtime storage is null");
        return runtime<spec_type>::push_async(req, *protocols);
    }

    template <typename... Protocols>
    [[nodiscard]] execution_state pop(request_for_t<Protocols...>& req)
    {
        using spec_type = stack_spec<Protocols...>;
        ensure_compatible<spec_type>();
        auto* protocols = static_cast<typename runtime<spec_type>::protocol_tuple_type*>(protocol_storage());
        MC_ASSERT_THROW(protocols != nullptr, mc::invalid_arg_exception, "proto instance runtime storage is null");
        return runtime<spec_type>::pop(req, *protocols);
    }

    template <typename... Protocols>
    [[nodiscard]] mc::future<void> pop_async(request_for_t<Protocols...>& req)
    {
        using spec_type = stack_spec<Protocols...>;
        ensure_compatible<spec_type>();
        auto* protocols = static_cast<typename runtime<spec_type>::protocol_tuple_type*>(protocol_storage());
        MC_ASSERT_THROW(protocols != nullptr, mc::invalid_arg_exception, "proto instance runtime storage is null");
        return runtime<spec_type>::pop_async(req, *protocols);
    }

    template <typename... Protocols>
    [[nodiscard]] execution_state resume(request_for_t<Protocols...>& req)
    {
        using spec_type = stack_spec<Protocols...>;
        ensure_compatible<spec_type>();
        auto* protocols = static_cast<typename runtime<spec_type>::protocol_tuple_type*>(protocol_storage());
        MC_ASSERT_THROW(protocols != nullptr, mc::invalid_arg_exception, "proto instance runtime storage is null");
        return runtime<spec_type>::resume(req, *protocols);
    }

protected:
    virtual const detail::stack_descriptor&         stack_descriptor() const noexcept                             = 0;
    virtual void*                                   protocol_storage() noexcept                                   = 0;
    virtual const void*                             protocol_storage() const noexcept                             = 0;
    virtual mc::shared_ptr<detail::request_session> make_session()                                                = 0;
    virtual void*                                   unsafe_protocol(const std::type_info& protocol_type) noexcept = 0;
    virtual const void* unsafe_protocol(const std::type_info& protocol_type) const noexcept                       = 0;
    virtual void        start_protocols()
    {}
    virtual void stop_protocols()
    {}

private:
    template <typename Spec>
    void ensure_compatible() const
    {
        MC_ASSERT_THROW(&stack_descriptor() == &detail::stack_descriptor_for<Spec>(), mc::invalid_arg_exception,
                        "protocol request type does not match instance");
    }

    friend struct detail::instance_access;
};

namespace detail {

struct instance_access {
    static mc::shared_ptr<request_session> make_session(mc::proto::instance& instance)
    {
        return instance.make_session();
    }

    template <typename Protocol>
    static Protocol* try_get_protocol(mc::proto::instance& instance) noexcept
    {
        return static_cast<Protocol*>(instance.unsafe_protocol(typeid(Protocol)));
    }

    template <typename Protocol>
    static const Protocol* try_get_protocol(const mc::proto::instance& instance) noexcept
    {
        return static_cast<const Protocol*>(instance.unsafe_protocol(typeid(Protocol)));
    }
};

template <std::size_t Index = 0, typename ProtocolTuple>
inline void bind_protocol_chain(ProtocolTuple& protocols)
{
    if constexpr (Index < std::tuple_size_v<ProtocolTuple>) {
        auto& proto = std::get<Index>(protocols);
        proto.set_layer_index(Index);
        if constexpr (Index == 0) {
            proto.set_parent(nullptr);
        } else {
            proto.set_parent(&std::get<Index - 1>(protocols));
        }
        bind_protocol_chain<Index + 1>(protocols);
    }
}

template <std::size_t Index = 0, typename ProtocolTuple>
inline void start_protocol_chain(instance& owner, ProtocolTuple& protocols)
{
    if constexpr (Index < std::tuple_size_v<ProtocolTuple>) {
        std::get<Index>(protocols).on_start(owner);
        start_protocol_chain<Index + 1>(owner, protocols);
    }
}

template <typename ProtocolTuple, std::size_t... Indexes>
inline void stop_protocol_chain_impl(instance& owner, ProtocolTuple& protocols, std::index_sequence<Indexes...>)
{
    (std::get<sizeof...(Indexes) - 1 - Indexes>(protocols).on_stop(owner), ...);
}

template <typename ProtocolTuple>
inline void stop_protocol_chain(instance& owner, ProtocolTuple& protocols)
{
    stop_protocol_chain_impl(owner, protocols, std::make_index_sequence<std::tuple_size_v<ProtocolTuple>>{});
}

template <typename... Protocols>
class request_session_model final : public request_session {
public:
    using spec_type           = stack_spec<Protocols...>;
    using protocol_tuple_type = std::tuple<Protocols...>;

    explicit request_session_model(protocol_tuple_type* protocols) : m_protocols(protocols)
    {}

    mc::proto::packet& packet() noexcept override
    {
        return m_request.buffer();
    }

    const mc::proto::packet& packet() const noexcept override
    {
        return m_request.buffer();
    }

    request_session& prepare_packet() noexcept override
    {
        m_request.prepare_buffer();
        return *this;
    }

    request_session& prepare_inbound() noexcept override
    {
        m_request.prepare_inbound_buffer();
        return *this;
    }

    execution_state push() override
    {
        return runtime<spec_type>::push(m_request, *m_protocols);
    }

    execution_state pop() override
    {
        return runtime<spec_type>::pop(m_request, *m_protocols);
    }

    execution_state resume() override
    {
        return runtime<spec_type>::resume(m_request, *m_protocols);
    }

    execution_state state() const noexcept override
    {
        return m_request.state();
    }

    const protocol_error& error() const noexcept override
    {
        return m_request.error();
    }

protected:
    void* unsafe_request_state(const std::type_info& protocol_type) noexcept override
    {
        void* result = nullptr;
        ((typeid(Protocols) == protocol_type ? result = static_cast<void*>(&m_request.template packet<Protocols>())
                                             : result),
         ...);
        return result;
    }

    const void* unsafe_request_state(const std::type_info& protocol_type) const noexcept override
    {
        const void* result = nullptr;
        ((typeid(Protocols) == protocol_type
              ? result = static_cast<const void*>(&m_request.template packet<Protocols>())
              : result),
         ...);
        return result;
    }

private:
    protocol_tuple_type* m_protocols{nullptr};
    request<spec_type>   m_request;
};

template <typename... Protocols>
class instance_model final : public mc::proto::instance {
public:
    using spec_type           = stack_spec<Protocols...>;
    using protocol_tuple_type = std::tuple<Protocols...>;

    instance_model()
    {
        bind_protocol_chain(m_protocols);
    }

protected:
    const detail::stack_descriptor& stack_descriptor() const noexcept override
    {
        return detail::stack_descriptor_for<spec_type>();
    }

    void* protocol_storage() noexcept override
    {
        return &m_protocols;
    }

    const void* protocol_storage() const noexcept override
    {
        return &m_protocols;
    }

    mc::shared_ptr<detail::request_session> make_session() override
    {
        return mc::static_pointer_cast<detail::request_session>(
            mc::make_shared<request_session_model<Protocols...>>(&m_protocols));
    }

    void* unsafe_protocol(const std::type_info& protocol_type) noexcept override
    {
        void* result = nullptr;
        ((typeid(Protocols) == protocol_type ? result = static_cast<void*>(&std::get<Protocols>(m_protocols)) : result),
         ...);
        return result;
    }

    const void* unsafe_protocol(const std::type_info& protocol_type) const noexcept override
    {
        const void* result = nullptr;
        ((typeid(Protocols) == protocol_type ? result = static_cast<const void*>(&std::get<Protocols>(m_protocols))
                                             : result),
         ...);
        return result;
    }

    void start_protocols() override
    {
        start_protocol_chain(*this, m_protocols);
    }

    void stop_protocols() override
    {
        stop_protocol_chain(*this, m_protocols);
    }

private:
    protocol_tuple_type m_protocols{};
};
} // namespace detail

template <typename... Protocols>
mc::shared_ptr<instance> make_instance()
{
    static_assert(sizeof...(Protocols) > 0, "make_instance requires at least one protocol");
    return mc::static_pointer_cast<instance>(mc::make_shared<detail::instance_model<Protocols...>>());
}

template <typename... Protocols>
struct stack {
    static_assert(sizeof...(Protocols) > 0, "proto::stack requires at least one protocol");

    using spec_type    = stack_spec<Protocols...>;
    using request      = mc::proto::request<spec_type>;
    using instance_ptr = mc::shared_ptr<mc::proto::instance>;

    static instance_ptr create()
    {
        return mc::proto::make_instance<Protocols...>();
    }

    static request make_request()
    {
        return request{};
    }
};

} // namespace mc::proto

namespace mc {

using proto_ptr = mc::shared_ptr<mc::proto::instance>;

} // namespace mc

#endif // MC_PROTOCOL_INSTANCE_H
