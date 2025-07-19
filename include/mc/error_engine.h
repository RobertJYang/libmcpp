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

#ifndef MC_ERROR_ENGINE_H
#define MC_ERROR_ENGINE_H

#include <mc/error.h>

namespace mc {

class error_engine : public mc::noncopyable_nonmovable {
public:
    error_engine();
    ~error_engine();

    static MC_API error_engine& get_instance();
    static MC_API void          reset_for_test();

    /*
     * 注册常量错误
     *
     * @param info 错误信息
     */
    MC_API error_info register_const_error(const error_info& info);
    MC_API error_info register_const_error(std::string_view name, std::string_view format = {},
                                           error_level level = error_level::error);

    /*
     * 注册动态错误
     *
     * @param name 错误名称
     * @param format 格式化字符串
     */
    MC_API error_info register_error(std::string name, std::string format,
                                     error_level level = error_level::error);

    /*
     * 获取错误信息
     *
     * @param name 错误名称
     * @return 错误信息
     */
    MC_API error_info get_error_info(std::string_view name);

    /*
     * 检查错误是否已注册
     *
     * @param name 错误名称
     * @return 如果已注册返回 true，否则返回 false
     */
    MC_API bool is_registered(std::string_view name);

    /*
     * 报告错误到错误引擎，错误必须预先注册到错误引擎，否则抛出常
     *
     * @param name 错误名称
     * @param format 格式化字符串
     * @return 创建的错误
     */
    MC_API error_ptr report_error(std::string_view name, mc::dict args = {});
    MC_API error_ptr report_error(const error_info& info, mc::dict args = {});
    MC_API error_ptr set_last_error(error_ptr new_error);
    MC_API void      reset_error();
    MC_API error_ptr last_error();

private:
    struct error_engine_impl;
    std::unique_ptr<error_engine_impl> m_impl;
};

} // namespace mc

#define REGISTER_CONST_ERROR(NAME, ERROR, ...) \
    inline auto NAME =                         \
        mc::engine::error_engine::get_instance().register_const_error(ERROR, ##__VA_ARGS__)

#define REGISTER_ERROR(NAME, ERROR, ...) \
    inline auto NAME = mc::engine::error_engine::get_instance().register_error(ERROR, ##__VA_ARGS__)

#endif // MC_ENGINE_ERROR_ENGINE_H