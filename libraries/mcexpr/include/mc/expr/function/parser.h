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

#ifndef MC_EXPR_FUNCTION_PARSER_H
#define MC_EXPR_FUNCTION_PARSER_H

#include <mc/dict.h>
#include <mc/engine/property/relation.h>
#include <mc/expr/function/call.h>
#include <mc/reflect.h>
#include <mc/string.h>
#include <mc/string_view.h>

namespace mc::expr {

using relate_property = mc::engine::relate_property;

// 引用对象结构
struct relate_object {
    MC_REFLECTABLE("mc.expr.relate_object");

    mc::string type;
    mc::string object_name;
    mc::string full_name;
};

// 属性类型配置结构
struct property_type_config {
    mc::string_view prefix;
    mc::string_view type_name;
    mc::string_view error_message_prefix;
};

struct dotted_property_parts {
    mc::string object_name;
    mc::string property_name;
};

class MC_API func_parser {
public:
    static func_parser& get_instance();

    func_call       parse_function_call(mc::string_view input);
    relate_property parse_property(mc::string_view input);
    relate_property parse_sync_property(mc::string_view input);
    relate_property parse_ref_property(mc::string_view input);
    relate_object   parse_ref_object(mc::string_view input);

private:
    func_parser()                              = default;
    ~func_parser()                             = default;
    func_parser(const func_parser&)            = delete;
    func_parser& operator=(const func_parser&) = delete;

    dotted_property_parts parse_dotted_property(mc::string_view input);
    mc::string            parse_function_name(mc::string_view input);
    mc::string            parse_params_string(mc::string_view input);

    // 通用属性解析函数
    relate_property parse_property_with_type(mc::string_view input, const property_type_config& config);
};

class MC_API param_value_comparator {
public:
    bool operator()(const mc::variant& a, const mc::variant& b) const;

private:
    bool compare_dicts(const mc::dict& a, const mc::dict& b) const;
};

} // namespace mc::expr

#endif // MC_EXPR_FUNCTION_PARSER_H