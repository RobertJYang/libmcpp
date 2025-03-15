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

/**
 * @file yaml.h
 * @brief YAML编解码功能的实现
 */
#ifndef MC_YAML_H
#define MC_YAML_H

#include <mc/variant.h>
#include <mc/exception.h>
#include <string>
#include <string_view>

namespace mc {
namespace yaml {

/**
 * @brief YAML编码选项
 */
struct yaml_encode_options {
    /// 全局默认编码配置
    static yaml_encode_options default_encode_options;

    /**
     * @brief 是否格式化输出
     * @details 如果为true，则输出格式化的YAML，包含缩进和换行
     *         如果为false，则输出紧凑的YAML，不包含多余的空白字符
     */
    bool pretty_print = true; // YAML默认使用格式化输出

    /**
     * @brief 缩进大小
     * @details 指定格式化输出时的缩进空格数
     *         取值范围：[0, 8]，超出范围会被截断到最近的有效值
     */
    int indent_size = 2; // YAML通常使用2空格缩进

    /**
     * @brief 是否对对象的键进行排序
     * @details 如果为true，则按字典序对对象的键进行排序
     *         如果为false，则保持原有顺序
     */
    bool sort_keys = false;

    /**
     * @brief 浮点数精度
     * @details 指定浮点数的小数位数
     *         -1表示使用默认精度（std::numeric_limits<double>::max_digits10）
     *         取值范围：[-1, 17]，超出范围会被截断到最近的有效值
     */
    int float_precision = -1;

    /**
     * @brief 最大嵌套深度
     * @details 限制YAML的最大嵌套深度，防止栈溢出
     *         取值范围：[1, 512]，超出范围会被截断到最近的有效值
     *         建议值：不超过100，默认值32足够处理大多数场景
     * @note 此限制影响数组和对象的嵌套层级
     */
    int max_depth = 32;

    /**
     * @brief 是否使用流式风格
     * @details 如果为true，则使用流式风格（使用{}和[]）
     *         如果为false，则使用块式风格（使用缩进表示层级）
     */
    bool flow_style = false;

    /**
     * @brief 是否使用单引号包围字符串
     * @details 如果为true，则使用单引号包围字符串
     *         如果为false，则根据字符串内容决定是否需要引号
     */
    bool single_quote = false;

    /**
     * @brief 验证并调整选项值到有效范围
     */
    void normalize() {
        // 缩进大小限制在[0, 8]范围内
        if (indent_size < 0) {
            indent_size = 0;
        } else if (indent_size > 8) {
            indent_size = 8;
        }

        // 浮点数精度限制在[-1, 17]范围内
        if (float_precision < -1) {
            float_precision = -1;
        } else if (float_precision > 17) {
            float_precision = 17;
        }

        // 最大嵌套深度限制在[1, 512]范围内
        if (max_depth < 1) {
            max_depth = 1;
        } else if (max_depth > 512) {
            max_depth = 512;
        }
    }
};

/**
 * @brief YAML解码选项
 */
struct yaml_decode_options {
    /// 全局默认解码配置
    static yaml_decode_options default_decode_options;

    /**
     * @brief 最大嵌套深度
     * @details 限制YAML的最大嵌套深度，防止栈溢出
     *         取值范围：[1, +∞)，小于1的值会被调整为1
     *         建议值：不超过100，默认值32足够处理大多数场景
     * @note 此限制影响数组和对象的嵌套层级
     */
    int max_depth = 32;

    /**
     * @brief 输入YAML字符串的最大长度
     * @details 限制输入YAML文本的总字节数，防止消耗过多内存
     *         取值范围：[1, +∞)，小于1的值会被调整为1
     *         默认值16MB适用于大多数场景
     * @note 此限制检查发生在解析开始前
     *       对于超长输入会立即返回错误，避免不必要的处理
     */
    size_t max_input_length = 16 * 1024 * 1024;

    /**
     * @brief 单个字符串的最大长度
     * @details 限制YAML中单个字符串的最大长度，包括对象的键名和字符串值
     *         取值范围：[1, +∞)，小于1的值会被调整为1
     *         默认值64KB适用于大多数场景
     * @note 此限制同时应用于：
     *       1. 对象的键名
     *       2. 字符串类型的值
     */
    size_t max_string_length = 65536;

    /**
     * @brief 数组最大元素数量
     * @details 限制YAML数组中的最大元素数量
     *         取值范围：[1, +∞)，小于1的值会被调整为1
     *         默认值64K适用于大多数场景
     * @note 此限制适用于每个数组
     *       对于嵌套数组，每个层级都单独计数
     */
    size_t max_array_size = 65536;

    /**
     * @brief 对象最大键值对数量
     * @details 限制YAML对象中的最大键值对数量
     *         取值范围：[1, +∞)，小于1的值会被调整为1
     *         默认值64K适用于大多数场景
     * @note 此限制适用于每个对象
     *       对于嵌套对象，每个层级都单独计数
     */
    size_t max_object_size = 65536;

    /**
     * @brief 是否允许解析多文档
     * @details 如果为true，则允许解析包含多个文档的YAML（使用---分隔）
     *         如果为false，则只解析第一个文档
     */
    bool allow_multiple_documents = false;

    /**
     * @brief 是否解析锚点和引用
     * @details 如果为true，则解析锚点(&)和引用(*)，并自动展开引用
     *         如果为false，则将锚点和引用视为普通字符串
     */
    bool resolve_anchors = true;

    /**
     * @brief 验证并调整选项值到有效范围
     * @note 所有限制值都必须大于等于1
     *       此函数会自动调整无效值到最近的有效值
     */
    void normalize() {
        max_depth = std::max(1, max_depth);
        max_input_length = std::max<size_t>(1, max_input_length);
        max_string_length = std::max<size_t>(1, max_string_length);
        max_array_size = std::max<size_t>(1, max_array_size);
        max_object_size = std::max<size_t>(1, max_object_size);
    }
};

/**
 * @brief 将variant编码为YAML字符串
 * 
 * @param value 要编码的variant对象
 * @param options 编码选项
 * @return std::string 编码后的YAML字符串
 * @throw mc::parse_error_exception 当编码失败时抛出异常
 */
std::string yaml_encode(const variant& value, const yaml_encode_options& options = yaml_encode_options::default_encode_options);

/**
 * @brief 从YAML字符串解码为variant对象
 * 
 * @param yaml YAML字符串视图
 * @param options 解码选项
 * @return variant 解码后的variant对象
 * @throw mc::parse_error_exception 当解码失败时抛出异常
 * @note 此函数可以接受以下类型的参数：
 *       1. std::string - 通过string_view的隐式转换
 *       2. std::string_view - 直接使用
 *       3. const char* - 通过string_view的隐式转换
 *       4. const char* + size_t - 通过string_view(const char*, size_t)构造
 */
variant yaml_decode(std::string_view yaml, const yaml_decode_options& options = yaml_decode_options::default_decode_options);

/**
 * @brief 从YAML字符串解码为多个variant对象（多文档模式）
 * 
 * @param yaml YAML字符串视图，可能包含多个文档（使用---分隔）
 * @param options 解码选项
 * @return std::vector<variant> 解码后的variant对象数组，每个元素对应一个YAML文档
 * @throw mc::parse_error_exception 当解码失败时抛出异常
 * @note 如果options.allow_multiple_documents为false，则只返回第一个文档
 */
std::vector<variant> yaml_decode_all(std::string_view yaml, const yaml_decode_options& options = yaml_decode_options::default_decode_options);

} // namespace yaml
} // namespace mc

#endif // MC_YAML_H