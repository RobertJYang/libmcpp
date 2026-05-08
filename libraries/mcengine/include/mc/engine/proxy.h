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

#ifndef MC_ENGINE_PROXY_H
#define MC_ENGINE_PROXY_H

#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include <mc/common.h>
#include <mc/engine/message.h>
#include <mc/memory.h>
#include <mc/pp.h>
#include <mc/string.h>
#include <mc/string_view.h>
#include <mc/time.h>
#include <mc/variant.h>

namespace mc::engine {

class service;
struct shm_object;

enum class proxy_route {
    auto_,    // 尊重属性标记：普通属性走缓存（SHM/内存），nocache 属性走消息
    no_cache, // 忽略属性标记，所有属性都通过消息（Properties.Get）读取
};

enum class proxy_message_route {
    auto_,
    mq,
    dbus,
};

enum class proxy_get_all_mode {
    complete,
    fast_available_only,
};

struct object_ref {
    mc::string service;
    mc::string path;

    std::optional<std::uint16_t> endpoint_id;
    std::optional<std::uint32_t> instance_id;
    std::optional<std::uint64_t> object_id;
    std::optional<std::uint64_t> service_epoch;
};

struct proxy_policy {
    proxy_route         route{proxy_route::auto_};
    proxy_message_route message_route{proxy_message_route::auto_};
    mc::milliseconds    timeout{5000};
    bool                allow_stale_read{false};
};

struct resolved_shm_object {
    shm_object*                  object{nullptr};
    std::optional<std::uint64_t> service_epoch;
};

// SHM 路径解析器：根据 object_ref 找到对应的 shm_object 与 owner service epoch
class MC_API proxy_shm_resolver : public mc::shared_base {
public:
    ~proxy_shm_resolver() override = default;

    virtual resolved_shm_object resolve(const object_ref& ref) = 0;
};

MC_API mc::shared_ptr<proxy_shm_resolver> make_default_proxy_shm_resolver();

class MC_API object_proxy {
public:
    explicit object_proxy(const service* svc, object_ref ref, proxy_policy policy = {},
                          mc::shared_ptr<proxy_shm_resolver> resolver = {});

    const object_ref&   ref() const noexcept;
    const proxy_policy& policy() const noexcept;

    mc::variant get_property(mc::string_view interface_name, mc::string_view property_name) const;
    mc::dict    get_all_properties(mc::string_view    interface_name,
                                   proxy_get_all_mode mode = proxy_get_all_mode::complete) const;
    void set_property(mc::string_view interface_name, mc::string_view property_name, const mc::variant& value) const;
    mc::variant invoke(mc::string_view interface_name, mc::string_view method_name, const mc::variants& args = {},
                       mc::string_view signature = {}) const;

    template <typename ProxyType>
    std::unique_ptr<ProxyType> as() const
    {
        auto proxy = std::make_unique<ProxyType>();
        proxy->bind_proxy({m_ref, m_svc, m_resolver, m_policy});
        return proxy;
    }

private:
    mc::variant call_message(mc::string_view interface_name, mc::string_view method_name, const mc::variants& args,
                             mc::string_view signature) const;
    message     make_method_call(mc::string_view interface_name, mc::string_view method_name, const mc::variants& args,
                                 mc::string_view signature) const;

    const service*                     m_svc{nullptr};
    object_ref                         m_ref;
    proxy_policy                       m_policy;
    mc::shared_ptr<proxy_shm_resolver> m_resolver;
};

struct object_proxy_seed {
    object_ref                         ref;
    const service*                     svc{nullptr};
    mc::shared_ptr<proxy_shm_resolver> resolver;
    proxy_policy                       policy;

    bool is_valid() const noexcept
    {
        return svc != nullptr;
    }
};

class MC_API interface_proxy_context {
public:
    interface_proxy_context() = default;

    void bind(const object_proxy_seed& seed, mc::string_view interface_name);

    bool is_bound() const noexcept;

    const object_ref&   ref() const noexcept;
    const proxy_policy& policy() const noexcept;
    mc::string_view     interface_name() const noexcept;

    mc::variant get_property(mc::string_view property_name) const;
    void        set_property(mc::string_view property_name, const mc::variant& value) const;
    mc::dict    get_all_properties(proxy_get_all_mode mode = proxy_get_all_mode::complete) const;
    mc::variant invoke(mc::string_view method_name, const mc::variants& args = {},
                       mc::string_view signature = {}) const;

private:
    void ensure_bound() const;

    bool                               m_bound{false};
    object_ref                         m_ref;
    mc::string                         m_iface_name;
    const service*                     m_svc{nullptr};
    mc::shared_ptr<proxy_shm_resolver> m_resolver;
    proxy_policy                       m_policy;
};

template <typename T>
class property_proxy {
public:
    using value_type = T;

    property_proxy(interface_proxy_context& ctx, mc::string_view name) noexcept : m_ctx(ctx), m_name(name)
    {}

    property_proxy(const property_proxy&)            = delete;
    property_proxy& operator=(const property_proxy&) = delete;
    property_proxy(property_proxy&&)                 = delete;
    property_proxy& operator=(property_proxy&&)      = delete;

    T get_value() const
    {
        return m_ctx.get_property(m_name).template as<T>();
    }

    void set_value(const T& value) const
    {
        m_ctx.set_property(m_name, mc::variant(value));
    }

    mc::string_view name() const noexcept
    {
        return m_name;
    }

    operator T() const
    {
        return get_value();
    }

    const property_proxy& operator=(const T& value) const
    {
        set_value(value);
        return *this;
    }

private:
    interface_proxy_context& m_ctx;
    mc::string_view          m_name;
};

template <typename DerivedT>
class interface_proxy {
public:
    interface_proxy() = default;

    interface_proxy(const interface_proxy&)            = delete;
    interface_proxy& operator=(const interface_proxy&) = delete;
    interface_proxy(interface_proxy&&)                 = delete;
    interface_proxy& operator=(interface_proxy&&)      = delete;

    void bind_proxy(const object_proxy_seed& seed)
    {
        m_ctx.bind(seed, DerivedT::interface_name);
    }

    bool is_bound() const noexcept
    {
        return m_ctx.is_bound();
    }

    interface_proxy_context& context() noexcept
    {
        return m_ctx;
    }
    const interface_proxy_context& context() const noexcept
    {
        return m_ctx;
    }

    interface_proxy_context m_ctx;

protected:
    template <typename T>
    property_proxy<T> property(mc::string_view name)
    {
        return property_proxy<T>(m_ctx, name);
    }

    template <typename R = void>
    R invoke(mc::string_view method_name, const mc::variants& args, mc::string_view signature = {}) const
    {
        if constexpr (std::is_void_v<R>) {
            (void)m_ctx.invoke(method_name, args, signature);
        } else {
            return m_ctx.invoke(method_name, args, signature).template as<R>();
        }
    }
};

// ---------------------------------------------------------------------------
// 简写宏：让 client header 长得像 IDL 描述
//
// 用法：
//   class ChipProxy : public mc::engine::interface_proxy<ChipProxy> {
//   public:
//       MC_INTERFACE_PROXY("bmc.dev.Chip");
//
//       MC_PROXY_PROP(uint32_t, Address);
//       MC_PROXY_PROP(uint8_t,  OffsetWidth);
//
//       MC_PROXY_METHOD(std::vector<uint8_t>, BitIORead,
//                       ((uint32_t, offset))((uint8_t, length))((uint32_t, mask)));
//       MC_PROXY_METHOD(std::vector<uint8_t>, BitIORead, "uyu",
//                       ((uint32_t, offset))((uint8_t, length))((uint32_t, mask))); // 可选显式签名
//
//       MC_PROXY_METHOD(int32_t, GetVersion);     // 无参版本
//       MC_PROXY_METHOD(int32_t, GetVersion, ""); // 无参 + 可选显式签名
//   };
// ---------------------------------------------------------------------------

#define MC_INTERFACE_PROXY(NAME) static constexpr mc::string_view interface_name = NAME

#define MC_PROXY_PROP(TYPE, NAME)                                                                                      \
    mc::engine::property_proxy<TYPE> NAME = this->template property<TYPE>(MC_PP_STRINGIZE(NAME))

#define MC_ENGINE_PROXY_PARAM_DECL(r, d, e) MC_PP_TUPLE_ELEM(0, e) MC_PP_TUPLE_ELEM(1, e)
#define MC_ENGINE_PROXY_PARAM_NAME(r, d, e) MC_PP_TUPLE_ELEM(1, e)
#define MC_ENGINE_PROXY_PACK_PARAM(r, d, e) d.emplace_back(mc::variant(MC_PP_TUPLE_ELEM(1, e)));

#define MC_ENGINE_PROXY_PARAMS_DECL(ARGS_SEQ)                                                                          \
    MC_PP_SEQ_ENUM(MC_PP_SEQ_TRANSFORM(MC_ENGINE_PROXY_PARAM_DECL, _, ARGS_SEQ))
#define MC_ENGINE_PROXY_PARAMS_NAMES(ARGS_SEQ)                                                                         \
    MC_PP_SEQ_ENUM(MC_PP_SEQ_TRANSFORM(MC_ENGINE_PROXY_PARAM_NAME, _, ARGS_SEQ))

#define MC_PROXY_METHOD_IMPL_EMPTY(R, NAME)                                                                            \
    R NAME() const                                                                                                     \
    {                                                                                                                  \
        return this->template invoke<R>(MC_PP_STRINGIZE(NAME), mc::variants{});                                        \
    }

#define MC_PROXY_METHOD_IMPL_EMPTY_SIG(R, NAME, SIGNATURE)                                                             \
    R NAME() const                                                                                                     \
    {                                                                                                                  \
        return this->template invoke<R>(MC_PP_STRINGIZE(NAME), mc::variants{}, SIGNATURE);                             \
    }

#define MC_PROXY_METHOD_IMPL_ARGS(R, NAME, ARGS_SEQ)                                                                   \
    R NAME(MC_ENGINE_PROXY_PARAMS_DECL(ARGS_SEQ)) const                                                                \
    {                                                                                                                  \
        mc::variants packed;                                                                                           \
        packed.reserve(MC_PP_SEQ_SIZE(ARGS_SEQ));                                                                      \
        MC_PP_SEQ_FOR_EACH(MC_ENGINE_PROXY_PACK_PARAM, packed, ARGS_SEQ)                                               \
        return this->template invoke<R>(MC_PP_STRINGIZE(NAME), packed);                                                \
    }

#define MC_PROXY_METHOD_IMPL_SIG_ARGS(R, NAME, SIGNATURE, ARGS_SEQ)                                                    \
    R NAME(MC_ENGINE_PROXY_PARAMS_DECL(ARGS_SEQ)) const                                                                \
    {                                                                                                                  \
        mc::variants packed;                                                                                           \
        packed.reserve(MC_PP_SEQ_SIZE(ARGS_SEQ));                                                                      \
        MC_PP_SEQ_FOR_EACH(MC_ENGINE_PROXY_PACK_PARAM, packed, ARGS_SEQ)                                               \
        return this->template invoke<R>(MC_PP_STRINGIZE(NAME), packed, SIGNATURE);                                     \
    }

#define MC_PROXY_METHOD_SELECT_2(R, NAME) MC_PROXY_METHOD_IMPL_EMPTY(R, NAME)

#define MC_PROXY_METHOD_SELECT_3(R, NAME, ARG)                                                                         \
    MC_PP_IIF(MC_PP_IS_BEGIN_PARENS(ARG), MC_PROXY_METHOD_IMPL_ARGS, MC_PROXY_METHOD_IMPL_EMPTY_SIG)(R, NAME, ARG)

#define MC_PROXY_METHOD_SELECT_4(R, NAME, SIGNATURE, ARGS_SEQ)                                                         \
    MC_PROXY_METHOD_IMPL_SIG_ARGS(R, NAME, SIGNATURE, ARGS_SEQ)

#define MC_PROXY_METHOD(R, NAME, ...)                                                                                  \
    MC_PP_CAT(MC_PROXY_METHOD_SELECT_, MC_PP_VARIADIC_SIZE(R, NAME, ##__VA_ARGS__))(R, NAME, ##__VA_ARGS__)

MC_API object_proxy_seed make_proxy_seed(mc::string_view path, proxy_policy policy = {});
MC_API object_proxy_seed make_proxy_seed(mc::string_view path, mc::string_view service_hint, proxy_policy policy = {});

template <typename ProxyT>
std::unique_ptr<ProxyT> make_proxy(mc::string_view path, proxy_policy policy = {})
{
    auto seed = make_proxy_seed(path, policy);
    if (!seed.is_valid()) {
        return nullptr;
    }
    auto proxy = std::make_unique<ProxyT>();
    proxy->bind_proxy(seed);
    return proxy;
}

template <typename ProxyT>
std::unique_ptr<ProxyT> make_proxy(mc::string_view path, mc::string_view service_hint, proxy_policy policy = {})
{
    auto seed = make_proxy_seed(path, service_hint, policy);
    if (!seed.is_valid()) {
        return nullptr;
    }
    auto proxy = std::make_unique<ProxyT>();
    proxy->bind_proxy(seed);
    return proxy;
}

template <typename ProxyT>
std::unique_ptr<ProxyT> make_remote_proxy(const service* svc, mc::string_view bus_name, mc::string_view path,
                                          proxy_policy policy = {})
{
    if (policy.route == proxy_route::auto_) {
        policy.route = proxy_route::no_cache;
    }
    object_proxy_seed seed;
    seed.ref.service = mc::string(bus_name);
    seed.ref.path    = mc::string(path);
    seed.svc         = svc;
    seed.policy      = policy;
    if (!seed.is_valid()) {
        return nullptr;
    }
    auto proxy = std::make_unique<ProxyT>();
    proxy->bind_proxy(seed);
    return proxy;
}

inline object_proxy make_remote_proxy(const service* svc, mc::string_view bus_name, mc::string_view path,
                                      proxy_policy policy = {})
{
    object_ref ref;
    ref.service = mc::string(bus_name);
    ref.path    = mc::string(path);
    if (policy.route == proxy_route::auto_) {
        policy.route = proxy_route::no_cache;
    }
    return object_proxy(svc, std::move(ref), policy);
}

} // namespace mc::engine

#endif // MC_ENGINE_PROXY_H
