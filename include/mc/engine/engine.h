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
class engine {
    using table_id_map = std::unordered_map<std::string_view, uint32_t>;

public:
    static engine& get_instance() {
        return mc::singleton<engine>::instance_with_creator([]() {
            return new engine();
        });
    }

    ~engine();

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
        if (!table || table->get_table_name().empty()) {
            return false;
        }

        m_database.register_table(table);
        return true;
    }

    // 注销对象表
    template <typename T>
    void unregister_object_table(std::shared_ptr<mc::db::table<T>> table) {
        if (!table || table->get_table_name().empty()) {
            return;
        }

        m_database.unregister_table(table->get_table_name());
    }

private:
    engine();

    mc::db::database m_database;
};

} // namespace engine
} // namespace mc

#endif // MC_ENGINE_ENGINE_H
