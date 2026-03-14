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
#ifndef MC_MDB_VALIDATOR_H
#define MC_MDB_VALIDATOR_H

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <mc/exception.h>
#include <mc/variant.h>

#include "../introspection/introspection_types.h"

class mdb_validator {
public:
    // 检查单个属性值是否符合DBus类型签名
    static void check(const std::string& name, const std::string& signature, const mc::variant& value);

    // 校验方法参数
    static void check_method_args(const std::string& method_name, const std::string& signature,
                                  const mc::variant& args);

private:
    static void check_type(const std::string& signature, const mc::variant& value);

    static void check_one(const std::string& signature, const mc::variant& value);

    static void check_basic(const std::string& signature, const mc::variant& value);

    static void check_array(const std::string& elem_sig, const mc::variant& value);

    static void check_struct(const std::string& signature, const mc::variant& value);

    static void check_dict(const std::string& signature, const mc::variant& value);

    static bool is_basic_type(const std::string& sig);

    static bool is_integer_type(const std::string& sig);

    static bool is_string_type(const std::string& sig);

    static bool is_struct_type(const std::string& sig);

    static bool is_dict_type(const std::string& sig);

    static void check_integer_range(const std::string& sig, const mc::variant& value);

    static void check_utf8(const std::string& s);
};

#endif // MC_MDB_VALIDATOR_H
