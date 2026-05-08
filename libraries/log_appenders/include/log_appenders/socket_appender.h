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

#ifndef MC_LOG_APPENDERS_SOCKET_APPENDER_H
#define MC_LOG_APPENDERS_SOCKET_APPENDER_H

#include <log_appenders/socket_client.h>
#include <mc/log/appender.h>
#include <mc/log/logger.h>
#include <mc/reflect.h>
#include <mc/timer.h>
#include <mc/variant.h>

#include <memory>
#include <mutex>
#include <string>

namespace mc {
namespace log {

/**
 * @brief socket日志追加器配置
 */
struct socket_appender_config {
    std::string name{"mdbctl"}; ///< 追加器名称
    std::string path;           ///< unix socket路径
};

/**
 * @brief 通过unix socket发送日志
 */
class MC_API socket_appender : public appender {
public:
    socket_appender();
    explicit socket_appender(std::shared_ptr<socket_client> shared_client);
    ~socket_appender() override = default;

    bool init(const variant& args) override;
    void append(const message& msg) override;

    void               set_path(std::string_view path);
    const std::string& get_path() const;

    void set_hb_path(std::string_view hb_path);

    void               set_type(const std::string& type);
    const std::string& get_type() const;

    socket_client&                 get_client();
    std::shared_ptr<socket_client> get_client_shared();
    bool                           connect();
    void                           disconnect();
    bool                           is_connected() const;
    bool                           heartbeat();

private:
    bool        ensure_connected();
    std::string format_message(const message& msg) const;
    void        stop_heartbeat_timer();
    void        start_heartbeat_timer();

    std::shared_ptr<socket_client> m_client;
    std::string                    m_path;         ///< 日志socket路径
    std::string                    m_type{"file"}; ///< 日志输出类型
    std::string                    m_hb_path;      ///< 心跳socket路径
    mc::timer_ptr                  m_hb_timer;     ///< 心跳定时器（由 init 在 auto_connect 时启动）
    int                            m_heartbeat_interval_sec{1};
    bool                           m_auto_connect{true};
    mutable std::recursive_mutex   m_mutex;
};

} // namespace log
} // namespace mc

#endif // MC_LOG_APPENDERS_SOCKET_APPENDER_H
