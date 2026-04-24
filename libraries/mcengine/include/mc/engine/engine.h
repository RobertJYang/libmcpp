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
#include <mc/engine/match.h>
#include <mc/engine/message.h>
#include <mc/engine/path_template.h>
#include <mc/exception.h>
#include <mc/singleton.h>
#include <mc/string.h>
#include <mc/string_view.h>

#include <cstdint>
#include <unordered_map>

namespace mc::engine {
using table_ptr = mc::db::table_ptr;
class object_table;

class abstract_object;
class service;

class MC_API engine {
public:
    ~engine();

    static engine& get_instance();
    static void    reset_for_test();

    // fork 后复位：忘掉继承的 service registry / match table，由上层重新装填。
    // 不触发 SHM endpoint 状态变更，避免污染父进程。
    static void reset_after_fork();

    mc::db::database& get_database();

    template <typename Table>
    auto get_table(mc::string_view table_name)
    {
        mc::engine::table_ptr table = find_table(table_name);
        if (table) {
            return std::dynamic_pointer_cast<Table>(table);
        }

        auto new_table = std::make_shared<Table>(table_name);
        MC_ASSERT(register_table(new_table), "注册表 ${table_name} 失败", ("table_name", table_name));
        return new_table;
    }

    table_ptr find_table(mc::string_view table_name);
    bool      register_table(table_ptr table);
    void      unregister_table(table_ptr table);

    object_table& get_object_table();

    static void                       add_path_template_backend(path_template_backend_ptr backend);
    static path_template_backend_list get_path_template_backends();
    static void                       set_path_template_backends(path_template_backend_list backends);
    static bool resolve_path_template(mc::string_view path_pattern, const abstract_object& object, mc::string& path);

    static match::table_ptr get_match_table();
    static void             set_match_table(match::table_ptr table);

    static void                      add_filter_backend(match::filter_backend_ptr backend);
    static match::filter_backend_ptr find_filter_backend(std::uint32_t backend_type);

    static void route_inbound(const message& msg);

    static void     register_service(service* svc);
    static void     unregister_service(service* svc);
    static service* find_service(mc::string_view name);

private:
    engine();

    struct engine_impl;
    std::shared_ptr<engine_impl> m_impl;
};

} // namespace mc::engine

#endif // MC_ENGINE_ENGINE_H
