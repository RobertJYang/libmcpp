/**
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

#ifndef MC_ENGINE_ERROR_ENGINE_H
#define MC_ENGINE_ERROR_ENGINE_H
#include <mc/engine/error.h>

namespace mc::engine {

class error_engine : public mc::noncopyable_nonmovable {
public:
    error_engine();
    ~error_engine();

    static error_engine& get_instance();

    void             register_const_error(const error_info& info);
    void             register_const_error(std::string_view name, std::string_view format);
    void             register_error(std::string name, std::string format);
    std::string_view get_error_format(std::string_view name);
    bool             is_registered(std::string_view name);

    /*
     * 报告错误到错误引擎，错误必须预先注册到错误引擎，否则抛出常
     *
     * @param name 错误名称
     * @param format 格式化字符串
     * @return 创建的错误
     */
    error& report_error(std::string_view name);
    error& report_error(const error_info& info);
    void   set_last_error(const error& error);
    void   reset_error();
    error& last_error();

    /*------------------- 一些静态辅助函数 -------------------*/
    /*
     * 检查错误名称是否有效
     *
     * @param name 错误名称
     * @return 如果有效返回 true，否则返回 false
     */
    static bool is_valid_error_name(std::string_view name);

    /**
     * @brief 解析 format 字符串，找到 ${name} 格式的占位符，并将其添加到 arg_names 中
     *
     * @param format 格式化字符串
     * @param arg_names 存储占位符名称的字典
     * @return 如果解析成功返回 true，否则返回 false
     */
    static bool get_format_args(std::string_view format, mc::dict& arg_names, error& error);

    /*
     * 创建错误，如果 name 不满足要求抛出错误
     *
     * @param name 错误名称
     * @param format 格式化字符串
     * @return 创建的错误
     *
     * 注意：这个方法可以创建任意错误，并不要求错误必须注册到错误引擎中
     */
    static error make_error(std::string_view name, std::string_view format);
    static error make_error(const error_info& info);

private:
    struct error_engine_impl;
    std::unique_ptr<error_engine_impl> m_impl;
};

} // namespace mc::engine

#endif // MC_ENGINE_ERROR_ENGINE_H