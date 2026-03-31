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

#ifndef MC_DATABASE_DATABASE_H
#define MC_DATABASE_DATABASE_H

#include <mc/db/table.h>
#include <mc/dict.h>

namespace mc {
namespace db {

using table_ptr = std::shared_ptr<table_base>;

class MC_API database {
    using table_name_map = std::unordered_map<mc::string_view, table_ptr>;
    using table_id_map   = std::unordered_map<uint32_t, table_ptr>;

public:
    database();
    ~database();

    void      register_table(table_ptr table);
    void      unregister_table(mc::string_view table_name);
    bool      is_table_registered(mc::string_view table_name) const;
    table_ptr get_table(mc::string_view table_name) const;

    bool   empty(mc::string_view table_name) const;
    size_t size(mc::string_view table_name) const;
    void   clear(mc::string_view table_name);

    /**
     * 添加对象到工厂
     * @param table_name 表名
     * @param var 字典
     * @return 对象基类的指针
     */
    object_ptr add(mc::string_view table_name, const mc::dict& var, transaction* txn = nullptr);

    /**
     * 添加对象到工厂
     * @param table_name 表名
     * @param var 字典
     * @return 对象指针
     */
    template <typename T>
    mc::shared_ptr<T> add(mc::string_view table_name, const mc::dict& var, transaction* txn = nullptr)
    {
        return add(table_name, var, txn).template static_pointer_cast<T>();
    }

private:
    mutable std::mutex m_mutex;
    table_name_map     m_tables;
    table_id_map       m_table_ids;
    uint32_t           m_next_table_id = 0;
};

} // namespace db
} // namespace mc

#endif // MC_DATABASE_DATABASE_H
