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

#ifndef MC_DATABASE_QUERY_BUILDER_H
#define MC_DATABASE_QUERY_BUILDER_H

#include <functional>
#include <vector>

#include <mc/database/query/condition.h>
#include <mc/dict.h>
#include <mc/variant.h>

namespace mc::database::query {

/**
 * 查询组合器，用于构建复杂查询条件
 */
class query_builder {
public:
    query_builder() = default;

    /**
     * 添加一个AND条件
     * @param cond 要添加的条件
     * @return 查询构建器自身，用于链式调用
     */
    query_builder& where(const condition& cond) {
        m_conditions.push_back({logical_op::AND, cond});
        return *this;
    }

    /**
     * 添加一个AND条件
     * @param field 字段名
     * @param op 比较操作符
     * @param value 比较值
     * @return 查询构建器自身，用于链式调用
     */
    query_builder& where(std::string_view field, compare_op op, const mc::variant& value) {
        return where(condition::make(field, op, value));
    }

    /**
     * 添加一个相等条件 (field == value)
     * @param field 字段名
     * @param value 比较值
     * @return 查询构建器自身，用于链式调用
     */
    query_builder& where_eq(std::string_view field, const mc::variant& value) {
        return where(condition::eq(field, value));
    }

    /**
     * 添加多个相等条件的AND组合
     * @param dict 包含字段-值对的字典
     * @return 查询构建器自身，用于链式调用
     */
    query_builder& where(const mc::dict& dict) {
        for (const auto& entry : dict) {
            where_eq(entry.key, entry.value);
        }
        return *this;
    }

    /**
     * 添加一个OR条件
     * @param cond 要添加的条件
     * @return 查询构建器自身，用于链式调用
     */
    query_builder& or_where(const condition& cond) {
        m_conditions.push_back({logical_op::OR, cond});
        return *this;
    }

    /**
     * 添加一个OR条件
     * @param field 字段名
     * @param op 比较操作符
     * @param value 比较值
     * @return 查询构建器自身，用于链式调用
     */
    query_builder& or_where(std::string_view field, compare_op op, const mc::variant& value) {
        return or_where(condition::make(field, op, value));
    }

    /**
     * 添加一个NOT条件
     * @param cond 要取反的条件
     * @return 查询构建器自身，用于链式调用
     */
    query_builder& not_where(const condition& cond) {
        m_conditions.push_back({logical_op::NOT, cond});
        return *this;
    }

    /**
     * 添加BETWEEN条件
     * @param field 字段名
     * @param min_val 最小值
     * @param max_val 最大值
     * @return 查询构建器自身，用于链式调用
     */
    query_builder& where_between(std::string_view field, const mc::variant& min_val,
                                 const mc::variant& max_val) {
        return where(condition::between(field, min_val, max_val));
    }

    /**
     * 添加IN条件
     * @param field 字段名
     * @param values 值列表
     * @return 查询构建器自身，用于链式调用
     */
    query_builder& where_in(std::string_view field, const std::vector<mc::variant>& values) {
        return where(condition::in(field, values));
    }

    /**
     * 添加LIKE条件
     * @param field 字段名
     * @param pattern 模式
     * @return 查询构建器自身，用于链式调用
     */
    query_builder& where_like(std::string_view field, const std::string& pattern) {
        return where(condition::like(field, pattern));
    }

    /**
     * 构建字典格式的查询
     *
     * 注意：目前只支持最简单的等值条件，复杂条件将在后续版本支持
     *
     * @return 包含查询条件的字典
     */
    mc::dict build_dict() const {
        mc::mutable_dict result;

        // 目前只能处理简单的等值条件
        for (const auto& [op, cond] : m_conditions) {
            if (op == logical_op::AND && cond.get_op() == compare_op::eq) {
                std::string field_name(cond.get_field());
                result[field_name] = cond.get_value();
            }
        }

        return result;
    }

    /**
     * 评估对象是否匹配查询条件
     * @param eval_fn 字段求值函数，接收字段名并返回对应值
     * @return 是否匹配
     */
    bool eval(std::function<mc::variant(std::string_view)> eval_fn) const {
        if (m_conditions.empty()) {
            return true; // 空条件匹配所有对象
        }

        bool result        = true; // 初始值为true，用于AND操作
        bool has_condition = false;

        for (const auto& [op, cond] : m_conditions) {
            has_condition    = true;
            auto field_value = eval_fn(cond.get_field());
            bool match       = cond.eval(field_value);

            switch (op) {
            case logical_op::AND:
                result = result && match;
                break;

            case logical_op::OR:
                result = result || match;
                break;

            case logical_op::NOT:
                result = result && !match;
                break;
            }

            // 短路求值：如果是AND操作并且已经失败，就不需要继续计算
            if (op == logical_op::AND && !result) {
                return false;
            }
        }

        return has_condition ? result : true;
    }

    /**
     * 获取所有条件
     * @return 条件列表
     */
    const std::vector<std::pair<logical_op, condition>>& get_conditions() const {
        return m_conditions;
    }

    /**
     * 获取所有条件（别名方法，与get_conditions功能相同）
     * @return 条件列表
     */
    const std::vector<std::pair<logical_op, condition>>& conditions() const {
        return m_conditions;
    }

    /**
     * 查询是否为空
     * @return 是否没有条件
     */
    bool is_empty() const {
        return m_conditions.empty();
    }

    /**
     * 清空所有条件
     */
    void clear() {
        m_conditions.clear();
    }

private:
    std::vector<std::pair<logical_op, condition>> m_conditions;
};

} // namespace mc::database::query

#endif // MC_DATABASE_QUERY_BUILDER_H