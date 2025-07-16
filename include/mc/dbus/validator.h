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
#include <string_view>

#include <mc/common.h>

namespace mc::dbus {

class MC_API validator {
public:
    MC_API static bool is_valid_interface_name(std::string_view name);
    MC_API static bool is_valid_member_name(std::string_view name);

    /*
     * 检查 bus name 是否有效
     *
     * 有效 bus name 格式：
     * bus name 是以点号分段的多段字符串，至少需要一个点的两段名称，如果
     * 以冒号开头表示唯一的 bus name，每段名称只能包含字母、数字、下划线，
     * 每段不能以数字和点开头
     */
    MC_API static bool is_valid_bus_name(std::string_view name);

    MC_API static bool is_valid_error_name(std::string_view errorname);
    MC_API static bool is_valid_path(std::string_view path);
    MC_API static bool is_message_too_large(std::size_t size);

    MC_API static constexpr uint32_t maximum_array_size    = (0x01 << 26);
    MC_API static constexpr uint32_t maximum_message_size  = (0x01 << 27);
    MC_API static constexpr uint32_t maximum_message_depth = 64;
};

} // namespace mc::dbus
