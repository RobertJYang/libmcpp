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

#include <log_appenders/socket_appender.h>

#include <cstdio>
#include <dlfcn.h>
#include <mc/filesystem.h>
#include <mc/log/appender.h>
#include <mc/log/log_level.h>
#include <mc/time.h>

#if __has_include(<logging_internal.h>)
// 优先使用依赖库 liblogger 提供的头
#include <logging_internal.h>
#else
// fallback：工程内 stub/测试用实现（native/UT 环境）
#include <log/logging_internal.h>
#endif

namespace mc {
namespace log {

socket_appender::socket_appender() : m_client(std::make_shared<socket_client>())
{}

socket_appender::socket_appender(std::shared_ptr<socket_client> shared_client) : m_client(std::move(shared_client))
{}
static std::string g_module_name{"Unknown"}; // 全局模块名称，所有 socket_appender 实例共享
typedef const char* (*get_log_time_str_func_t)(int);
static get_log_time_str_func_t get_log_time_str_ptr = nullptr;
constexpr uint32_t             LOG_US_TIME          = 0x02;

void socket_appender::stop_heartbeat_timer()
{
    if (m_hb_timer) {
        m_hb_timer->stop();
        m_hb_timer.reset();
    }
}

void socket_appender::start_heartbeat_timer()
{
    disconnect();
    if (m_heartbeat_interval_sec <= 0) {
        return;
    }

    auto timer = mc::make_shared<mc::timer>();
    timer->set_interval(mc::seconds(m_heartbeat_interval_sec));
    timer->timeout.connect([this]() {
        bool ok = true;
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            ok = m_client->heartbeat();
            if (!ok) {
                m_client->disconnect();
            }
        }
        if (!ok) {
            stop_heartbeat_timer();
        }
    });
    m_hb_timer = timer;
    m_hb_timer->start(mc::seconds(m_heartbeat_interval_sec));
}

bool socket_appender::init(const variant& args)
{
    if (!args.is_object()) {
        return false;
    }

    stop_heartbeat_timer();

    void* handle = dlopen(LOGGING_PATH, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
    } else {
        get_log_time_str_ptr = (get_log_time_str_func_t)dlsym(handle, "get_log_time_str_c");
        if (!get_log_time_str_ptr) {
            fprintf(stderr, "dlsym get_log_time_str failed: %s\n", dlerror());
        }
    }

    auto dict = args.as<mc::dict>();
    if (!dict.contains("path") || !dict.contains("hb_path")) {
        return false;
    }

    auto path    = dict["path"].as<std::string>();
    auto hb_path = dict["hb_path"].as<std::string>();
    if (dict.contains("name")) {
        set_name(dict["name"].as<std::string>());
    }

    if (dict.contains("module_name")) {
        std::string module_name = dict["module_name"].as<std::string>();
        g_module_name           = module_name;
    }

    m_auto_connect = true;
    if (dict.contains("auto_connect")) {
        m_auto_connect = dict["auto_connect"].as<bool>();
    }

    m_heartbeat_interval_sec = 1;
    if (dict.contains("heartbeat_interval_sec")) {
        m_heartbeat_interval_sec = static_cast<int>(dict["heartbeat_interval_sec"].as<std::int64_t>());
    }

    if (dict.contains("type")) {
        set_type(dict["type"].as<std::string>());
    }

    set_path(path);
    set_hb_path(hb_path);

    if (m_auto_connect) {
        if (!connect()) {
            return false;
        }
        start_heartbeat_timer();
    }

    return true;
}

namespace {
// mdbctl在接收消息后会返回确认消息"ok"，长度为2
constexpr size_t ACK_LENGTH = 2;
} // namespace

void socket_appender::append(const message& msg)
{
    // mdbctl 类别：显式调用 mdbctl_log 时输出，不受 m_type 限制
    // debug 类别：仅当 m_type 为 "local" 时输出
    bool is_mdbctl      = msg.get_category() == log_category::mdbctl;
    bool is_debug_local = msg.get_category() == log_category::debug && m_type == "local";
    if (!is_mdbctl && !is_debug_local) {
        return;
    }

    std::string payload = format_message(msg);
    if (payload.empty()) {
        return;
    }

    if (!ensure_connected()) {
        return;
    }

    auto do_send = [this, &payload]() -> bool {
        if (m_client->send(payload)) {
            return true;
        }
        m_client->disconnect();
        return ensure_connected() && m_client->send(payload);
    };
    if (!do_send()) {
        return;
    }

    std::string ack;
    if (m_client->recv_all(ack, ACK_LENGTH)) {
        return;
    }
}

void socket_appender::set_path(std::string_view path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_path.assign(path.begin(), path.end());
    m_client->set_path(m_path);
}

void socket_appender::set_hb_path(std::string_view hb_path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_client->set_hb_path(hb_path);
}

const std::string& socket_appender::get_path() const
{
    return m_path;
}

void socket_appender::set_type(const std::string& type)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_type = type;
}

const std::string& socket_appender::get_type() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_type;
}

socket_client& socket_appender::get_client()
{
    return *m_client;
}

std::shared_ptr<socket_client> socket_appender::get_client_shared()
{
    return m_client;
}

bool socket_appender::connect()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return ensure_connected();
}

void socket_appender::disconnect()
{
    stop_heartbeat_timer();
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_client->disconnect();
}

bool socket_appender::is_connected() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_client->is_connected();
}

bool socket_appender::heartbeat()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_client->heartbeat();
}

bool socket_appender::ensure_connected()
{
    if (m_client->is_connected()) {
        return true;
    }

    if (m_path.empty()) {
        return false;
    }

    m_client->set_path(m_path);
    return m_client->connect();
}

std::string socket_appender::format_message(const message& msg) const
{
    const context& ctx = msg.get_context();

    // 获取时间戳
    const char* time_str = nullptr;
    if (get_log_time_str_ptr) {
        time_str = get_log_time_str_ptr(LOG_US_TIME);
    }
    if (!time_str) {
        // 如果无法获取外部时间，使用系统时间
        std::string_view                time_view = mc::time_point::now();
        static thread_local std::string fallback_time;
        fallback_time.assign(time_view.data(), time_view.size());
        time_str = fallback_time.c_str();
    }

    // 获取日志级别字符串
    std::string_view level_str = mc::log::to_string(msg.get_level());

    // 获取文件名（去除路径）
    std::string file_name;
    if (ctx.m_file.empty()) {
        file_name = "unknown";
    } else {
        file_name = mc::filesystem::basename(ctx.m_file);
    }

    // 获取行号字符串
    std::string line_str = std::to_string(ctx.m_line);

    // 获取模块名：优先使用消息参数中的 module_name（通过 Lua 接口第一个参数传入）
    // 如果都没有，使用全局 g_module_name
    std::string module_name = g_module_name;
    const auto& args        = msg.get_args();
    if (args.contains("module_name")) {
        try {
            module_name = args["module_name"].as<std::string>();
        } catch (...) {
            // 如果转换失败，使用默认的 g_module_name
        }
    }

    // 格式化：time_str module_name level_str: file_name(line_str) : msg.get_message()
    std::string result;
    result.reserve(512); // 预分配空间

    result.append(time_str);
    result.push_back(' ');
    result.append(module_name);
    result.push_back(' ');
    result.append(level_str);
    result.append(": ");
    result.append(file_name);
    result.push_back('(');
    result.append(line_str);
    result.append(") : ");
    // 过滤无效字符，避免发送包含控制字符的内容
    std::string message_str = msg.get_message();
    logging::filter_invalid_chars(message_str);
    result.append(message_str);

    return result;
}

} // namespace log
} // namespace mc

MC_REGISTER_APPENDER(mc::log::socket_appender)
