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

class database {
    using table_name_map = std::unordered_map<std::string_view, table_ptr>;
    using table_id_map   = std::unordered_map<uint32_t, table_ptr>;

public:
    database();
    ~database();

    void      register_table(table_ptr table);
    void      unregister_table(std::string_view table_name);
    bool      is_table_registered(std::string_view table_name) const;
    table_ptr get_table(std::string_view table_name) const;

    bool   empty(std::string_view table_name) const;
    size_t size(std::string_view table_name) const;
    void   clear(std::string_view table_name);

    /**
     * 添加对象到工厂
     * @param table_name 表名
     * @param var 字典
     * @return 对象基类的指针
     */
    const object_base* add(std::string_view table_name, const mc::dict& var,
                           transaction* txn = nullptr);

    /**
     * 添加对象到工厂
     * @param table_name 表名
     * @param var 字典
     * @return 对象指针
     */
    template <typename T>
    typename T::const_object_ptr_type add(std::string_view table_name, const mc::dict& var,
                                          transaction* txn = nullptr) {
        auto obj = add(table_name, var, txn);
        if (obj == nullptr) {
            return nullptr;
        }

        return mc::im::ref_ptr<T>(obj);
    }

    /**
     * 查询对象
     * @param table_name 表名
     * @param builder 查询构建器
     * @return 对象指针
     */
    template <typename T>
    typename T::const_object_ptr_type find(std::string_view     table_name,
                                           const query_builder& builder) {
        typename T::const_object_ptr_type result;
        query<T>(table_name, builder, [&result](typename T::const_object_ptr_type obj) {
            result = obj;
            return false;
        });
        return result;
    }

    /**
     * 查询对象
     * @param table_name 表名
     * @param builder 查询构建器
     * @param limit 限制返回的记录数量，0表示不限制
     * @return 对象指针数组
     */
    template <typename T>
    std::vector<typename T::const_object_ptr_type>
    query(std::string_view table_name, const query_builder& builder, int limit = 0) {
        std::vector<typename T::const_object_ptr_type> results;
        query<T>(table_name, builder, [&results, limit](typename T::const_object_ptr_type obj) {
            if (limit > 0 && results.size() >= limit) {
                return false;
            }
            results.emplace_back(std::move(obj));
            return true;
        });
        return results;
    }

    template <typename T>
    std::vector<typename T::const_object_ptr_type> all(std::string_view table_name) {
        return query<T>(table_name, query_builder());
    }

    /**
     * 查询对象
     * @param table_name 表名
     * @param builder 查询构建器
     * @param handler 处理函数
     * @return 是否查询成功
     */
    template <typename T>
    bool query(std::string_view table_name, const query_builder& builder,
               std::function<bool(typename T::const_object_ptr_type)>&& handler) {
        auto table = m_tables.find(table_name);
        if (table == m_tables.end()) {
            return false;
        }

        return table->second->query_object(
            builder,
            [handler = std::forward<std::function<bool(typename T::const_object_ptr_type)>>(
                 handler)](const object_base* obj) {
                return handler(mc::im::cast<const T>(obj));
            });
    }

    bool remove(std::string_view table_name, const query_builder& builder,
                transaction* txn = nullptr);

    bool update(std::string_view table_name, const query_builder& builder, const mc::dict& values,
                transaction* txn = nullptr);
    bool update(std::string_view table_name, const query_builder& builder,
                const std::map<std::string, variant>& values, transaction* txn = nullptr);

private:
    mutable std::mutex m_mutex;
    table_name_map     m_tables;
    table_id_map       m_table_ids;
    uint32_t           m_next_table_id = 0;
};

} // namespace db
} // namespace mc

#endif // MC_DATABASE_DATABASE_H
