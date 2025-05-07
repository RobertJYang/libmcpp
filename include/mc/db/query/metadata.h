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

#ifndef MC_DATABASE_QUERY_METADATA_H
#define MC_DATABASE_QUERY_METADATA_H

#include <string>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include <mc/db/index.h>
#include <mc/db/key_extractor.h>
#include <mc/reflect/reflect_metadata.h>

namespace mc::db::query {

/**
 * 索引类型枚举
 */
enum class index_type {
    ordered_unique,    // 有序唯一索引
    ordered_non_unique // 有序非唯一索引
};

/**
 * 键提取器类型枚举
 */
enum class key_extractor_type {
    member_key,          // 成员变量键
    member_function_key, // 成员函数键
    functor_key,         // 函数对象键
    composite_key        // 复合键
};

/**
 * 索引元数据，描述一个索引的关键特性
 */
struct index_metadata {
    size_t                   index_id       = 0;                              // 索引ID
    index_type               type           = index_type::ordered_unique;     // 索引类型
    key_extractor_type       extractor_type = key_extractor_type::member_key; // 键提取类型
    std::vector<std::string> field_names; // 相关字段名称(对于复合键可能有多个)
    std::type_index          key_type  = std::type_index(typeid(void)); // 键类型的类型索引
    bool                     is_unique = true;                          // 是否唯一索引
};

/**
 * 表的索引元数据容器，管理一个表的所有索引元数据
 * @tparam ObjectType 表中存储的对象类型
 */
template <typename ObjectType>
class table_index_metadata {
public:
    using index_metadata_list = std::vector<std::unique_ptr<index_metadata>>;

    /**
     * 添加索引元数据
     * @param metadata 要添加的索引元数据
     */
    void add_index(const index_metadata& metadata) {
        auto& md = m_indices.emplace_back(std::make_unique<index_metadata>(metadata));

        for (const auto& field_names : md->field_names) {
            m_field_to_indices[field_names].push_back(m_indices.size() - 1);
        }
    }

    /**
     * 检查字段是否有非唯一索引
     * @param field_name 字段名
     * @return 是否有非唯一索引
     */
    bool has_non_unique_index(std::string_view field_name) const {
        auto it = m_field_to_indices.find(field_name);
        if (it == m_field_to_indices.end()) {
            return false;
        }

        for (size_t index_id : it->second) {
            if (!m_indices[index_id]->is_unique) {
                return true;
            }
        }
        return false;
    }

    /**
     * 查找字段的最佳索引ID
     * @param field_name 字段名
     * @param prefer_unique 是否优先选择唯一索引
     * @return 索引ID，如果没有找到则返回-1
     */
    int find_best_index_id(std::string_view field_name, bool prefer_unique = true) const {
        auto it = m_field_to_indices.find(field_name);
        if (it == m_field_to_indices.end()) {
            return -1;
        }

        // 优先查找唯一索引
        if (prefer_unique) {
            for (size_t index_id : it->second) {
                if (m_indices[index_id]->is_unique && m_indices[index_id]->index_id > 0) {
                    return static_cast<int>(m_indices[index_id]->index_id);
                }
            }
        }

        // 查找任意可用索引
        for (size_t index_id : it->second) {
            if (m_indices[index_id]->index_id > 0) {
                return static_cast<int>(m_indices[index_id]->index_id);
            }
        }

        return -1;
    }

    /**
     * 获取所有索引元数据
     * @return 索引元数据列表
     */
    const index_metadata_list& get_all_indices() const {
        return m_indices;
    }

    /**
     * 根据索引ID获取索引元数据
     * @param index_id 索引ID
     * @return 索引元数据指针，如果不存在则返回nullptr
     */
    const index_metadata* get_index_by_id(size_t index_id) const {
        if (index_id < m_indices.size()) {
            return m_indices[index_id].get();
        }
        return nullptr;
    }

private:
    using field_to_indices_map = std::unordered_map<std::string_view, std::vector<size_t>>;

    index_metadata_list  m_indices;          // 所有索引元数据
    field_to_indices_map m_field_to_indices; // 字段到索引的映射
};

namespace detail {

/**
 * 从索引定义元组中提取元数据
 * @tparam T 表类型
 * @tparam Indices 索引元组类型
 * @tparam I 索引序号
 * @return 索引元数据
 */
template <typename T, typename Indices, size_t I>
index_metadata extract_index_metadata_from_tuple() {
    // 静态断言确保T是反射类型
    static_assert(mc::reflect::is_reflectable<T>(), "T必须是反射类型(使用MC_REFLECT宏定义)");

    using current_index_type = std::tuple_element_t<I, Indices>;
    using extractor_t        = typename current_index_type::key_extractor_type;
    using tag_type           = typename current_index_type::tag_type;

    constexpr size_t index_id = I;

    // 判断索引是否唯一
    constexpr bool is_unique = current_index_type::is_unique;

    // 创建索引元数据
    index_metadata metadata;
    metadata.index_id  = index_id;
    metadata.type      = is_unique ? index_type::ordered_unique : index_type::ordered_non_unique;
    metadata.is_unique = is_unique;

    // 根据提取器类型设置元数据
    if constexpr (std::is_same_v<typename mc::db::detail::key_extractor_traits<extractor_t>::tag,
                                 mc::db::detail::tag_member>) {
        metadata.extractor_type = key_extractor_type::member_key;
    } else if constexpr (std::is_same_v<
                             typename mc::db::detail::key_extractor_traits<extractor_t>::tag,
                             mc::db::detail::tag_member_function>) {
        metadata.extractor_type = key_extractor_type::member_function_key;
    } else if constexpr (std::is_same_v<
                             typename mc::db::detail::key_extractor_traits<extractor_t>::tag,
                             mc::db::detail::tag_functor>) {
        metadata.extractor_type = key_extractor_type::functor_key;
    } else if constexpr (std::is_same_v<
                             typename mc::db::detail::key_extractor_traits<extractor_t>::tag,
                             mc::db::detail::tag_composite>) {
        metadata.extractor_type = key_extractor_type::composite_key;
    }

    // 获取字段名称
    if constexpr (is_field_tag_v<tag_type>) {
        metadata.field_names = tag_type::get_field_names();
    } else {
        metadata.field_names = extractor_t::get_field_names();
    }

    // 设置键类型
    if constexpr (std::is_same_v<typename mc::db::detail::key_extractor_traits<extractor_t>::tag,
                                 mc::db::detail::tag_composite>) {
        // 复合键类型较复杂，暂时使用void类型
        metadata.key_type = std::type_index(typeid(void));
    } else {
        using key_t       = typename mc::db::detail::key_extractor_traits<extractor_t>::key_type;
        metadata.key_type = std::type_index(typeid(key_t));
    }

    return metadata;
}

/**
 * 递归从索引元组中提取所有索引的元数据
 */
template <typename T, typename Indices, size_t I = 0, size_t N = std::tuple_size_v<Indices>>
void extract_all_indices_metadata(table_index_metadata<T>& metadata) {
    if constexpr (I < N) {
        auto index_meta = extract_index_metadata_from_tuple<T, Indices, I>();
        metadata.add_index(index_meta);
        extract_all_indices_metadata<T, Indices, I + 1, N>(metadata);
    }
}

} // namespace detail

/**
 * 从表类型构建索引元数据
 * @tparam TableType 表类型
 * @return 表索引元数据
 */
template <typename TableType>
auto build_table_metadata() {
    using object_type        = typename TableType::object_type;
    using indices_tuple_type = typename TableType::indices_tuple_type;

    auto result = table_index_metadata<object_type>();
    detail::extract_all_indices_metadata<object_type, indices_tuple_type>(result);
    return result;
}

} // namespace mc::db::query

#endif // MC_DATABASE_QUERY_METADATA_H