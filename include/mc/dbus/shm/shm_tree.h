/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MC_DBUS_SHM_SHM_TREE_H
#define MC_DBUS_SHM_SHM_TREE_H

#include <mc/dbus/shm/gvariant_convert.h>
#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/shm/local_msg.h>
#include <mc/engine/base.h>
#include <mc/time.h>

namespace mc::dbus {
constexpr std::string_view OBJECT_PROPERTIES_INTERFACE = "bmc.kepler.Object.Properties";

/**
 * @brief 共享内存对象树
 */
class shm_tree {
public:
    /**
     * @brief 构造函数
     * @param harbor_name [in] 港口名称
     * @param service_name [in] 服务名称
     * @param unique_name [in] 唯一名称
     */
    shm_tree(std::string_view harbor_name, std::string_view service_name,
             std::string_view unique_name);

    /**
     * @brief 注册对象
     * @param obj [in/out] 抽象对象
     */
    void register_object(mc::engine::abstract_object& obj);

    /**
     * @brief 注销对象
     * @param path [in] 对象路径
     */
    void unregister_object(std::string_view path);

    /**
     * @brief 超时调用方法
     * @param timeout [in] 超时时间
     * @param service_name [in] 服务名称
     * @param path [in] 对象路径
     * @param interface [in] 接口名称
     * @param method [in] 方法名称
     * @param signature [in] 方法签名
     * @param args [in] 方法参数
     * @return 返回方法调用结果，超时或失败返回nullopt
     */
    std::optional<mc::variants> timeout_call(mc::milliseconds timeout,
                                             std::string_view service_name, std::string_view path,
                                             std::string_view interface, std::string_view method,
                                             std::string_view signature, const variants& args);

    /**
     * @brief 指定调用者进行超时调用方法
     * @param timeout [in] 超时时间
     * @param sender [in] 调用者服务名称
     * @param service_name [in] 服务名称
     * @param path [in] 对象路径
     * @param interface [in] 接口名称
     * @param method [in] 方法名称
     * @param signature [in] 方法签名
     * @param args [in] 方法参数
     * @return 返回方法调用结果，超时或失败返回nullopt
     */
    static std::optional<mc::variants> timeout_call_with_sender(mc::milliseconds timeout, std::string_view sender,
                                                                std::string_view service_name, std::string_view path,
                                                                std::string_view interface, std::string_view method,
                                                                std::string_view signature, const variants& args);

    /**
     * @brief 获取属性
     * @param service_name [in] 服务名称
     * @param path [in] 对象路径
     * @param interface [in] 接口名称
     * @param property [in] 属性名称
     * @return 返回属性值
     */
    static variant get_property(std::string_view service_name, std::string_view path,
                                std::string_view interface, std::string_view property);

    /**
     * @brief 设置属性
     * @param service_name [in] 服务名称
     * @param path [in] 对象路径
     * @param interface [in] 接口名称
     * @param property [in] 属性名称
     * @param value [in] 属性值
     */
    static void set_property(std::string_view service_name, std::string_view path,
                             std::string_view interface, std::string_view property,
                             const variant& value);

    /**
     * @brief 设置属性内部实现
     * @param prop [in/out] 共享内存属性指针
     * @param value [in] 属性值
     */
    static void set_property_inner(shm::shared_ptr<shm::property> prop, const variant& value);

    /**
     * @brief 添加匹配规则
     * @param rule [in] 匹配规则
     * @param cb [in] 回调函数
     * @param id [in] 规则ID
     */
    void add_match(match_rule& rule, mc::dbus::match_cb_t&& cb, uint64_t id);

    /**
     * @brief 移除匹配规则
     * @param id [in] 规则ID
     */
    void remove_match(uint64_t id);

private:
    std::string                                         m_service_name;
    std::string                                         m_unique_name;
    shm::object_tree*                                   m_tree;
    std::unordered_map<uint64_t, std::function<void()>> m_shm_slots;
};

/**
 * @brief 共享内存对象访问器
 */
struct shm_obj_visitor : mc::engine::metadata_visitor {
    /**
     * @brief 构造函数
     * @param shm_obj [in/out] 共享内存对象
     * @param obj [in] 抽象对象
     */
    shm_obj_visitor(shm::object& shm_obj, const mc::engine::abstract_object& obj)
        : m_obj(obj), m_shm_ins(shm::shared_memory::get_instance()), m_shm_obj(shm_obj) {
    }

    /**
     * @brief 处理接口开始
     * @param iface [in] 接口元数据
     */
    void handle_interface_begin(const mc::engine::interface_metadata& iface) override {
        m_shm_intf = &m_shm_obj.register_interface(m_shm_ins, false, iface.metadata->get_class_name());
        if (iface.metadata->get_class_name() == OBJECT_PROPERTIES_INTERFACE) {
            m_shm_obj.add_named_object_view(m_shm_ins, OBJECT_PROPERTIES_INTERFACE);
        }
        m_iface_meta = &iface;
        m_iface      = mc::engine::to_interface_ptr(&m_obj, iface.interface);
    }

    /**
     * @brief 处理接口结束
     * @param iface [in] 接口元数据
     */
    void handle_interface_end(const mc::engine::interface_metadata& iface) override {
    }

    /**
     * @brief 处理属性
     * @param info [in] 属性类型信息
     */
    void handle(const mc::engine::property_type_info* info) override {
        shm::shared_ptr<shm::property> shm_prop = m_shm_intf->add_p(m_shm_ins, info->name, info->get_signature());
        shm_prop->set_read_privilege(0);
        shm_prop->set_write_privilege(0);
        shm_prop->set_flags(info->flags);
        if (m_iface_meta->metadata->get_class_name() == OBJECT_PROPERTIES_INTERFACE) {
            auto value = mc::engine::common_properties_interface::get_instance().get(info->name);
            shm_tree::set_property_inner(shm_prop, value);
        } else {
            auto value = m_iface->get_property(info->name, mc::engine::property_options::memory);
            shm_tree::set_property_inner(shm_prop, value);
        }
    }

    /**
     * @brief 处理方法
     * @param info [in] 方法类型信息
     */
    void handle(const mc::engine::method_type_info* info) override {
        shm::method& shm_method =
            m_shm_intf->add_m(m_shm_ins, info->name, info->get_args_signature(), info->get_result_signature());
        shm_method.set_privilege(0);
        shm_method.set_flags(info->flags);
    }

    /**
     * @brief 处理信号
     * @param info [in] 信号类型信息
     */
    void handle(const mc::engine::signal_type_info* info) override {
        shm::signal& shm_signal = m_shm_intf->add_s(m_shm_ins, info->name, info->get_args_signature());
        shm_signal.set_flags(info->flags);
    }

    const mc::engine::abstract_object&    m_obj;
    const mc::engine::abstract_interface* m_iface{nullptr};
    const mc::engine::interface_metadata* m_iface_meta{nullptr};
    shm::shared_memory&                   m_shm_ins;
    shm::object&                          m_shm_obj;
    shm::interface*                       m_shm_intf;
};
} // namespace mc::dbus

#endif // MC_DBUS_SHM_SHM_TREE_H