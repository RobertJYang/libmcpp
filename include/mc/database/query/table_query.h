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

/**
 * @file table_query.h
 * @brief 提供通用的表查询功能，支持索引优化
 */
#ifndef MC_DATABASE_QUERY_TABLE_QUERY_H
#define MC_DATABASE_QUERY_TABLE_QUERY_H

#include <functional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <mc/database/query/builder.h>
#include <mc/database/query/condition.h>
#include <mc/database/query/field_accessor.h>
#include <mc/database/query/metadata.h>
#include <mc/variant.h>

namespace mc::database::query {

/**
 * 表查询助手类
 *
 * 提供通用的表查询功能，支持索引优化
 *
 * @tparam TableType 表类型
 */
template <typename TableType>
class table_query {
public:
    using object_type     = typename TableType::object_type;
    using object_ptr_type = typename TableType::object_ptr_type;
    using object_id_type  = typename object_type::object_id_type;

    /**
     * 默认构造函数
     */
    table_query() : m_table(nullptr) {
    }

    /**
     * 构造函数
     *
     * @param table 表引用
     */
    explicit table_query(TableType& table) : m_table(&table) {
    }

    /**
     * 查询对象并收集所有匹配的结果
     *
     * @param builder 查询构建器
     * @return 匹配对象列表
     */
    std::vector<object_type> query_all(const query_builder& builder) {
        std::vector<object_type> result;

        // 执行查询，收集所有匹配对象
        query_impl(builder, [&result](const object_type& obj) {
            result.push_back(obj);
            return true;
        });

        return result;
    }

    /**
     * 查询对象，限制返回数量
     *
     * @param builder 查询构建器
     * @param limit 最大返回数量
     * @return 匹配对象列表
     */
    std::vector<object_type> query_limit(const query_builder& builder, size_t limit) {
        std::vector<object_type> result;

        // 执行查询，收集指定数量的匹配对象
        query_impl(builder, [&result, limit](const object_type& obj) {
            result.push_back(obj);
            return result.size() < limit;
        });

        return result;
    }

    /**
     * 查询单个对象
     *
     * @param builder 查询构建器
     * @return 匹配的第一个对象的可选包装
     */
    std::optional<object_type> query_one(const query_builder& builder) {
        auto results = query_limit(builder, 1);
        if (results.empty()) {
            return std::nullopt;
        }
        return results[0];
    }

    /**
     * 查询对象并收集所有匹配的ID
     *
     * @param builder 查询构建器
     * @return 匹配对象ID列表
     */
    std::vector<object_id_type> query_ids(const query_builder& builder) {
        std::vector<object_id_type> result;

        // 执行查询，只收集ID
        query_impl(builder, [&result](const object_type& obj) {
            result.push_back(obj.id());
            return true;
        });

        return result;
    }

    /**
     * 查询对象并使用自定义处理函数处理结果
     *
     * @param builder 查询构建器
     * @param handler 结果处理函数，返回false表示停止查询
     */
    template <typename Handler>
    void query(const query_builder& builder, Handler handler) {
        query_impl(builder, handler);
    }

private:
    /**
     * 索引类型探测辅助模板
     */
    template <typename T, typename = void>
    struct has_is_unique : std::false_type {};

    template <typename T>
    struct has_is_unique<T, std::void_t<decltype(std::declval<T>().is_unique())>> : std::true_type {
    };

    /**
     * 辅助方法：检查索引是否唯一
     */
    template <typename IndexType>
    static constexpr bool is_index_unique() {
        if constexpr (has_is_unique<IndexType>::value) {
            return IndexType::is_unique();
        } else {
            // 默认假设为唯一索引
            return true;
        }
    }

    /**
     * 构建表的索引元数据
     * @return 表索引元数据
     */
    static const query::table_index_metadata<object_type>& get_metadata() {
        static const auto metadata = query::build_table_metadata<TableType>();
        return metadata;
    }

    /**
     * 查找主键索引
     * @return 主键索引ID，通常是1（用户自定义的ID索引）
     */
    int find_primary_key_index() const {
        if (m_table == nullptr) {
            return -1;
        }

        // 获取表的元数据
        const auto& metadata = get_metadata();

        // 查找ID字段相关的索引
        for (const auto& name : {"rowid"}) {
            auto indices = metadata.find_indices_by_field(name);
            if (!indices.empty()) {
                // 优先选择非0的索引，因为0通常是object_id索引
                for (const auto* index : indices) {
                    if (index->index_id > 0 && index->is_unique) {
                        return static_cast<int>(index->index_id);
                    }
                }

                // 如果没有找到合适的，返回第一个非0的索引
                for (const auto* index : indices) {
                    if (index->index_id > 0) {
                        return static_cast<int>(index->index_id);
                    }
                }
            }
        }

        // 默认返回1（用户定义的ID索引）
        return 1;
    }

    /**
     * 利用元数据查找索引
     * @param field 字段名
     * @return 索引ID，如果没有匹配的索引则返回-1
     */
    int find_index_by_metadata(std::string_view field) const {
        if (m_table == nullptr) {
            return -1;
        }

        // 获取表的元数据
        const auto& metadata = get_metadata();

        // 查找匹配的索引
        auto indices = metadata.find_indices_by_field(field);
        if (indices.empty()) {
            return -1;
        }

        // 如果有多个索引，优先选择唯一索引
        for (const auto* index : indices) {
            if (index->is_unique && index->index_id > 0) {
                return static_cast<int>(index->index_id);
            }
        }

        // 如果没有唯一索引，则选择第一个ID大于0的索引
        for (const auto* index : indices) {
            if (index->index_id > 0) {
                return static_cast<int>(index->index_id);
            }
        }

        return -1;
    }

    /**
     * 查找可用的索引
     *
     * @param field 字段名
     * @return 索引ID，如果没有匹配的索引则返回-1
     */
    int find_index(std::string_view field) const {
        if (m_table == nullptr) {
            return -1;
        }

        // 尝试使用元数据查找索引
        int metadata_index = find_index_by_metadata(field);
        if (metadata_index >= 0) {
            return metadata_index;
        }

        // 如果元数据方式找不到，回退到硬编码方式
        // 预定义的字段和索引映射
        // 注意：默认object_id索引为0，用户自定义索引从1开始
        if (field == "id") {
            return 1; // 用户自定义的ID索引
        } else if (field == "name") {
            return 2; // 姓名索引
        } else if (field == "age") {
            return 3; // 年龄索引
        } else if (field == "city") {
            return 4; // 城市索引
        }

        return -1;
    }

    /**
     * 通用索引查询辅助方法
     * 模板参数允许为每种索引生成专门的代码
     */
    template <size_t IndexId, typename KeyType, typename Handler>
    bool query_by_specific_index(const mc::variant& value, Handler handler) {
        try {
            // 尝试转换值类型
            auto key_value = value.as<KeyType>();

            // 获取索引
            auto& index = m_table->template get<IndexId>();

            // 为age和city索引特殊处理，强制使用equal_range
            if constexpr (IndexId == 3 || IndexId == 4) {
                // 非唯一索引，使用equal_range
                auto range = index.equal_range(key_value);
                for (auto it = range.first; it != range.second; ++it) {
                    if (!handler(*it)) {
                        break;
                    }
                }
            } else {
                // 判断索引类型并选择查询方式
                if constexpr (is_index_unique<decltype(index)>()) {
                    // 唯一索引，使用find
                    auto it = index.find(key_value);
                    if (it != index.end()) {
                        handler(*it);
                    }
                } else {
                    // 非唯一索引，使用equal_range
                    auto range = index.equal_range(key_value);
                    for (auto it = range.first; it != range.second; ++it) {
                        if (!handler(*it)) {
                            break;
                        }
                    }
                }
            }
            return true;
        } catch (const mc::bad_cast_exception&) {
            return false;
        }
    }

    /**
     * 尝试使用索引查询
     *
     * @param field 字段名
     * @param op 比较操作符
     * @param value 比较值
     * @param handler 结果处理函数
     * @return 是否成功使用索引
     */
    template <typename Handler>
    bool query_by_index(std::string_view field, compare_op op, const mc::variant& value,
                        Handler handler) {
        if (m_table == nullptr) {
            return false;
        }

        // 只有等值查询才能使用索引
        if (op != compare_op::eq) {
            return false;
        }

        int index_id = find_index(field);
        if (index_id < 0) {
            return false;
        }

        // 使用辅助模板方法实现类型安全的索引查询
        switch (index_id) {
        case 1: // id索引
            return query_by_specific_index<1, object_id_type>(value, handler);
        case 2: // name索引
            return query_by_specific_index<2, std::string>(value, handler);
        case 3: // age索引
            return query_by_specific_index<3, int>(value, handler);
        case 4: // city索引
            return query_by_specific_index<4, std::string>(value, handler);
        default:
            return false;
        }
    }

    /**
     * 内部查询实现
     *
     * @param builder 查询构建器
     * @param handler, 结果处理函数
     */
    template <typename Handler>
    void query_impl(const query_builder& builder, Handler handler) {
        // 如果查询为空或表为空，直接返回
        if (builder.is_empty() || m_table == nullptr) {
            return;
        }

        // 尝试找到单个等值条件，用于索引查询
        const auto& conditions = builder.get_conditions();
        if (conditions.size() == 1) {
            const auto& condition_pair = conditions[0];
            const auto& op             = condition_pair.first;
            const auto& condition      = condition_pair.second;

            if (op == logical_op::AND && condition.get_op() == compare_op::eq) {
                const auto& field = condition.get_field();
                const auto& value = condition.get_value();

                // 尝试使用索引查询
                if (query_by_index(field, compare_op::eq, value, handler)) {
                    return;
                }
            }
        }

        // 如果无法使用索引，则回退到全表扫描
        full_table_scan(builder, handler);
    }

    /**
     * 全表扫描实现
     *
     * @param builder 查询构建器
     * @param handler 结果处理函数
     */
    template <typename Handler>
    void full_table_scan(const query_builder& builder, Handler handler) {
        if (m_table == nullptr) {
            return;
        }

        // 查找主键索引
        int primary_index_id = find_primary_key_index();

        // 由于template get<>需要编译期常量，使用switch处理不同索引ID
        switch (primary_index_id) {
        case 0:
            scan_by_index<0>(builder, handler);
            break;
        case 1:
            scan_by_index<1>(builder, handler);
            break;
        case 2:
            scan_by_index<2>(builder, handler);
            break;
        case 3:
            scan_by_index<3>(builder, handler);
            break;
        case 4:
            scan_by_index<4>(builder, handler);
            break;
        default:
            // 默认使用索引0（对象ID索引）
            scan_by_index<0>(builder, handler);
            break;
        }
    }

    /**
     * 按特定索引进行全表扫描
     *
     * @tparam IndexId 索引ID
     * @param builder 查询构建器
     * @param handler 结果处理函数
     */
    template <size_t IndexId, typename Handler>
    void scan_by_index(const query_builder& builder, Handler handler) {
        // 使用指定索引遍历所有对象
        auto& index = m_table->template get<IndexId>();

        for (auto it = index.begin(); it != index.end(); ++it) {
            const auto& obj = *it;

            // 使用字段访问器评估条件
            auto accessor = make_field_accessor(obj);

            if (builder.eval(accessor)) {
                // 符合条件，调用处理器
                if (!handler(obj)) {
                    // 处理器返回false，提前结束查询
                    break;
                }
            }
        }
    }

    TableType* m_table;
};

/**
 * 创建表查询助手的工厂函数
 *
 * @param table 表对象
 * @return 表查询助手对象
 */
template <typename TableType>
auto make_table_query(TableType& table) {
    return table_query<TableType>(table);
}

} // namespace mc::database::query

#endif // MC_DATABASE_QUERY_TABLE_QUERY_H