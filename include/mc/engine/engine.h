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

#ifndef MC_ENGINE_ENGINE_H
#define MC_ENGINE_ENGINE_H

#include <mc/db/database.h>
#include <mc/engine/interface.h>
#include <mc/engine/object.h>
#include <mc/singleton.h>
#include <unordered_map>

namespace mc {
namespace engine {

// 对象引擎，负责管理所有对象和接口
class object_engine {
public:
    static object_engine& get_instance() {
        return mc::singleton<object_engine>::instance_with_creator([]() {
            return new object_engine();
        });
    }

    ~object_engine();

    // 初始化引擎
    void init();

    // 运行引擎
    void run();

    // 停止引擎
    void stop();

    // 获取数据库实例
    mc::db::database& get_database();

    // 注册对象表（每个对象类对应一个表）
    template <typename T>
    bool register_object_table(std::shared_ptr<mc::db::table<T>> table) {
        if (!table) {
            return false;
        }

        auto table_name = table->name();
        m_database.register_table(table);

        m_table_ids[std::string(table_name)] = m_next_table_id++;
        return true;
    }

    // 通过全局ID查找对象
    template <typename T>
    mc::im::ref_ptr<const T> find_object(object_id_type global_id) const {
        // 从全局ID解析表ID和对象ID
        uint32_t table_id  = static_cast<uint32_t>(global_id >> 32);
        uint32_t object_id = static_cast<uint32_t>(global_id & 0xFFFFFFFF);

        // 查找表名
        for (const auto& [name, id] : m_table_ids) {
            if (id == table_id) {
                // 在表中查找对象
                auto obj = find_object_in_table<T>(name, object_id);
                return obj;
            }
        }

        return nullptr;
    }

    // 在指定表中查找对象
    template <typename T>
    mc::im::ref_ptr<const T> find_object_in_table(std::string_view table_name,
                                                  uint32_t         object_id) const {
        auto field_id = T::field(&T::get_object_id);
        return m_database.find<T>(table_name, field_id == object_id);
    }

    // 添加对象到引擎
    template <typename T>
    mc::im::ref_ptr<const T> add_object(std::string_view table_name, const T& obj) {
        auto it = m_table_ids.find(std::string(table_name));
        if (it == m_table_ids.end()) {
            return nullptr; // 表不存在
        }

        // 生成全局唯一ID
        uint32_t       table_id  = it->second;
        uint32_t       object_id = obj.get_object_id();
        object_id_type global_id = (static_cast<object_id_type>(table_id) << 32) | object_id;

        // 将对象转换为字典并添加到数据库
        mc::variant var(obj);
        auto        added_obj = m_database.add<T>(table_name, var.as<mc::dict>());

        if (added_obj) {
            // 设置全局ID
            const_cast<T*>(added_obj.get())->set_global_id(global_id);
        }

        return added_obj;
    }

    // 移除对象
    template <typename T>
    bool remove_object(std::string_view table_name, uint32_t object_id) {
        auto field_id = T::field(&T::get_object_id);
        return m_database.remove(table_name, field_id == object_id);
    }

    // 调用对象方法
    mc::variant call_object_method(object_id_type global_id, std::string_view interface_name,
                                   std::string_view                method_name,
                                   const std::vector<mc::variant>& args);

    // 获取对象属性
    mc::variant get_object_property(object_id_type global_id, std::string_view interface_name,
                                    std::string_view property_name);

    // 设置对象属性
    bool set_object_property(object_id_type global_id, std::string_view interface_name,
                             std::string_view property_name, const mc::variant& value);

private:
    object_engine();

    mc::db::database                          m_database;
    std::unordered_map<std::string, uint32_t> m_table_ids;
    uint32_t                                  m_next_table_id;
};

} // namespace engine
} // namespace mc

#endif // MC_ENGINE_ENGINE_H
