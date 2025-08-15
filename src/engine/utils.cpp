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
#include <mc/engine/utils.h>
#include <mc/string.h>

namespace mc::engine::detail {

bool path_starts_with(std::string_view path, std::string_view prefix) {
    if (!mc::string::starts_with(path, prefix)) {
        return false;
    }

    // 是根路径
    if (prefix.size() == 1 && prefix[0] == '/') {
        return true;
    }

    // 否则必须判断末尾是 / 分隔符
    return path.size() > prefix.size() && path[prefix.size()] == '/';
}

} // namespace mc::engine::detail
