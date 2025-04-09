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
 * @file variant.cpp
 * @brief 实现 variant.h 中声明的方法
 */
#include <cstring>
#include <limits>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/string.h>
#include <mc/variant.h>
#include <mc/variant/variant_base.h>
#include <stdexcept>

namespace mc {

// 获取类型名称
const char* get_type_name_internal(type_id type) {
    static const char* type_names[] = {
        "null",   // null_type
        "int8",   // int8_type
        "uint8",  // uint8_type
        "int16",  // int16_type
        "uint16", // uint16_type
        "int32",  // int32_type
        "uint32", // uint32_type
        "int64",  // int64_type
        "uint64", // uint64_type
        "double", // double_type
        "bool",   // bool_type
        "string", // string_type
        "array",  // array_type
        "object", // object_type
        "blob"    // blob_type
    };

    int index = static_cast<int>(type);
    if (index >= 0 && index < static_cast<int>(type_id::max_type)) {
        return type_names[index];
    }

    return "unknown";
}

void throw_type_error(const char* expected_type, type_id actual_type) {
    if (actual_type < type_id::max_type) {
        MC_THROW(mc::invalid_arg_exception, "类型错误：期望${type}，实际为${actual_type}",
                 ("type", expected_type)("actual_type", get_type_name_internal(actual_type)));
    } else {
        MC_THROW(mc::invalid_arg_exception, "类型错误：期望${type}，实际为${actual_type}",
                 ("type", expected_type)("actual_type", static_cast<int>(actual_type)));
    }
}

void throw_unknow_type_error(type_id actual_type) {
    MC_THROW(mc::invalid_arg_exception, "未知类型：${type}",
             ("type", static_cast<int>(actual_type)));
}

void throw_invalid_type_comparison_error(const char* type1, const char* type2, const char* op) {
    MC_THROW(mc::invalid_op_exception, "不支持的类型比较操作: ${type1} ${op} ${type2}",
             ("type1", type1)("op", op)("type2", type2));
}

// 计算字符串的哈希值
size_t calculate_str_hash(const char* data, size_t length) {
    if (!data || length == 0) {
        return 0;
    }

    // 使用黄金比例作为种子
    size_t       h    = 0x9e3779b9 ^ length;
    const size_t step = (length >> 5) + 1;
    for (size_t l1 = length; l1 >= step; l1 -= step) {
        h = h ^ ((h << 5) + (h >> 2) + static_cast<uint8_t>(data[l1 - 1]));
    }

    return h;
}

} // namespace mc