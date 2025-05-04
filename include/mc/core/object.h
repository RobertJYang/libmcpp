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

#ifndef MC_CORE_OBJECT_H
#define MC_CORE_OBJECT_H

#include <mc/common.h>
#include <mc/db/object.h>

#include <memory>
#include <string_view>
#include <vector>

namespace mc::core {
class object;
using object_ptr  = mc::im::ref_ptr<object>;
using child_list  = std::vector<object_ptr>;
using object_base = mc::db::object_base;

/**
 * @brief 对象基类，提供对象层次结构和生命周期管理
 */
class object : virtual public object_base, public mc::noncopyable {
public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit object(object* parent = nullptr);

    /**
     * @brief 析构函数，会自动删除所有子对象
     */
    virtual ~object() noexcept;

    /**
     * @brief 移动构造函数
     * @param other 右值对象
     */
    object(object&& other) noexcept;

    /**
     * @brief 移动赋值运算符
     * @param other 右值对象
     */
    object& operator=(object&& other) noexcept;

    /**
     * @brief 移动赋值运算符
     * @param other 右值对象
     */
    /**
     * @brief 获取对象名称
     * @return 对象名称
     */
    std::string_view get_name() const;

    /**
     * @brief 设置对象名称
     * @param name 新的对象名称
     */
    void set_name(std::string_view name);

    /**
     * @brief 获取父对象
     * @return 父对象指针
     */
    object* parent() const;

    /**
     * @brief 设置父对象
     * @param parent 新的父对象指针
     *
     * 如果对象已经有父对象，则会先从原父对象的子对象列表中移除
     */
    void set_parent(object* parent);

    /**
     * @brief 获取子对象列表
     * @return 子对象指针列表的副本
     */
    const child_list& children() const;

    /**
     * @brief 查找子对象
     * @param name 子对象名称
     * @return 子对象指针，如果未找到则返回nullptr
     */
    object* find_child(std::string_view name) const;

protected:
    /**
     * @brief 添加子对象
     * @param child 子对象指针
     */
    void add_child(object* child);

    /**
     * @brief 移除子对象
     * @param child 子对象指针
     */
    void remove_child(object* child);

private:
    struct object_impl;
    std::unique_ptr<object_impl> m_impl;
};

} // namespace mc::core

#endif // MC_CORE_OBJECT_H