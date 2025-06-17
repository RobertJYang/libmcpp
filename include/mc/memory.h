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

#ifndef MC_MEMORY_H
#define MC_MEMORY_H

/**
 * @file memory.h
 * @brief MC内存管理模块统一导出头文件
 *
 * 本头文件提供了MC库的所有内存管理功能，包括：
 * - 引用计数基类 (shared_base)
 * - 智能指针 (ref_ptr)
 * - 弱引用指针 (weak_ptr)
 * - 内存分配和管理工具
 *
 * 使用方式：
 * @code
 * #include <mc/memory.h>
 *
 * class MyClass : public mc::shared_base<MyClass> {
 *     // ...
 * };
 *
 * auto ptr = mc::make_shared<MyClass>();
 * mc::weak_ptr<MyClass> weak = ptr;
 * @endcode
 */

// 核心内存管理组件
#include <mc/memory/allocator.h>
#include <mc/memory/shared_base.h>
#include <mc/memory/shared_ptr.h>
#include <mc/memory/weak_ptr.h>

namespace mc {

using mc::memory::shared_base;
using mc::memory::shared_ptr;
using mc::memory::weak_ptr;

using mc::memory::allocate_shared;
using mc::memory::make_shared;

using mc::memory::const_pointer_cast;
using mc::memory::dynamic_pointer_cast;
using mc::memory::reinterpret_pointer_cast;
using mc::memory::static_pointer_cast;

namespace memory {
/**
 * @brief 获取对象的引用计数
 * @param ptr 指向ref_base对象的指针
 * @return 当前引用计数，如果ptr为空则返回0
 */
template <typename T>
size_t use_count(const T* ptr) noexcept {
    return ptr ? ptr->ref_count() : 0;
}

/**
 * @brief 检查对象是否唯一引用
 * @param ptr 指向ref_base对象的指针
 * @return 如果引用计数为1则返回true，否则返回false
 */
template <typename T>
bool unique(const T* ptr) noexcept {
    return use_count(ptr) == 1;
}
} // namespace memory
} // namespace mc

#endif // MC_MEMORY_H