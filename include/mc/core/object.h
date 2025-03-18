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

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace mc {
namespace core {

/**
 * @brief 对象基类，提供对象层次结构和生命周期管理
 *
 * object类是框架中所有对象的基类，提供以下功能：
 * - 对象命名
 * - 父子对象关系管理
 * - 对象树遍历
 */
class object {
public:
    /**
     * @brief 构造函数
     * @param name 对象名称
     * @param parent 父对象指针，默认为nullptr
     */
    explicit object(std::string name = "", object* parent = nullptr);

    /**
     * @brief 析构函数，会自动删除所有子对象
     */
    virtual ~object();

    /**
     * @brief 获取对象名称
     * @return 对象名称
     */
    const std::string& name() const {
        return m_name;
    }

    /**
     * @brief 设置对象名称
     * @param name 新的对象名称
     */
    void set_name(const std::string& name) {
        m_name = name;
    }

    /**
     * @brief 获取父对象
     * @return 父对象指针，如果没有父对象则返回nullptr
     */
    object* parent() const {
        return m_parent;
    }

    /**
     * @brief 设置父对象
     * @param parent 新的父对象指针
     *
     * 如果对象已经有父对象，则会先从原父对象的子对象列表中移除
     */
    void set_parent(object* parent);

    /**
     * @brief 获取子对象列表
     * @return 子对象指针列表
     */
    const std::vector<object*>& children() const {
        return m_children;
    }

    /**
     * @brief 查找子对象
     * @param name 子对象名称
     * @return 子对象指针，如果未找到则返回nullptr
     */
    object* find_child(const std::string& name) const;

    /**
     * @brief 查找子对象（递归）
     * @param name 子对象名称
     * @return 子对象指针，如果未找到则返回nullptr
     */
    object* find_child_recursive(const std::string& name) const;

    // 禁止拷贝构造和赋值操作
    object(const object&)            = delete;
    object& operator=(const object&) = delete;

private:
    std::string          m_name;     ///< 对象名称
    object*              m_parent;   ///< 父对象指针
    std::vector<object*> m_children; ///< 子对象列表

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
};

} // namespace core
} // namespace mc

#endif // MC_CORE_OBJECT_H 