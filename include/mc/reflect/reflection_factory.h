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
#include <mc/reflect/reflect_metadata.h>
#include <mc/sync/mutex_box.h>

namespace mc::reflect {

/**
 * @brief 反射工厂类
 *
 * 管理类型注册和反射对象创建
 */
class reflection_factory {
public:
    /**
     * @brief 获取单例实例
     * @return reflection_factory& 工厂实例引用
     */
    static reflection_factory& instance();

    ~reflection_factory();

    /**
     * @brief 注册类型
     * @tparam T 要注册的类型
     * @return int 分配的类型ID
     */
    template <typename T>
    int register_type() {
        return register_type(reflector<T>::name(), []() {
            return reflection_metadata<T>::instance_ptr();
        });
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
    };

    reflection_factory() = default;
    int register_type(std::string_view type_name, metadata_creator&& creator);

    reflection_metadata_ptr get_metadata_inner(int type_id, data_t& data);

    mc::mutex_box<data_t, std::mutex> m_data;
};

/**
 * @brief 通过类型名创建反射对象
 * @param type_name 类型名
 * @return reflected_object_ptr 反射对象实例
 */
inline reflected_object_ptr create_object(std::string_view type_name) {
    auto& factory  = reflection_factory::instance();
    auto  metadata = factory.get_metadata(type_name);
    if (!metadata) {
        MC_THROW(mc::bad_type_exception, "类型不存在: ${type_name}", ("type_name", type_name));
    }
    return metadata->create_object();
}

/**
 * @brief 通过类型ID创建反射对象
 * @param type_id 类型ID
 * @return reflected_object_ptr 反射对象实例
 */
inline reflected_object_ptr create_object(int type_id) {
    auto& factory  = reflection_factory::instance();
    auto  metadata = factory.get_metadata(type_id);
    if (!metadata) {
        MC_THROW(mc::bad_type_exception, "类型ID不存在: ${type_id}", ("type_id", type_id));
    }
    return metadata->create_object();
}

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