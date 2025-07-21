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

#ifndef MC_EXPR_FUNCTION_CALL_H
#define MC_EXPR_FUNCTION_CALL_H

#include <mc/dict.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <string>

namespace mc::expr {

struct func_call {
    MC_REFLECTABLE();

    std::string      func;
    mc::mutable_dict params;

private:
    friend struct mc::reflect::reflector<func_call>;
};

MC_API void to_variant(const func_call& fc, mc::variant& v);
MC_API void from_variant(const mc::variant& v, func_call& fc);
MC_API bool is_function_call(const mc::variant& v);

// 声明 is_relate_property 函数
MC_API bool is_relate_property(const mc::variant& value);

class MC_API func {
public:
    MC_REFLECTABLE();

    MC_API func() = default;

    // 修改构造函数参数顺序，使其与测试用例匹配
    MC_API func(const std::string& result, const mc::dict& args) : m_result(result), m_args(args) {
    }

    MC_API void validate_result();
    MC_API void validate_args();
    MC_API mc::variant call(const std::string_view& position, mc::mutable_dict& params);
    MC_API mc::mutable_dict get_relate_properties(const std::string_view& position, mc::mutable_dict& params);

    // 添加访问 m_args 的方法
    MC_API const mc::dict& get_args() const {
        return m_args;
    }
    MC_API void set_args(const mc::dict& args) {
        m_args = args;
    }

private:
    friend struct mc::reflect::reflector<func>;

    std::string m_result;
    mc::dict    m_args;
};

} // namespace mc::expr

#endif // MC_EXPR_FUNCTION_CALL_H