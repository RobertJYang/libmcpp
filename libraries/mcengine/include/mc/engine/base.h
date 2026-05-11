/**
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_ENGINE_BASE_H
#define MC_ENGINE_BASE_H

#include <mc/db/table.h>
#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
#include <mc/db/shm_storage_engine.h>
#include <mc/engine/shm_object.h>
#endif
#include <mc/array.h>
#include <mc/engine/call_stack.h>
#include <mc/engine/context.h>
#include <mc/engine/macro.h>
#include <mc/engine/metadata.h>
#include <mc/engine/signal_info.h>
#include <mc/engine/utils.h>
#include <mc/memory.h>
#include <mc/module.h>
#include <mc/object.h>
#include <mc/reflect.h>
#include <mc/reflect/signature_helper.h>
#include <mc/result.h>
#include <mc/string.h>
#include <mc/string_view.h>
#include <mc/time.h>
#include <mc/traits.h>
#include <mc/variant.h>

#include <map>
#include <tuple>
#include <type_traits>

namespace mc::engine {
namespace mdb            = mc::db;
using method_type_info   = mc::reflect::method_type_info;
using property_type_info = mc::reflect::property_type_info;

using core_object = mc::object;

class abstract_object;
class abstract_interface;
class property_base;
class service;
struct shm_object;

using property_changed_signal = mc::signal<void(const mc::variant&, const property_base&)>;

using object_call_stack = detail::call_stack<service, abstract_object>;

using invoke_result = mc::variant;

using object_identifier_t = std::tuple<uint8_t, mc::string, mc::string, mc::string>;

#define MC_REFLECT_FLAG_PROPERTY_TPL 0x01 // 接口的属性是 property<T> 类型的反射标记
#define MC_REFLECT_FLAG_INTERFACE    0x02 // 对象的属性是一个 interface 类型的反射标记
#define MC_REFLECT_FLAG_NOCACHE      0x04 // 属性不缓存到快速存储
#ifndef MC_REFLECT_FLAG_READONLY
#define MC_REFLECT_FLAG_READONLY 0x08 // 属性只读；Properties.Set 应拒绝写入
#endif
#define MC_REFLECT_FLAG_INVALIDATED 0x10 // PropertiesChanged 仅携带属性名

// 属性同步来源信息
struct property_sync_info : public mc::shared_base {
    mc::string                                                            source;
    mc::array<std::tuple<mc::string, mc::string, mc::string, mc::string>> properties;
};

using property_sync_info_ptr = mc::shared_ptr<property_sync_info>;

class MC_API property_base {
public:
    virtual mc::string_view get_name() const      = 0;
    virtual mc::string_view get_signature() const = 0;
    virtual uint32_t        get_access() const    = 0;
    virtual uint64_t        get_flags() const     = 0;

    virtual abstract_interface* get_interface() const                        = 0;
    virtual void                set_interface(abstract_interface* interface) = 0;
    virtual abstract_object*    get_object() const                           = 0;

    virtual mc::variant get_value(int options = 0) const = 0;

    void set_value(const mc::variant& value)
    {
        set_variant(value);
    }

    virtual property_changed_signal& property_changed() = 0;

    virtual mc::variant get_override_value() const                   = 0;
    virtual void        set_override_value(const mc::variant& value) = 0;

protected:
    virtual void set_variant(const mc::variant& value) = 0;
};

class MC_API abstract_object : public mc::object {
public:
    MC_REFLECTABLE("mc.engine.abstract_object");

    class managed_objects {
    public:
        using value_type     = std::pair<const mc::string, abstract_object*>;
        using iterator       = std::map<mc::string, abstract_object*>::iterator;
        using const_iterator = std::map<mc::string, abstract_object*>::const_iterator;

        bool empty() const noexcept
        {
            return m_items.empty();
        }

        std::size_t size() const noexcept
        {
            return m_items.size();
        }

        void clear() noexcept
        {
            m_items.clear();
        }

        iterator begin() noexcept
        {
            return m_items.begin();
        }

        const_iterator begin() const noexcept
        {
            return m_items.begin();
        }

        iterator end() noexcept
        {
            return m_items.end();
        }

        const_iterator end() const noexcept
        {
            return m_items.end();
        }

        iterator find(mc::string_view key)
        {
            return m_items.find(mc::string(key));
        }

        const_iterator find(mc::string_view key) const
        {
            return m_items.find(mc::string(key));
        }

        std::size_t count(mc::string_view key) const
        {
            return m_items.count(mc::string(key));
        }

        abstract_object*& operator[](mc::string_view key)
        {
            return m_items[mc::string(key)];
        }

        iterator erase(iterator it)
        {
            return m_items.erase(it);
        }

        std::size_t erase(mc::string_view key)
        {
            return m_items.erase(mc::string(key));
        }

    private:
        std::map<mc::string, abstract_object*> m_items;
    };
    using mc::object::connect;

    abstract_object(core_object* parent = nullptr) : mc::object(parent)
    {}

    virtual ~abstract_object() = default;

    virtual void     set_service(service* s) = 0;
    virtual service* get_service() const     = 0;

    virtual abstract_object*       get_owner() const                 = 0;
    virtual void                   set_owner(abstract_object* owner) = 0;
    virtual const managed_objects& get_managed_objects() const       = 0;

    virtual mc::string_view     get_object_name() const                                      = 0;
    virtual void                set_object_name(mc::string_view name)                        = 0;
    virtual mc::string_view     get_object_path() const                                      = 0;
    virtual void                set_object_path(mc::string_view path)                        = 0;
    virtual mc::string_view     get_position() const                                         = 0;
    virtual void                set_position(mc::string_view position)                       = 0;
    virtual mc::string_view     get_class_name() const                                       = 0;
    virtual bool                is_initializing() const
    {
        return false;
    }
    virtual object_identifier_t get_object_identifier() const                                = 0;
    virtual void                set_object_identifier(const object_identifier_t& identifier) = 0;

    virtual bool                has_interface(mc::string_view interface_name) const = 0;
    virtual abstract_interface* get_interface(mc::string_view interface_name) const = 0;

    virtual mc::variant get_property(mc::string_view property_name, mc::string_view interface_name = {},
                                     int options = 0) const                                                    = 0;
    virtual bool        has_property(mc::string_view property_name, mc::string_view interface_name = {}) const = 0;
    virtual mc::dict    get_all_properties(mc::string_view interface_name = {}, int options = 0) const         = 0;
    virtual bool        set_property(mc::string_view property_name, const mc::variant& value,
                                     mc::string_view interface_name = {})                                      = 0;

    virtual void                   set_property_ref_info(mc::string_view property_name, mc::string_view info,
                                                         mc::string_view interface_name = {})        = 0;
    virtual mc::string             get_property_ref_info(mc::string_view property_name,
                                                         mc::string_view interface_name = {}) const  = 0;
    virtual void                   set_property_sync_info(mc::string_view property_name, property_sync_info_ptr info,
                                                          mc::string_view interface_name = {})       = 0;
    virtual property_sync_info_ptr get_property_sync_info(mc::string_view property_name,
                                                          mc::string_view interface_name = {}) const = 0;

    virtual mc::connection_type connect(mc::string_view signal_name, slot_type slot,
                                        mc::string_view interface_name = {}) = 0;
    virtual mc::variant         emit(mc::string_view signal_name, const mc::variants& args,
                                     mc::string_view interface_name = {})    = 0;

    virtual bool                has_method(mc::string_view method_name, mc::string_view interface_name = {}) const = 0;
    virtual invoke_result       invoke(mc::string_view method_name, const mc::variants& args = {},
                                       mc::string_view interface_name = {})                                        = 0;
    virtual result<mc::variant> async_invoke(mc::string_view method_name, const mc::variants& args = {},
                                             mc::string_view interface_name = {})                                  = 0;

    virtual void                     notify_property_changed(const mc::variant& value, const property_base& prop) = 0;
    virtual property_changed_signal& property_changed()                                                           = 0;
    virtual void notify_property_update_shm(const mc::variant& value, const property_base& prop)                  = 0;
    virtual property_changed_signal& property_update_shm()                                                        = 0;

    mc::shared_ptr<abstract_object> shared_from_this()
    {
        return mc::shared_ptr<abstract_object>(this);
    }

    virtual const object_metadata& get_metadata() const = 0;

    // 获取当前调用的上下文对象
    context& get_context();

    // SHM 锚点：把 heap C++ 对象绑定到对应的 SHM POD（shm_object）
    //   - USE_SHM=ON：service::add 在 SHM 中分配 shm_object 后调用 set_shm_handle
    //     把指针固定到本对象；shm_storage_engine 通过 shm_handle_extractor_fn
    //     读取该指针写入索引；销毁路径由 service 负责释放 shm_object
    //   - USE_SHM=OFF：始终为 nullptr，不影响行为
    //   abstract_object 不持有 shm_object 的所有权，仅作为指针锚点
    shm_object* get_shm_handle() const noexcept
    {
        return m_shm_handle;
    }
    void set_shm_handle(shm_object* h) noexcept
    {
        m_shm_handle = h;
    }

protected:
    friend class object_impl;
    virtual void add_managed_object(abstract_object* obj)    = 0;
    virtual void remove_managed_object(abstract_object* obj) = 0;

private:
    shm_object* m_shm_handle{nullptr};
};

class MC_API abstract_interface : public mc::object {
public:
    using mc::object::connect;

    virtual ~abstract_interface() = default;

    virtual abstract_object* get_owner() const = 0;

    service* get_service() const;

    virtual mc::string_view get_interface_name() const = 0;

    // 属性相关接口
    virtual mc::variant get_property(mc::string_view property_name, int options = 0) const    = 0;
    virtual mc::dict    get_all_properties(int options = 0) const                             = 0;
    virtual bool        set_property(mc::string_view property_name, const mc::variant& value) = 0;

    // 方法相关接口
    virtual invoke_result       invoke(mc::string_view method_name, const mc::variants& args = {})       = 0;
    virtual result<mc::variant> async_invoke(mc::string_view method_name, const mc::variants& args = {}) = 0;

    // 信号相关接口
    virtual mc::variant              emit(mc::string_view signal_name, const mc::variants& args)                  = 0;
    virtual mc::connection_type      connect(mc::string_view signal_name, slot_type slot)                         = 0;
    virtual void                     notify_property_changed(const mc::variant& value, const property_base& prop) = 0;
    virtual property_changed_signal& property_changed()                                                           = 0;

    // 类型反射相关接口
    virtual const metadata_list&      get_metadata() const                                   = 0;
    virtual const signal_type_info*   get_signal_info(mc::string_view signal_name) const     = 0;
    virtual const method_type_info*   get_method_info(mc::string_view method_name) const     = 0;
    virtual const property_type_info* get_property_info(mc::string_view property_name) const = 0;
    virtual const property_type_info* get_property_info(const void* prop_addr) const         = 0;

    bool has_property(mc::string_view property_name) const
    {
        return get_property_info(property_name) != nullptr;
    }

    // 标记属性不缓存，proxy 必须通过消息接口读写
    void set_property_nocache(mc::string_view property_name)
    {
        auto* info = get_property_info(property_name);
        if (info != nullptr) {
            const_cast<property_type_info*>(info)->set_flags(MC_REFLECT_FLAG_NOCACHE);
        }
    }
    void set_property_invalidated(mc::string_view property_name)
    {
        auto* info = get_property_info(property_name);
        if (info != nullptr) {
            const_cast<property_type_info*>(info)->set_flags(MC_REFLECT_FLAG_INVALIDATED);
        }
    }
    bool has_method(mc::string_view method_name) const
    {
        return get_method_info(method_name) != nullptr;
    }
    bool has_signal(mc::string_view signal_name) const
    {
        return get_signal_info(signal_name) != nullptr;
    }

    // 获取当前调用的上下文对象
    context& get_context();
};

using object_ptr = mc::shared_ptr<abstract_object>;

} // namespace mc::engine

namespace mc::engine {
MC_FIELD_INDEX_TAG(by_path, "path");
MC_FIELD_INDEX_TAG(by_class_name, "class_name");
MC_FIELD_INDEX_TAG(by_object_name, "object_name");

using path_non_unique_index = mc::db::ordered_non_unique<&abstract_object::get_object_path, by_path::tag>;
using path_unique_index     = mc::db::ordered_unique<&abstract_object::get_object_path, by_path::tag>;
using class_name_index =
    mc::db::ordered_non_unique<&abstract_object::get_class_name, &abstract_object::get_position, by_class_name::tag>;
using object_name_index = mc::db::ordered_unique<&abstract_object::get_object_name, by_object_name::tag>;

// ----------------------------------------------------------------------------
// 默认 storage_engine 选择
//   - MCENGINE_USE_SHM=1（默认）：shm_storage_engine（跨进程崩溃安全）
//   - MCENGINE_USE_SHM=0        ：local_storage_engine（heap，进程内嵌入式特化）
// ----------------------------------------------------------------------------

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
template <typename IndexDef>
using engine_default = mc::db::shm_storage_engine<abstract_object, mc::engine::shm_object,
                                                  mc::db::detail::indices_count_v<IndexDef>, mc::db::shm_engine_alloc>;
using engine_alloc_t = mc::db::shm_engine_alloc;
#else
template <typename IndexDef>
using engine_default =
    mc::db::local_storage_engine<abstract_object, mc::db::detail::indices_count_v<IndexDef>, std::allocator<char>>;
using engine_alloc_t = std::allocator<char>;
#endif

using service_object_indices = mdb::indexed_by<path_unique_index, class_name_index, object_name_index>;
using object_indices         = mdb::indexed_by<path_non_unique_index, class_name_index, object_name_index>;

// 每个服务自己的对象表
using service_object_table_impl =
    mdb::table<abstract_object, service_object_indices, engine_alloc_t, engine_default<service_object_indices>>;

using object_table_impl = mdb::table<abstract_object, object_indices, engine_alloc_t, engine_default<object_indices>>;

// 服务对象表（每个服务一个）
class service_object_table : public service_object_table_impl {
public:
    service_object_table(mc::string_view service_name, const engine_alloc_t& alloc = engine_alloc_t())
        : service_object_table_impl(mc::string(service_name) + ".object_table", 0U, alloc)
    {}
};

// 全局对象表，与服务对象表的差别是 path 是非唯一索引
class object_table : public object_table_impl {
public:
    object_table(const engine_alloc_t& alloc = engine_alloc_t()) : object_table_impl("object_table", 0U, alloc)
    {}
};

} // namespace mc::engine

#endif // MC_ENGINE_BASE_H
