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

/**
 * @brief DBus名称和消息验证器
 */
class MC_API validator {
public:
    /**
     * @brief 检查接口名称是否有效
     * @param name [in] 接口名称
     * @return 有效返回true，否则返回false
     * @constraint 接口名称必须符合DBus规范
     */
    static bool is_valid_interface_name(std::string_view name);

    /**
     * @brief 检查成员名称是否有效
     * @param name [in] 成员名称
     * @return 有效返回true，否则返回false
     * @constraint 成员名称必须符合DBus规范
     */
    static bool is_valid_member_name(std::string_view name);

    /**
     * @brief 检查总线名称是否有效
     * @param name [in] 总线名称
     * @return 有效返回true，否则返回false
     * @constraint 有效bus name格式：
     *   - bus name是以点号分段的多段字符串，至少需要一个点的两段名称
     *   - 如果以冒号开头表示唯一的bus name
     *   - 每段名称只能包含字母、数字、下划线
     *   - 每段不能以数字和点开头
     */
    static bool is_valid_bus_name(std::string_view name);

    /**
     * @brief 检查错误名称是否有效
     * @param errorname [in] 错误名称
     * @return 有效返回true，否则返回false
     * @constraint 错误名称必须符合DBus规范
     */
    static bool is_valid_error_name(std::string_view errorname);

    /**
     * @brief 检查对象路径是否有效
     * @param path [in] 对象路径
     * @return 有效返回true，否则返回false
     * @constraint 对象路径必须符合DBus规范
     */
    static bool is_valid_path(std::string_view path);

    /**
     * @brief 检查消息大小是否过大
     * @param size [in] 消息大小（字节）
     * @return 过大返回true，否则返回false
     * @constraint 消息大小不能超过maximum_message_size
     */
    static bool is_message_too_large(std::size_t size);

    static constexpr uint32_t maximum_array_size    = (0x01 << 26);
    static constexpr uint32_t maximum_message_size  = (0x01 << 27);
    static constexpr uint32_t maximum_message_depth = 64;
};

} // namespace mc::dbus
