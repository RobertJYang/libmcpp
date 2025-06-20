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
#include <mc/exception.h>
#include <mc/expr.h>
#include <mc/singleton.h>
#include <unordered_map>

namespace mc::engine {
using table_ptr = mc::db::table_ptr;
class object_table;

class engine {
public:
    ~engine();

    static engine& get_instance();

    mc::db::database& get_database();

    template <typename Table>
    std::shared_ptr<Table> get_table(std::string_view table_name) {
        mc::engine::table_ptr table = find_table(table_name);
        if (table) {
            return std::dynamic_pointer_cast<Table>(table);
        }

        auto new_table = std::make_shared<Table>(table_name);
        MC_ASSERT(register_table(new_table), "注册表 ${table_name} 失败",
                  ("table_name", table_name));
        return new_table;
    }

    table_ptr find_table(std::string_view table_name);
    bool      register_table(table_ptr table);
    void      unregister_table(table_ptr table);

    object_table& get_object_table();

    mc::expr::engine&  get_expr_engine();
    mc::expr::node_ptr compile(std::string_view expr);

private:
    engine();

    struct engine_impl;
    std::shared_ptr<engine_impl> m_impl;
};

} // namespace mc::engine

#endif // MC_ENGINE_ENGINE_H
