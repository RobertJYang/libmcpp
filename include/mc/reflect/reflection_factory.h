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

/**
 * @file reflection_factory.h
 * @brief 反射工厂类定义
 */
#ifndef MC_REFLECT_REFLECTION_FACTORY_H
#define MC_REFLECT_REFLECTION_FACTORY_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>

#include <mc/reflect/base.h>
#include <mc/reflect/reflection_enum_metadata.h>
#include <mc/reflect/reflection_metadata.h>
#include <mc/sync/mutex_box.h>

namespace mc::reflect {

/**
 * @brief 模块节点结构
 *
 * 用于构建模块树，支持层次化的类型组织
 */
struct module_node {
    using submodules_map = std::unordered_map<std::string_view, std::unique_ptr<module_node>>;
    using types_map      = std::unordered_map<std::string_view, int>;

    module_node()                              = default;
    module_node(const module_node&)            = delete;
    module_node& operator=(const module_node&) = delete;
    module_node(module_node&&)                 = default;
    module_node& operator=(module_node&&)      = default;

    submodules_map submodules; // 子模块
    types_map      types;      // 该模块下直接包含的类型ID
};

/**
 * @brief 反射工厂类
 *
 * 管理类型注册和反射对象创建，支持模块系统
 */
class reflection_factory {
public:
    /**
     * @brief 获取单例实例
     * @return reflection_factory& 工厂实例引用
     */
    static reflection_factory& instance() {
        return mc::singleton<reflection_factory>::instance_with_creator([]() {
            return new reflection_factory();
        });
    }

    ~reflection_factory();

    /**
     * @brief 注册类型
     * @tparam T 要注册的类型
     * @return int 分配的类型ID
     */
    template <typename T>
    int register_type() {
        auto type_id = register_type(reflector<T>::name(), []() {
            return reflection_metadata<T>::instance_ptr();
        });
        register_to_module_tree(reflector<T>::name(), type_id);
        return type_id;
    }

    /**
     * @brief 注册枚举类型（枚举类型不需要完整的反射元数据）
     * @tparam T 要注册的枚举类型
     * @return int 分配的类型ID
     */
    template <typename T>
    int register_enum_type() {
        static_assert(std::is_enum_v<T>, "T must be an enum type");

        auto type_id = register_type(reflector<T>::name(), []() -> reflection_metadata_ptr {
            return reflection_enum_metadata<T>::instance_ptr();
        });
        register_to_module_tree(reflector<T>::name(), type_id);
        return type_id;
    }

    /**
     * @brief 获取反射元数据
     * @param type_id 类型ID
     * @return reflection_metadata_ptr 反射元数据
     */
    reflection_metadata_ptr get_metadata(int type_id);

    /**
     * @brief 通过类型名获取反射元数据
     * @param type_name 类型名
     * @return reflection_metadata_ptr 反射元数据
     */
    reflection_metadata_ptr get_metadata(std::string_view type_name);

    /**
     * @brief 创建反射对象
     * @param type_id 类型ID
     * @return reflected_object_ptr 反射对象实例
     */
    reflected_object_ptr create_object(int type_id);

    /**
     * @brief 通过类型名创建反射对象
     * @param type_name 类型名
     * @return reflected_object_ptr 反射对象实例
     */
    reflected_object_ptr create_object(std::string_view type_name);

    /**
     * @brief 获取类型ID
     * @param type_name 类型名
     * @return int 类型ID，如果不存在返回-1
     */
    int get_type_id(std::string_view type_name) const;

    /**
     * @brief 获取所有已注册的类型名
     * @return std::vector<std::string_view> 类型名列表
     */
    std::vector<std::string_view> get_registered_types() const;

    /**
     * @brief 根据模块路径获取该模块下的所有类型
     * @param module_path 模块路径（如 "mc::devices" 或 "mc.devices"）
     * @return std::vector<std::pair<std::string, int>> 类型名和类型ID的列表
     */
    std::vector<std::pair<std::string, int>> get_module_types(std::string_view module_path) const;

    /**
     * @brief 获取所有模块路径
     * @return std::vector<std::string> 模块路径列表
     */
    std::vector<std::string> get_all_module_paths() const;

    /**
     * @brief 检查模块是否存在
     * @param module_path 模块路径
     * @return bool 模块是否存在
     */
    bool has_module(std::string_view module_path) const;

    void sort_searchers();

private:
    using metadata_creator = std::function<reflection_metadata_ptr()>;
    using type_ids_map     = std::unordered_map<std::string_view, int>;
    using metadata_map     = std::unordered_map<int, reflection_metadata_wptr>;
    using initializer_map  = std::unordered_map<int, metadata_creator>;

    struct data_t {
        type_ids_map    m_type_ids;
        metadata_map    m_metadata;
        initializer_map m_metadata_initializers;
        int             m_next_type_id{0};
        module_node     m_root_module;
    };

    reflection_factory() = default;
    int register_type(std::string_view type_name, metadata_creator&& creator);

    reflection_metadata_ptr get_metadata_inner(int type_id, data_t& data);

    /**
     * @brief 将类型注册到模块树中
     * @param full_name 完整类型名（如 "mc::devices::sensor_device"）
     * @param type_id 类型ID
     */
    void register_to_module_tree(std::string_view full_name, int type_id);

    /**
     * @brief 查找模块节点
     * @param path 模块路径
     * @param root 根节点
     * @return module_node* 找到的节点，不存在返回nullptr
     */
    module_node* find_module_node(std::string_view path, module_node& root) const;

    /**
     * @brief 收集所有模块路径（递归遍历模块树）
     * @param node 当前节点
     * @param current_path 当前路径
     * @param paths 路径列表（输出参数）
     */
    void collect_module_paths(const module_node&        node,
                              const std::string&        current_path,
                              std::vector<std::string>& paths) const;

    mc::mutex_box<data_t, std::mutex> m_data;
};

/**
 * @brief 全局便利函数：通过类型ID创建反射对象
 * @param type_id 类型ID
 * @return reflected_object_ptr 反射对象实例
 */
reflected_object_ptr create_object(int type_id);

/**
 * @brief 全局便利函数：通过类型名创建反射对象
 * @param type_name 类型名
 * @return reflected_object_ptr 反射对象实例
 */
reflected_object_ptr create_object(std::string_view type_name);

/**
 * @brief 获取已注册的所有类型名
 * @return std::vector<std::string> 类型名列表
 */
inline std::vector<std::string> get_registered_types() {
    auto                     types = reflection_factory::instance().get_registered_types();
    std::vector<std::string> result;
    result.reserve(types.size());
    for (auto type : types) {
        result.emplace_back(type);
    }
    return result;
}

/**
 * @brief 获取类型ID
 * @param type_name 类型名
 * @return int 类型ID，如果不存在返回-1
 */
inline int get_type_id(std::string_view type_name) {
    return reflection_factory::instance().get_type_id(type_name);
}

/**
 * @brief 包装现有对象为反射对象
 * @tparam T 对象类型
 * @param obj 对象实例
 * @return reflected_object_ptr 反射对象包装器
 */
template <typename T>
reflected_object_ptr wrap_object(std::shared_ptr<T> obj) {
    return std::make_shared<reflected_object_impl<T>>(obj);
}

/**
 * @brief 包装现有对象为反射对象（创建副本）
 * @tparam T 对象类型
 * @param obj 对象实例
 * @return reflected_object_ptr 反射对象包装器
 */
template <typename T>
reflected_object_ptr wrap_object(const T& obj) {
    auto copy = std::make_shared<T>(obj);
    return wrap_object(copy);
}

} // namespace mc::reflect

#endif // MC_REFLECT_REFLECTION_FACTORY_H