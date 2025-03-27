/*
* Copyright (c) 2023 Huawei Technologies Co., Ltd.
* openUBMC is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*         http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#ifndef MC_DATABASE_COMMON_H
#define MC_DATABASE_COMMON_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <functional>

namespace mc::database {

// 错误码定义
enum class error_code {
    success = 0,
    not_initialized,
    already_exists,
    not_found,
    invalid_path,
    access_denied,
    out_of_memory,
    invalid_argument,
    internal_error,
    timeout,
    connection_failure
};

// 路径类型
using path = std::string;
using path_view = std::string_view;

// 客户端ID类型
using client_id = uint32_t;

// 对象ID类型
using object_id = uint64_t;

// 标准回调类型
using completion_handler = std::function<void(error_code)>;
using data_handler = std::function<void(error_code, void*, size_t)>;
using boolean_handler = std::function<void(error_code, bool)>;
using string_list_handler = std::function<void(error_code, const std::vector<std::string>&)>;

// 获取错误码描述
std::string_view error_to_string(error_code code);

} // namespace mc::database

#endif // MC_DATABASE_COMMON_H 