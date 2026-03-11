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

/**
 * @file operation_base.h
 * @brief 通用操作基类定义
 *
 * 提供统一的操作节点基类，包含：
 * - next 指针（用于链表）
 * - 函数指针（执行函数）
 * - 销毁函数指针（类型安全的资源释放）
 */

#ifndef MC_RUNTIME_OPERATION_BASE_H
#define MC_RUNTIME_OPERATION_BASE_H

#include <functional>

namespace mc::runtime {

/**
 * @brief 通用操作基类
 *
 * 提供统一的操作节点接口，包含：
 * - next 指针（用于链表）
 * - 函数指针（执行函数）
 * - 销毁函数指针（类型安全的资源释放）
 */
struct operation_base {
    operation_base* next;

    typedef void (*func_type)(operation_base*);
    typedef void (*destroy_type)(operation_base*);

    func_type    func;
    destroy_type destroy_func;

    operation_base(func_type f, destroy_type d)
        : next(nullptr), func(f), destroy_func(d)
    {
    }

    void execute()
    {
        func(this);
    }

    void destroy()
    {
        destroy_func(this);
    }
};

} // namespace mc::runtime

#endif // MC_RUNTIME_OPERATION_BASE_H
