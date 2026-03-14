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

#ifndef MC_ERROR_MESSAGE_CONVERTER_H
#define MC_ERROR_MESSAGE_CONVERTER_H

#include <mc/dict.h>
#include <mc/error.h>
#include <mc/error_message_parser.h>

#include <mutex>
#include <string>

namespace mc {

/**
 * @brief 标准错误消息格式
 *
 * 对应 Redfish/HTTP 接口的标准错误消息格式
 */
struct standard_error_message {
    MC_REFLECTABLE("mc.standard_error_message");

    /**
     * @brief 消息 ID（格式：Prefix.1.0.0.MessageName）
     */
    std::string message_id;

    /**
     * @brief 消息名称
     */
    std::string message_name;

    /**
     * @brief 消息参数
     */
    mc::dict message_args;

    /**
     * @brief 注册表前缀
     */
    std::string registry_prefix;

    /**
     * @brief 注册表版本
     */
    std::string registry_version;

    /**
     * @brief HTTP 状态码
     */
    int http_status_code;

    /**
     * @brief 严重程度
     */
    std::string severity;

    /**
     * @brief 格式化后的消息文本
     */
    std::string message;

    /**
     * @brief 解决方案
     */
    std::string resolution;

    /**
     * @brief 相关属性列表（Redfish 规范）
     */
    std::vector<std::string> related_properties;

    /**
     * @brief 转换为 dict
     */
    mc::dict to_dict() const;
};

/**
 * @brief 错误消息转换器
 *
 * 负责将 mc::error 转换为标准消息格式
 */
class error_message_converter {
public:
    error_message_converter()                                              = default;
    ~error_message_converter()                                             = default;
    error_message_converter(const error_message_converter&)                = delete;
    error_message_converter& operator=(const error_message_converter&)     = delete;
    error_message_converter(error_message_converter&&) noexcept            = default;
    error_message_converter& operator=(error_message_converter&&) noexcept = default;

    /**
     * @brief 加载标准错误定义文件
     *
     * @param base_json_path base.json 文件路径
     * @param custom_json_path custom.json 文件路径
     */
    MC_API void load_registries(std::string_view base_json_path, std::string_view custom_json_path);

    /**
     * @brief 从字符串加载标准错误定义
     *
     * @param base_json base.json 内容
     * @param custom_json custom.json 内容
     */
    MC_API void load_registries_from_string(std::string_view base_json, std::string_view custom_json);

    /**
     * @brief 将 error 转换为标准消息格式
     *
     * @param err 错误对象
     * @param related_properties 相关属性列表（Redfish 规范）
     * @return 标准错误消息
     */
    MC_API standard_error_message convert(const mc::error&                err,
                                          const std::vector<std::string>& related_properties = {}) const;

    /**
     * @brief 将 error 转换为标准消息格式（dict 格式）
     *
     * @param err 错误对象
     * @param related_properties 相关属性列表（Redfish 规范）
     * @return dict 格式的错误消息
     */
    MC_API mc::dict convert_to_dict(const mc::error&                err,
                                    const std::vector<std::string>& related_properties = {}) const;

    /**
     * @brief 查找错误定义
     *
     * @param message_name 消息名称
     * @return 错误定义，如果未找到返回 std::nullopt
     */
    MC_API std::optional<error_message_definition> find_definition(std::string_view message_name) const;

    /**
     * @brief 获取单例实例
     */
    static MC_API error_message_converter& get_instance();

private:
    mutable std::mutex     m_mutex; // 保护注册表访问的互斥锁
    error_message_registry m_base_registry;
    error_message_registry m_custom_registry;
};

/**
 * @brief 辅助函数：快速转换 error 为标准消息格式
 *
 * @param err 错误对象
 * @param related_properties 相关属性列表（Redfish 规范）
 * @return 标准错误消息
 */
MC_API standard_error_message to_standard_message(const mc::error&                err,
                                                  const std::vector<std::string>& related_properties = {});

/**
 * @brief 辅助函数：快速转换 error 为 dict 格式
 *
 * @param err 错误对象
 * @param related_properties 相关属性列表（Redfish 规范）
 * @return dict 格式的错误消息
 */
MC_API mc::dict to_standard_message_dict(const mc::error& err, const std::vector<std::string>& related_properties = {});

} // namespace mc

#endif // MC_ERROR_MESSAGE_CONVERTER_H
