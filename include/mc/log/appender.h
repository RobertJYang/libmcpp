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

#ifndef MC_LOG_APPENDER_H
#define MC_LOG_APPENDER_H

#include <mc/log/log_message.h>

namespace mc {
namespace log {

/**
 * @brief 日志追加器接口
 *
 * 日志追加器负责将日志消息输出到特定目标（控制台、文件等）
 */
class appender {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~appender() = default;

    /**
     * @brief 初始化追加器
     *
     * @param config 配置字符串，可以是JSON或其他格式
     * @return bool 初始化是否成功
     */
    virtual bool init(const variant& args) = 0;

    /**
     * @brief 追加日志消息
     *
     * @param msg 日志消息
     */
    virtual void append(const message& msg) = 0;
    
    /**
     * @brief 获取追加器名称
     *
     * @return const std::string& 追加器名称
     */
    const std::string& get_name() const {
        return m_name;
    }
    
    /**
     * @brief 设置追加器名称
     *
     * @param name 追加器名称
     */
    void set_name(const std::string& name) {
        m_name = name;
    }

protected:
    std::string m_name; // 追加器名称
};

using appender_ptr = std::shared_ptr<appender>;

/**
 * @brief 追加器注册宏
 *
 * 用于在动态库中注册追加器
 */
#define MC_REGISTER_APPENDER(appender_class)                                                       \
    extern "C" void* create_appender() {                                                           \
        return new appender_ptr(new appender_class);                                               \
    }                                                                                              \
                                                                                                   \
    extern "C" void destroy_appender(void* ptr) {                                                  \
        delete static_cast<appender_ptr*>(ptr);                                                    \
    }

} // namespace log
} // namespace mc

#endif // MC_LOG_APPENDER_H