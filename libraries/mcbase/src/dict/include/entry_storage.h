/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_SRC_DICT_INCLUDE_ENTRY_STORAGE_H
#define MC_SRC_DICT_INCLUDE_ENTRY_STORAGE_H

#include <cstddef>
#include <mc/variant/variant_common.h>

namespace mc::dict_types {

struct entry;
}

namespace mc::dict_types {

/**
 * @brief 从进程内共享的对象池中构造一个 entry
 */
entry* entry_storage_create(mc::variant key, mc::variant value);

/**
 * @brief 销毁并归还 entry 到共享对象池
 */
void entry_storage_destroy(entry* item) noexcept;

/**
 * @brief 预分配至少 slot_count 个槽位（不构造对象）
 */
void entry_storage_reserve(std::size_t slot_count);

} // namespace mc::dict_types

#endif // MC_SRC_DICT_INCLUDE_ENTRY_STORAGE_H
