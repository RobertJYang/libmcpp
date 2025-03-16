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

#include <iomanip>
#include <mc/log/log_message.h>
#include <regex>
#include <sstream>

namespace mc {
namespace log {

// 辅助函数：字典转字符串
std::string dict_to_string(const dict& d) {
    std::ostringstream ss;
    ss << "{";
    bool first = true;

    for (const auto& key : d.keys()) {
        if (!first) {
            ss << ", ";
        }
        ss << "\"" << key << "\": ";

        const variant& value = d[key];

        if (value.is_object()) {
            // 递归处理嵌套字典
            ss << dict_to_string(value.as<dict>());
        } else if (value.is_string()) {
            // 字符串值需要引号
            ss << "\"" << value.as<std::string>() << "\"";
        } else {
            // 其他类型值
            ss << value.as_string();
        }

        first = false;
    }

    ss << "}";
    return ss.str();
}

} // namespace log
} // namespace mc