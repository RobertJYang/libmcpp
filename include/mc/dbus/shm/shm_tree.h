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
class MC_API shm_tree {
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
     * @param params [in] 方法调用参数
     * @return 返回方法调用结果，超时或失败返回nullopt
     */
    std::optional<mc::variants> timeout_call(mc::milliseconds timeout, const method_call_params& params);

    /**
     * @brief 指定发送者调用方法
     * @param timeout [in] 超时时间
     * @param sender [in] 发送者名称
     * @param params [in] 方法调用参数
     * @return 返回方法调用结果，超时或失败返回nullopt
     */
    static std::optional<mc::variants> timeout_call_with_sender(mc::milliseconds timeout, std::string_view sender, const method_call_params& params);

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
     * @brief 通过共享内存获取单个属性值（快速路径）
     * @param service_name [in] 服务名称
     * @param path [in] 对象路径
     * @param interface_name [in] 接口名称
     * @param property_name [in] 属性名称
     * @return 返回属性值，失败返回 std::nullopt
     */
    static std::optional<variant> call_shm_get_property(std::string_view service_name,
                                                        std::string_view path,
                                                        std::string_view interface_name,
                                                        std::string_view property_name);

    /**
     * @brief 通过共享内存获取接口下所有属性值
     * @param service_name [in] 服务名称
     * @param path [in] 对象路径
     * @param interface_name [in] 接口名称
     * @return 返回属性字典 {"prop1": value1, "prop2": value2, ...}，失败返回 std::nullopt
     */
    static std::optional<dict> call_shm_get_all_properties(std::string_view service_name,
                                                           std::string_view path,
                                                           std::string_view interface_name);

    /**
     * @brief 通过共享内存根据属性名数组获取属性值集合
     * @param service_name [in] 服务名称
     * @param path [in] 对象路径
     * @param interface_name [in] 接口名称
     * @param prop_names [in] 属性名数组
     * @return 返回数组，包含两个字典：
     *         - [0]: 成功获取的属性字典 {"prop1": value1, "prop2": value2, ...}
     *         - [1]: RPC 调用过程中的错误信息 {"prop3": "error_message", ...}
     *         失败时返回 std::nullopt
     */
    static std::optional<variants> call_shm_get_properties_by_names(
        std::string_view service_name, std::string_view path, std::string_view interface_name,
        const variants& prop_names);

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

    /**
     * @brief 根据路径和接口列表查询对象，返回服务名到接口列表的映射
     * @param path [in] 对象路径
     * @param interfaces [in] 接口名称数组；为空时查询所有接口
     * @return 返回字典：{"service_name": ["interface1", "interface2", ...]}，对象不存在时返回 std::nullopt
     */
    static std::optional<dict> get_mdb_object(std::string_view path, const variants& interfaces);

    /**
     * @brief 根据路径和深度查询子路径列表，支持分页和接口过滤
     * @param path [in] 起始对象路径
     * @param depth [in] 查询深度
     * @param interfaces [in] 接口名称数组；为空时不过滤接口
     * @param skip [in] 跳过的对象数量（分页用）
     * @param top [in] 返回的最大对象数量（分页用）；0 表示不限制
     * @param ignore_case [in] 是否忽略路径大小写
     * @return 返回路径数组，对象不存在时返回 std::nullopt
     */
    static std::optional<variants> get_mdb_sub_paths(std::string_view path, uint32_t depth,
                                                     const variants& interfaces, uint32_t skip = 0,
                                                     uint32_t top = 0, bool ignore_case = false);

    /**
     * @brief 检查路径是否有效（对象是否存在）
     * @param path [in] 对象路径
     * @param ignore_case [in] 是否忽略路径大小写
     * @return 路径有效返回 true，否则返回 false
     */
    static bool is_valid_mdb_path(std::string_view path, bool ignore_case);

    /**
     * @brief 查询指定接口的所有拥有者（服务名和路径）
     * @param interface_name [in] 接口名称
     * @return 返回数组：[[service_name, path], [service_name, path], ...]
     */
    static variants get_mdb_interface_owners(std::string_view interface_name);

    /**
     * @brief 根据 sender（unique_name）查询对应的 well-known 服务名
     * @param sender [in] 发送者唯一名称（unique_name）
     * @return 返回 well-known 服务名，未找到时返回空字符串
     */
    static std::string_view get_mdb_service_name(std::string_view sender);

    /**
     * @brief 根据接口名与属性过滤条件查询第一个匹配对象的路径和服务名
     * @param interface_name [in] 接口名
     * @param filter_json [in] 属性过滤条件的 JSON 字符串（key 为属性名，value 为期望值）
     * @param ignore_case [in] 是否忽略字符串大小写
     * @return 返回数组：[path, service_name]；未找到时返回 ["", ""]
     */
    static variants get_mdb_path(std::string_view interface_name, std::string_view filter_json,
                                 bool ignore_case);

    /**
     * @brief 根据接口名与属性过滤条件查询第一个匹配对象的路径和服务名（内部实现）
     * @param interface_name [in] 接口名
     * @param filter_dict [in] 属性过滤条件（key 为属性名，value 为期望值）
     * @param ignore_case [in] 是否忽略字符串大小写
     * @return 返回数组：[path, service_name]；未找到时返回 ["", ""]
     */
    static variants get_mdb_path_impl(std::string_view interface_name, const dict& filter_dict,
                                      bool ignore_case);

    /**
     * @brief 获取所有服务的 well-known 名称列表
     * @return 返回服务名称数组
     */
    static variants get_mdb_service_names();

    /**
     * @brief 查询指定服务拥有的 class_name 列表
     * @param service_name [in] 服务名称；为空表示查询所有服务的 class_name
     * @return 返回 class_name 数组（可能为空）
     */
    static variants get_mdb_classes(std::string_view service_name);

    /**
     * @brief 根据 class_name 查询对象名、服务名与路径的映射关系
     * @param class_name [in] 类名
     * @return 返回数组：[[obj_name, [[service_name, path], ...]], ...]
     */
    static variants get_mdb_object_list(std::string_view class_name);

    /**
     * @brief 根据对象名查询对象归属（服务名、路径）
     * @param object_name [in] 对象名
     * @return 返回数组：[[service_name, path], ...]
     */
    static variants get_mdb_object_owner(std::string_view object_name);

    /**
     * @brief 根据服务名与接口名称模式匹配查询对象列表
     * @param service_name [in] 服务名；为空表示匹配所有服务
     * @param iface_pattern [in] 接口名称正则表达式；为空表示匹配所有接口
     * @return 返回数组：[[obj_name, path, [iface...]], ...]
     */
    static variants get_mdb_matched_objects(std::string_view service_name,
                                            std::string_view iface_pattern);

    /**
     * @brief 根据路径/深度查询子对象的接口映射关系
     * @param path [in] 起始对象路径
     * @param depth [in] 查询深度
     * @param interfaces [in] 接口过滤列表（为空则记录所有接口）
     * @return 返回路径映射关系，失败返回nullopt
     */
    static std::optional<dict> get_mdb_sub_objects(std::string_view path, uint32_t depth,
                                                   const variants& interfaces);

    /**
     * @brief 查询父路径链上的对象接口映射关系
     * @param path [in] 起始对象路径
     * @param interfaces [in] 接口过滤列表（为空则记录所有接口）
     * @return 返回父路径映射关系，失败返回nullopt
     */
    static std::optional<dict> get_mdb_parent_objects(std::string_view path,
                                                      const variants&  interfaces);

    /**
     * @brief 查询资源协作接口信息
     * @param params [in] 方法调用参数
     * @return 返回资源协作接口信息，失败返回nullopt
     */
    static std::optional<mc::variants> get_mdb_info(const method_call_params& params);

private:
    std::string                                         m_service_name;
    std::string                                         m_unique_name;
    shm::object_tree*                                   m_tree;
    std::unordered_map<uint64_t, std::function<void()>> m_shm_slots;
};

/**
 * @brief 共享内存对象访问器
 */
struct MC_API shm_obj_visitor : mc::engine::metadata_visitor {
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