/*
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

#include <mc/engine/errors/std_errors.h>
#include <mc/engine/object.h>
#include <mc/engine/internal/shm_binding.h>
#include <mc/engine/path.h>
#include <mc/engine/service.h>
#include "shm_property_sync.h"
#include <mc/engine/utils.h>
#include <mc/log/log.h>

namespace mc::engine {

namespace detail {

// 获取 property<T> 类型属性在接口中的地址
static property_base* to_property_base(const object_impl* self, const interface_item<property_type_info>& info)
{
    return info.template to_item_ptr<property_base>(self);
}

// 获取接口属性在对象中的地址
static interface_impl* get_interface(const object_impl* self, const property_type_info* iface)
{
    return static_cast<interface_impl*>(to_interface_ptr(self, iface));
}

template <typename ResultType>
ResultType do_invoke(object_impl* self, context& ctx, mc::string_view method_name, const mc::variants& args,
                     mc::string_view interface_name)
{
    // 方法查找顺序：标准接口方法 -> 对象方法 -> 接口方法

    // invoke 事件过滤优先于既有方法路由：事件过滤 -> 标准接口方法 -> 对象方法 -> 接口方法

    method_invoke_event invoke_event(ctx, method_name, args, interface_name, std::is_same_v<ResultType, async_result>);
    self->send_event(invoke_event);
    if (invoke_event.is_accepted()) {
        ctx.accept();
        if constexpr (std::is_same_v<ResultType, invoke_result>) {
            if (invoke_event.has_result()) {
                return invoke_event.result();
            }
            return {};
        } else {
            if (invoke_event.has_async_result()) {
                return invoke_event.take_async_result();
            }
            if (invoke_event.has_result()) {
                return async_result(invoke_event.result());
            }
            return async_result{};
        }
    }

    // 全局查找方法，如果找不到再尝试在指定的 interface 中查找
    const auto& metadata = self->get_metadata();
    auto        info     = metadata.get_method_info(method_name, {});
    if (info.item == nullptr) {
        // 全局查找找不到说明方法真的不存在，设置上下文方法为空，底层根据这个值返回合适的错误给调用方
        ctx.set_method(nullptr);
        return {};
    }

    // 找到了方法，还得再检查一下是否需要精确指定接口调用
    void* obj_ptr = self;
    if (info.interface != nullptr) {
        auto* interface = get_interface(self, info.interface);
        if (!interface_name.empty() && interface->get_interface_name() != interface_name) {
            // 需要精确调用指定接口的方法，但是全局找到的并不是这个接口，重新按指定接口查询
            info = metadata.get_method_info(method_name, interface_name);
            if (info.item == nullptr) {
                ctx.set_method(nullptr); // 指定接口不存在该方法
                return {};
            }
        }

        obj_ptr = interface;
    }

    // 路由到对象的方法，这已经是最后一级路由了，再不行只能抛出错误
    ctx.set_method(info.item);
    ctx.accept();
    if constexpr (std::is_same_v<ResultType, invoke_result>) {
        return info.item->invoke(obj_ptr, args);
    } else {
        return info.item->async_invoke(obj_ptr, args);
    }
}

template <typename ResultType>
ResultType invoke_impl(object_impl* self, mc::string_view method_name, const mc::variants& args,
                       mc::string_view interface_name)
{
    // 跟踪对象调用
    object_call_stack::context object_ctx{self->get_service(), *self};

    auto* ctx = context_stack::top_value();
    if (ctx && &ctx->get_object() == self && ctx->get_method_name() == method_name) {
        return do_invoke<ResultType>(self, *ctx, method_name, args, interface_name);
    }

    context tmp_ctx(*self->get_service(), *self);
    tmp_ctx.set_call_info(detail::variants_call{args, interface_name, method_name});
    context_stack::context call_ctx(self->get_service(), tmp_ctx);
    return do_invoke<ResultType>(self, tmp_ctx, method_name, args, interface_name);
}

} // namespace detail

object_impl::object_impl(core_object* parent) : abstract_object(parent)
{}

object_impl::~object_impl()
{
    m_object_path.clear();
    m_position.clear();
    m_managed_objects.clear();
    m_property_changed_signal.reset();
}

bool object_impl::init(const mc::dict& args)
{
    try {
        from_variant(args, *this);
        return true;
    } catch (const std::exception& e) {
        elog("init object ${class} failed: ${error}", ("class", get_class_name())("error", e.what()));
        return false;
    }
}

mc::string_view object_impl::get_object_path() const
{
    if (!m_object_path.empty()) {
        return m_object_path;
    }

    const_cast<object_impl*>(this)->set_object_path(service::resolve_object_path(get_path_pattern(), *this));
    return m_object_path;
}

const abstract_object::managed_objects& object_impl::get_managed_objects() const
{
    return m_managed_objects;
}

void object_impl::add_managed_object(abstract_object* obj)
{
    if (!obj || obj == this) {
        return;
    }

    auto old_owner = obj->get_owner();
    if (old_owner == this) {
        return;
    }

    if (old_owner != nullptr) {
        old_owner->remove_managed_object(obj);
    }

    // 检查当前的子节点中，是否有节点应该成为新对象的子节点
    auto path = obj->get_object_path();
    for (auto it = m_managed_objects.begin(); it != m_managed_objects.end();) {
        auto c_path = it->first;
        auto c_obj  = static_cast<object_impl*>(it->second);
        if (c_obj == obj) {
            it = m_managed_objects.erase(it);
            continue;
        }

        if (detail::path_starts_with(c_path, path)) {
            it = m_managed_objects.erase(it);
            c_obj->set_owner(obj);
        } else {
            ++it;
        }
    }

    m_managed_objects[path] = obj;
}

void object_impl::remove_managed_object(abstract_object* obj)
{
    if (obj->get_owner() != this) {
        return;
    }

    // 将 obj 从管理列表中移除
    auto it = m_managed_objects.find(obj->get_object_path());
    if (it != m_managed_objects.end()) {
        m_managed_objects.erase(it);
    }
}

void object_impl::notify_property_changed(const mc::variant& value, const property_base& prop)
{
    if (m_property_changed_signal) {
        (*m_property_changed_signal)(value, prop);
    }
}

void object_impl::on_event(mc::event& e)
{
    if (e.type() != property_changed_event_id) {
        return;
    }

    auto* property_event = dynamic_cast<property_changed_event*>(&e);
    if (property_event == nullptr) {
        return;
    }

    if (property_event->property().get_object() != this) {
        return;
    }

    notify_property_changed(property_event->value(), property_event->property());
}

property_changed_signal& object_impl::property_changed()
{
    if (m_property_changed_signal == nullptr) {
        m_property_changed_signal = std::make_unique<property_changed_signal>();
    }

    return *m_property_changed_signal;
}

void object_impl::notify_property_update_shm(const mc::variant& value, const property_base& prop)
{
    shm_sync_property(*this, value, prop);

    if (m_property_update_shm_signal) {
        (*m_property_update_shm_signal)(value, prop);
    }
}

property_changed_signal& object_impl::property_update_shm()
{
    if (m_property_update_shm_signal == nullptr) {
        m_property_update_shm_signal = std::make_unique<property_changed_signal>();
    }
    return *m_property_update_shm_signal;
}

abstract_object* object_impl::get_owner() const
{
    return m_owner;
}

void object_impl::set_owner(abstract_object* new_owner)
{
    if (m_owner == new_owner) {
        return;
    }

    auto* old_owner = m_owner;

    // 将当前对象从旧的 owner 中移除
    if (m_owner != nullptr) {
        m_owner->remove_managed_object(this);
        if (!new_owner) {
            // 如果没有新的 owner，那需要将当前对象的子对象添加到其 owner 的管理列表中
            auto& child_objects = get_managed_objects();
            while (!child_objects.empty()) {
                child_objects.begin()->second->set_owner(m_owner);
            }
            m_owner = nullptr;
            _sync_owner_to_shm(old_owner, /*new_owner=*/nullptr);
            return;
        }
    }

    // 将当前对象添加到新的 owner 中
    if (new_owner != nullptr) {
        new_owner->add_managed_object(this);
    }
    m_owner = new_owner;
    _sync_owner_to_shm(old_owner, new_owner);
}

void object_impl::_sync_owner_to_shm(abstract_object* old_owner,
                                     abstract_object* new_owner) noexcept
{
    shm_binding::sync_owner(*this, old_owner, new_owner);
}

/**
 * - 直接调用基类的 get_name()，它现在返回 string_view，已经是高效的
 * - 移除了对象级缓存，因为基类已经处理了缓存逻辑
 * - 保持了线程安全性
 */
mc::string_view object_impl::get_object_name() const
{
    return this->get_name();
}

/**
 * **并发安全性：**
 * - 使用基类的线程安全接口进行名称设置
 * - 名称唯一性检查现在由基类 core::object::set_name() 统一处理
 */
void object_impl::set_object_name(mc::string_view name)
{
    // 基类 set_name 不允许 register 后重复设置；走到这里时 m_shm_handle 必然为 null，
    // 后续 _ensure_shm_handle 会把当时的 name 一次性写入 SHM。
    this->set_name(name);
}

object_identifier_t object_impl::get_object_identifier() const
{
    return m_object_identifier;
}

void object_impl::set_object_identifier(const object_identifier_t& identifier)
{
    m_object_identifier = identifier;
}

void object_impl::set_object_path(mc::string_view path)
{
    // 去除尾部多余的 '/'
    while (path.size() > 1 && path.back() == '/') {
        path = path.substr(0, path.size() - 1);
    }

    if (!path.empty() && !mc::engine::path::is_valid(path)) {
        MC_THROW(mc::invalid_arg_exception, "invalid object path ${path}", ("path", path));
    }

    // 如果已经注册到服务中则不能修改路径
    MC_ASSERT_THROW(m_object_path.empty() || !get_service(), mc::invalid_arg_exception,
                    "object ”${path}“ is registered, please unregister first", ("path", m_object_path));

    m_object_path = path;
}

mc::string_view object_impl::get_position() const
{
    return m_position;
}

void object_impl::set_position(mc::string_view position)
{
    m_position = position;
    shm_binding::sync_position(*this, std::string_view(position.data(), position.size()));
}

void object_impl::set_service(service* s)
{
    m_service = s;
}

service* object_impl::get_service() const
{
    return m_service;
}

void object_impl::init_interface_object(const object_metadata& metadata)
{
    /* 初始化子类对象的属性（interface、property）
     *
     * 这个做法不符合 C++ 对象构造顺序，因为基类先于子类构造，这里强制转换成子类指针，
     * 并直接调用子类属性的方法，当基类构造函数返回后，子类又会重新构造子类属性，所以
     * 必须确保已经在这里设置的属性，不要在该属性的构造函数中覆盖掉。
     *
     * 基类构造函数 -> 设置子类属性的值 -> 子类构造函数 -> 子类属性构造
     *                    |                             |
     *                    v                             v
     *              设置子类属性值                 需要保证子类属性构造
     *                                          不要覆盖前面设置的值
     */

    metadata.visit_interfaces([&](const interface_metadata& iface) {
        detail::get_interface(this, iface.interface)->set_owner(this);
    });
}

mc::variant object_impl::get_property(mc::string_view property_name, mc::string_view interface_name,
                                      int options) const
{
    // TODO:: 属性目前没有实现对象重载接口属性，后续需要实现
    const auto& metadata = get_metadata();
    auto        info     = metadata.get_property_info(property_name, interface_name);
    if (info.item != nullptr) {
        if (info.item->has_flags(MC_REFLECT_FLAG_PROPERTY_TPL)) {
            return detail::to_property_base(this, info)->get_value(options);
        } else {
            return info.item->get_value(detail::get_interface(this, info.interface));
        }
    }

    return {};
}

bool object_impl::handle_override(property_base* prop, mc::string_view property_name, const mc::variant& new_value,
                                  mc::string_view interface_name)
{
    auto* ctx = context::get_current_context_ptr();
    if (!ctx) {
        return false;
    }
    auto override_value = prop->get_override_value();
    auto override_mode  = ctx->get_arg("OverrideMode");
    if (override_mode.is_null()) {
        if (override_value.is_null()) {
            return false;
        }
        // 已有Override值情况下，以非Override模式设置属性值，只更新后台原始值，不发属性变更信号
        // 这里设置上下文标记，后续流程的属性值设置视为Override模式设置，与lua框架保持一致
        ctx->set_arg("OverrideMode", "set");
        return false;
    }
    auto old_value = prop->get_value(0);
    if (override_mode == "set") {
        prop->set_override_value(new_value);
        return true;
    }
    if (override_mode == "unset") {
        if (override_value.is_null()) {
            return true;
        }
        prop->set_override_value({});
        ctx->set_arg("OverrideMode", mc::variant());
    }
    return true;
}

bool object_impl::set_property(mc::string_view property_name, const mc::variant& value,
                               mc::string_view interface_name)
{
    const auto& metadata = get_metadata();
    auto        info     = metadata.get_property_info(property_name, interface_name);
    if (info.item == nullptr) {
        return false;
    }
    if (info.item->has_flags(MC_REFLECT_FLAG_PROPERTY_TPL)) {
        property_base* prop = detail::to_property_base(this, info);
        if (handle_override(prop, property_name, value, interface_name)) {
            return true;
        }
        prop->set_value(value);
        auto override_value = prop->get_override_value();
        if (!override_value.is_null()) {
            // 如果存在Override值，设置属性值后使用Override值更新共享内存
            notify_property_update_shm(override_value, *prop);
        }
        return true;
    } else {
        info.item->set_value(detail::get_interface(this, info.interface), value);
        return true;
    }
}

mc::dict object_impl::get_all_properties(mc::string_view interface_name, int options) const
{
    const auto& metadata = get_metadata();
    if (interface_name.empty()) {
        mc::dict dict;
        to_variant(*this, dict, options);
        return dict;
    }

    auto* iface_info = metadata.get_interface_info(interface_name);
    if (iface_info == nullptr) {
        return {};
    }
    mc::dict dict = detail::get_interface(this, iface_info)->get_all_properties(options);
    return dict;
}

void object_impl::set_property_ref_info(mc::string_view property_name, mc::string_view info,
                                        mc::string_view interface_name)
{
    if (!m_properties_ref_info) {
        m_properties_ref_info = std::make_unique<object_optional_data<std::string>>();
    }
    m_properties_ref_info->set(interface_name, property_name, info);
}

mc::string object_impl::get_property_ref_info(mc::string_view property_name, mc::string_view interface_name) const
{
    if (!m_properties_ref_info) {
        return "";
    }
    return m_properties_ref_info->get(interface_name, property_name);
}

void object_impl::set_property_sync_info(mc::string_view property_name, property_sync_info_ptr info,
                                         mc::string_view interface_name)
{
    if (!m_properties_sync_info) {
        m_properties_sync_info = std::make_unique<object_optional_data<property_sync_info_ptr>>();
    }
    m_properties_sync_info->set(interface_name, property_name, info);
}

property_sync_info_ptr object_impl::get_property_sync_info(mc::string_view property_name,
                                                           mc::string_view interface_name) const
{
    if (!m_properties_sync_info) {
        return {};
    }
    return m_properties_sync_info->get(interface_name, property_name);
}

abstract_interface* object_impl::get_interface(mc::string_view interface_name) const noexcept
{
    const auto& metadata   = get_metadata();
    auto*       iface_info = metadata.get_interface_info(interface_name);
    if (iface_info == nullptr) {
        return nullptr;
    }

    return detail::get_interface(this, iface_info);
}

void object_impl::from_variant(const mc::dict& d, object_impl& obj)
{
    const auto& metadata = obj.get_metadata();
    metadata.visit_properties([&](interface_item<property_type_info> info) {
        if (!d.contains(info.item->name)) {
            return;
        }

        // 只处理对象级别的属性，接口属性在下面接口中处理
        if (info.item->has_flags(MC_REFLECT_FLAG_PROPERTY_TPL)) {
            // property<T> 类型属性
            detail::to_property_base(&obj, info)->set_value(d[info.item->name]);
        } else if (!info.item->has_flags(MC_REFLECT_FLAG_INTERFACE)) {
            // 普通属性
            auto* iface = detail::get_interface(&obj, info.interface);
            info.item->set_value(iface, d[info.item->name]);
        }
    });

    // 处理每个接口的属性
    metadata.visit_interfaces([&](const interface_metadata& iface) {
        // 先尝试从接口名称中查找
        auto it = d.find(iface.metadata->get_class_name());
        if (it != d.end()) {
            interface_impl::from_variant(it->value.as_dict(), *detail::get_interface(&obj, iface.interface));
            return;
        }

        // 再尝试从反射名字查找
        it = d.find(iface.interface->name);
        if (it != d.end()) {
            interface_impl::from_variant(it->value.as_dict(), *detail::get_interface(&obj, iface.interface));
            return;
        }
    });
}

void object_impl::to_variant(const object_impl& obj, mc::dict& dict, int options)
{
    const auto& metadata = obj.get_metadata();

    if (options & property_options::with_object_property) {
        // 先处理对象级别定义的属性
        // TODO:: 如果该属性遮蔽了接口的属性，则应该写到到被遮蔽的接口中而不是作为独立属性存在在对象中，
        // 因为对象实现这些属性的目的就是为了遮蔽接口属性，达到接口扩展的目的
        metadata.get_object_metadata().visit_properties([&](const property_type_info* property) {
            if (dict.contains(property->name)) {
                return mc::reflect::visit_status::VS_CONTINUE;
            }

            // 只处理对象级别的属性，接口属性在下面接口中处理
            if (property->has_flags(MC_REFLECT_FLAG_PROPERTY_TPL)) {
                // property<T> 类型属性
                dict[property->name] = detail::to_property_base(&obj, {nullptr, property})->get_value(options);
            } else if (!property->has_flags(MC_REFLECT_FLAG_INTERFACE)) {
                // 普通属性
                dict[property->name] = property->get_value(&obj);
            }

            return mc::reflect::visit_status::VS_CONTINUE;
        });
    }

    // 处理每个接口的属性
    metadata.visit_interfaces([&](const interface_metadata& iface) {
        mc::dict sub_dict;
        interface_impl::to_variant(*detail::get_interface(&obj, iface.interface), sub_dict, options);
        dict[iface.metadata->get_class_name()] = sub_dict;
    });
}

bool object_impl::has_property(mc::string_view property_name, mc::string_view interface_name) const
{
    const auto& metadata = get_metadata();
    auto        info     = metadata.get_property_info(property_name, interface_name);
    if (info.item != nullptr) {
        return true;
    }

    return false;
}

bool object_impl::has_interface(mc::string_view interface_name) const
{
    return get_metadata().get_interface_info(interface_name) != nullptr;
}

mc::connection_type object_impl::connect(mc::string_view signal_name, slot_type slot, mc::string_view interface_name)
{
    const auto& metadata = get_metadata();
    auto        info     = metadata.get_signal_info(signal_name, interface_name);
    if (info.item != nullptr) {
        return info.item->connect(detail::get_interface(this, info.interface), std::move(slot));
    }

    return {};
}

mc::variant object_impl::emit(mc::string_view signal_name, const mc::variants& args, mc::string_view interface_name)
{
    const auto& metadata = get_metadata();
    auto        info     = metadata.get_signal_info(signal_name, interface_name);
    if (info.item != nullptr) {
        return info.item->emit(detail::get_interface(this, info.interface), args);
    }

    return {};
}

invoke_result object_impl::invoke(mc::string_view method_name, const mc::variants& args,
                                  mc::string_view interface_name)
{
    return detail::invoke_impl<invoke_result>(this, method_name, args, interface_name);
}

result<mc::variant> object_impl::async_invoke(mc::string_view method_name, const mc::variants& args,
                                              mc::string_view interface_name)
{
    return detail::invoke_impl<result<mc::variant>>(this, method_name, args, interface_name);
}

bool object_impl::has_method(mc::string_view method_name, mc::string_view interface_name) const
{
    const auto& metadata = get_metadata();
    auto        info     = metadata.get_method_info(method_name, interface_name);
    return info.item != nullptr;
}

} // namespace mc::engine
