/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_CORE_OBJECT_BASE_H
#define MC_CORE_OBJECT_BASE_H

#include <mc/common.h>
#include <mc/memory.h>
#include <mc/runtime.h> // 需要 any_executor

namespace mc::core {

using object_id_type = uint64_t;

/**
 * @brief 对象基类，提供对象ID管理和引用计数支持
 *
 * 这是所有对象的基类，提供基本的对象标识和生命周期管理功能。
 * 主要用于支持对象存储到数据库中的查询引擎。
 */
class MC_API object_base : public enable_shared_from_this<object_base> {
public:
    using executor_type = mc::any_executor;

    object_base();
    virtual ~object_base();

    object_base(const object_base& other);

    object_base(object_base&& other);

    object_base& operator=(object_base&& other);

    object_base& operator=(const object_base& other);

    /**
     * 获取对象ID
     * @return 对象ID
     */
    virtual object_id_type get_object_id() const;

    /**
     * 设置对象ID
     * @param id 对象ID
     */
    void set_object_id(object_id_type id);

    /**
     * 检查对象ID是否有效
     * @return 如果ID不为0则返回true
     */
    bool has_valid_id() const;

protected:
    object_id_type m_object_id{0};
};

} // namespace mc::core

#endif // MC_CORE_OBJECT_BASE_H
