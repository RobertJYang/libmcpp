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

#ifndef MC_ERROR_MESSAGE_PARSER_H
#define MC_ERROR_MESSAGE_PARSER_H

#include <mc/dict.h>
#include <mc/variant.h>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc {

/**
 * @brief 错误定义消息结构
 *
 * 对应 JSON 文件中的单个 Message 定义
 */
struct error_message_definition {
    error_message_definition()                                               = default;
    error_message_definition(const error_message_definition&)                = default;
    error_message_definition(error_message_definition&&) noexcept            = default;
    error_message_definition& operator=(const error_message_definition&)     = default;
    error_message_definition& operator=(error_message_definition&&) noexcept = default;

    /**
     * @brief 错误描述
     */
    std::string description;

    /**
     * @brief 错误消息模板（包含 %1, %2 等占位符）
     */
    std::string message;

    /**
     * @brief 错误严重程度（OK、Warning、Critical）
     */
    std::string severity;

    /**
     * @brief 参数数量
     */
    int number_of_args = 0;

    /**
     * @brief 参数类型列表
     */
    std::vector<std::string> param_types;

    /**
     * @brief 解决方案
     */
    std::string resolution;

    /**
     * @brief HTTP 状态码
     */
    int http_status_code = 500;

    /**
     * @brief IPMI 完成码
     */
    std::string ipmi_completion_code;

    /**
     * @brief SNMP 状态码
     */
    int snmp_status_code = 0;

    /**
     * @brief 追踪深度
     */
    int trace_depth = 0;
};

/**
 * @brief 错误消息注册表
 *
 * 对应整个 JSON 文件
 */
struct error_message_registry {
    error_message_registry()                                             = default;
    error_message_registry(const error_message_registry&)                = default;
    error_message_registry(error_message_registry&&) noexcept            = default;
    error_message_registry& operator=(const error_message_registry&)     = default;
    error_message_registry& operator=(error_message_registry&&) noexcept = default;

    /**
     * @brief 注册表描述
     */
    std::string description;

    /**
     * @brief 注册表前缀（Base 或 openUBMC）
     */
    std::string registry_prefix;

    /**
     * @brief 注册表版本
     */
    std::string registry_version;

    /**
     * @brief 消息映射表
     */
    std::unordered_map<std::string, error_message_definition> messages;
};

/**
 * @brief 错误消息解析器
 *
 * 负责解析错误定义文件（base.json 和 custom.json），并提供查询功能
 */
class error_message_parser {
public:
    error_message_parser()                                           = default;
    ~error_message_parser()                                          = default;
    error_message_parser(const error_message_parser&)                = delete;
    error_message_parser& operator=(const error_message_parser&)     = delete;
    error_message_parser(error_message_parser&&) noexcept            = default;
    error_message_parser& operator=(error_message_parser&&) noexcept = default;

    /**
     * @brief 从 JSON 字符串加载错误定义
     *
     * @param json_content JSON 字符串内容
     * @return 解析后的错误注册表
     */
    static MC_API error_message_registry parse_from_string(std::string_view json_content);

    /**
     * @brief 从 JSON 文件加载错误定义
     *
     * @param file_path JSON 文件路径
     * @return 解析后的错误注册表
     */
    static MC_API error_message_registry parse_from_file(std::string_view file_path);

    /**
     * @brief 查找错误定义
     *
     * @param registry 错误注册表
     * @param message_name 消息名称
     * @return 错误定义，如果未找到返回 std::nullopt
     */
    static MC_API std::optional<error_message_definition> find_message(const error_message_registry& registry,
                                                                       std::string_view              message_name);

    /**
     * @brief 格式化错误消息
     *
     * 将消息模板中的 %1, %2 等占位符替换为实际参数值
     *
     * @param template_msg 消息模板
     * @param args 参数列表
     * @return 格式化后的消息
     */
    static MC_API std::string format_message(std::string_view template_msg, const mc::dict& args);

    /**
     * @brief 格式化错误消息（使用错误定义）
     *
     * @param def 错误定义
     * @param args 参数列表
     * @return 格式化后的消息
     */
    static MC_API std::string format_message(const error_message_definition& def, const mc::dict& args);
};

} // namespace mc

#endif // MC_ERROR_MESSAGE_PARSER_H
