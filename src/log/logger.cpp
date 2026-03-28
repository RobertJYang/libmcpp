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

#include <dlfcn.h>
#include <logging_internal.h>
#include <mc/exception.h>
#include <mc/fmt/format_dict.h>
#include <mc/log/log_manager.h>
#include <mc/log/logger.h>
#include <mc/log.h>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include "securec.h"

#include <sys/wait.h>

extern "C" char** environ;

constexpr uint32_t LOG_US_TIME = 0x01;

namespace mc {
namespace log {

namespace {

// 周期节流状态：与 logger 配置分离，允许 clone 出来的临时 logger 共享节流信息
struct throttle_state {
    std::unordered_map<std::string, std::chrono::system_clock::time_point> m_last_log_time_map;
    std::mutex                                                             m_log_mutex;
};

} // namespace

// 检查串口输出内容是否含 shell 元字符，避免注入
static bool check_shell_special_character_s(std::string_view s)
{
    static constexpr const char* const k_needles[] = {
        "||", ";", "&&", "$", "|", "&", ">>", ">", "<", "`", "\\", "!", "\n",
    };
    for (size_t i = 0; i < sizeof(k_needles) / sizeof(k_needles[0]); ++i) {
        if (s.find(k_needles[i]) != std::string_view::npos) {
            elog("cmd_str includes special character ${token} of shell command",
                ("token", std::string(k_needles[i])));
            return false;
        }
    }
    return true;
}

// 日志记录器实现类
class logger::impl {
public:
    logger_config                   m_config;         // 日志记录器配置
    std::vector<appender_ptr>       m_appenders;      // 日志追加器映射
    int                             system_id;        // 系统ID，用于在日志中打印
    int                             period_s;         // 日志打印间隔（秒），0表示不限制
    std::shared_ptr<throttle_state> m_throttle_state; // 周期节流状态（可共享）

    impl(const std::string& name = MC_LOG_DEFAULT_LOGGER)
        : m_config(name), system_id(-1), period_s(0), m_throttle_state(std::make_shared<throttle_state>())
    {}

    impl(const impl& other)
        : m_config(other.m_config), m_appenders(other.m_appenders), system_id(other.system_id),
          period_s(other.period_s), m_throttle_state(other.m_throttle_state)
    {
        // 说明：节流状态与 logger 配置分离，clone 需要共享节流信息才能支持链式临时 logger 的周期限制
    }

    impl(impl&& other) noexcept
        : m_config(std::move(other.m_config)), m_appenders(std::move(other.m_appenders)), system_id(other.system_id),
          period_s(other.period_s), m_throttle_state(std::move(other.m_throttle_state))
    {}
};

// 静态方法实现
logger logger::get(const char* name)
{
    return log_manager::instance().get_logger(name);
}

logger::logger() : m_impl(std::make_shared<impl>())
{}

// 构造函数实现
logger::logger(const std::string& name) : m_impl(std::make_shared<impl>(name))
{}

logger::logger(const logger& other) : m_impl(other.m_impl)
{}

logger::logger(logger&& other) noexcept : m_impl(std::move(other.m_impl))
{}

// 赋值运算符实现
logger& logger::operator=(const logger& other)
{
    if (this != &other) {
        m_impl = other.m_impl;
    }
    return *this;
}

logger& logger::operator=(logger&& other) noexcept
{
    if (this != &other) {
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

// 方法实现
logger& logger::set_level(level lvl)
{
    m_impl->m_config.level = lvl;
    return *this;
}

level logger::get_level() const
{
    return m_impl->m_config.level;
}

void logger::set_name(const std::string& name)
{
    m_impl->m_config.name = name;
}

const std::string& logger::get_name() const
{
    return m_impl->m_config.name;
}

bool logger::is_enabled(level lvl) const
{
    return static_cast<int>(lvl) >= static_cast<int>(m_impl->m_config.level);
}

bool logger::is_debug_log(log_category category) const
{
    return category == log_category::debug || category == log_category::serial_printf;
}

logger& logger::system(int system_id)
{
    m_impl->system_id = system_id;
    return *this;
}

logger& logger::period(int period_s)
{
    // 检查参数有效性：period_s 只能为正整数
    if (period_s <= 0) {
        MC_THROW(mc::invalid_arg_exception, "Invalid log period value");
    }

    // 不需要锁，因为 period_s 是原子操作，且每个 logger 实例独立拥有 impl
    m_impl->period_s = period_s;
    return *this;
}

logger& logger::condition(bool cond)
{
    m_impl->m_config.condition = cond;
    return *this;
}

logger logger::clone() const
{
    logger copy;
    // 深拷贝 impl，避免与原 logger 共享状态（system_id/period/节流状态等）
    copy.m_impl = std::make_shared<impl>(*m_impl);
    return copy;
}

void logger::raise(const std::string& fmt_template, const mc::dict& args)
{
    std::string msg;
    try {
        msg = mc::format_dict(fmt_template, args);
    } catch (const std::exception& e) {
        // 格式化失败时，降级为原始 fmt + 错误原因
        msg = fmt_template + " (format_error: " + e.what() + ")";
    }

    // 抛出运行时异常
    MC_THROW(mc::runtime_exception, msg);
}

static std::string make_period_log_key(int period_s, const message& msg)
{
    // 说明：
    // - 优先使用 format_template 作为标识（更稳定，避免同模板不同参数导致不同标识）
    // - 没有 format_template 时，使用已格式化 message（Lua 侧目前就是这种）
    // - 直接使用字符串组合，不使用 hash，更直观易调试
    const std::string& tmpl = msg.get_format_template();
    const std::string& base = tmpl.empty() ? msg.get_message() : tmpl;

    // 组合：消息标识 + 周期值，作为唯一标识
    return base + "|period:" + std::to_string(period_s);
}

bool logger::should_log_period(const message& msg)
{
    const int period_s = m_impl->period_s;
    if (period_s <= 0) {
        return true;
    }

    const auto        now = std::chrono::system_clock::now();
    const std::string key = make_period_log_key(period_s, msg);

    std::lock_guard<std::mutex> lock(m_impl->m_throttle_state->m_log_mutex);
    auto&                       last_time = m_impl->m_throttle_state->m_last_log_time_map[key];
    if (last_time == std::chrono::system_clock::time_point{}) {
        // 首次打印：默认构造的 time_point 表示未初始化
        last_time = now;
        return true;
    }

    // 使用毫秒精度进行比较
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
    const auto period_ms  = static_cast<int64_t>(period_s) * 1000; // 转换为毫秒
    if (elapsed_ms < period_ms) {
        return false;
    }
    last_time = now;
    return true;
}

void logger::log(message msg)
{
    if (!m_impl->m_config.condition) {
        return;
    }

    if (is_debug_log(msg.get_category()) && !is_enabled(msg.get_level())) {
        return;
    }

    // 仅当 logger 配置了 system_id 或 period 时，才修改 message 的 args（添加前缀/后缀用）
    // 未配置时本分支不执行，保持原有格式化逻辑不变，不影响未使用扩展能力的调用方
    if (m_impl->system_id != -1 || m_impl->period_s > 0) {
        // dict 为共享数据模型，修改前先 copy 出独立副本，避免影响其它持有同一 args 的对象
        msg.get_args() = msg.get_args().copy();
        if (m_impl->system_id != -1) {
            msg.get_args()["system_id"] = m_impl->system_id;
        }
        if (m_impl->period_s > 0) {
            msg.get_args()["period"] = m_impl->period_s;
        }
    }

    // 检查 period 时间间隔
    if (!should_log_period(msg)) {
        return;
    }

    // 输出日志
    for (const auto& appender : m_impl->m_appenders) {
        appender->append(msg);
    }
}

namespace {

constexpr size_t LOG_PRINTF_BUF_SIZE = 4096;

} // namespace

void logger::log_printf(level lvl, const char* fmt, std::va_list ap)
{
    if (!is_enabled(lvl)) {
        return;
    }

    char buf[LOG_PRINTF_BUF_SIZE];
    int  n = vsnprintf_s(buf, sizeof(buf), sizeof(buf), fmt, ap);
    if (n < 0) {
        return;
    }

    std::string formatted(buf);
    context     ctx("", "", 0);
    message     msg(lvl, formatted, ctx);
    msg.set_category(log_category::serial_printf);
    log(msg);
}

void logger::debug_printf(const char* fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    log_printf(level::debug, fmt, ap);
    va_end(ap);
}

void logger::info_printf(const char* fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    log_printf(level::info, fmt, ap);
    va_end(ap);
}

void logger::notice_printf(const char* fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    log_printf(level::notice, fmt, ap);
    va_end(ap);
}

void logger::warn_printf(const char* fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    log_printf(level::warn, fmt, ap);
    va_end(ap);
}

void logger::error_printf(const char* fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    log_printf(level::error, fmt, ap);
    va_end(ap);
}

// 在子进程中用 execve 执行 sh -c cmd，父进程等待子进程结束
static void run_shell_cmd(const std::string& cmd)
{
    const pid_t pid = fork();
    if (pid < 0) {
        return;
    }
    if (pid == 0) {
        char* const argv[] = {
            const_cast<char*>("/bin/sh"),
            const_cast<char*>("-c"),
            const_cast<char*>(cmd.c_str()),
            nullptr,
        };
        execve("/bin/sh", argv, ::environ);
        _exit(127);
    }
    (void)waitpid(pid, nullptr, 0);
}

typedef const char* (*get_log_time_str_func_t)(int);
static get_log_time_str_func_t get_log_time_str_ptr = nullptr;

void logger::log_serial_printf(level lvl, const message& msg)
{
    if (!m_impl->m_config.condition || !is_enabled(lvl)) {
        return;
    }

    std::string message_str = msg.get_message();
    // 过滤无效字符，避免输出控制字符导致终端显示异常
    logging::filter_invalid_chars(message_str);

    if (!get_log_time_str_ptr) {
        void* handle         = dlopen(LOGGING_PATH, RTLD_NOW);
        get_log_time_str_ptr = (get_log_time_str_func_t)dlsym(handle, "get_log_time_str_c");
    }
    std::string module_name = "Unknown";
    const auto& args        = msg.get_args();
    if (args.contains("module_name")) {
        try {
            module_name = args["module_name"].as<std::string>();
        } catch (...) {
        }
    }

    std::string file_str;
    file_str.reserve(64);
    context ctx = msg.get_context();
    if (ctx.m_file.empty()) {
        file_str.append("unknown");
    } else {
        file_str.append(mc::filesystem::basename(ctx.m_file));
    }

    if (get_log_time_str_ptr) {
        const char* time_str = get_log_time_str_ptr(LOG_US_TIME);
        if (time_str) {
            std::string str = mc::format_dict(
                "${time} ${module} ${level}: ${file}(${line}): ${message}",
                mc::dict()("time", time_str)("module", module_name)("level", mc::log::to_string(msg.get_level()))(
                    "file", file_str)("line", std::to_string(ctx.m_line))("message", message_str));
            if (!check_shell_special_character_s(str)) {
                return;
            }
            const std::string cmd = "echo \"" + str + "\" > /dev/ttyS0";
            run_shell_cmd(cmd);
        }
    }
}

void logger::add_appender(const appender_ptr& a)
{
    if (a) {
        m_impl->m_appenders.push_back(a);
    }
}

bool logger::remove_appender(const std::string& name)
{
    auto& appenders = m_impl->m_appenders;
    auto  it        = std::find_if(appenders.begin(), appenders.end(), [&name](const appender_ptr& a) {
        return a && a->get_name() == name;
    });

    if (it != appenders.end()) {
        appenders.erase(it);
        return true;
    }

    return false;
}

appender_ptr logger::find_appender(const std::string& name) const
{
    const auto& appenders = m_impl->m_appenders;
    auto        it        = std::find_if(appenders.begin(), appenders.end(), [&name](const appender_ptr& a) {
        return a && a->get_name() == name;
    });
    return (it != appenders.end()) ? *it : nullptr;
}

const std::vector<appender_ptr>& logger::get_appenders() const
{
    return m_impl->m_appenders;
}

void logger::clear_appenders()
{
    m_impl->m_appenders.clear();
}

} // namespace log
} // namespace mc

MC_REFLECT_ENUM(mc::log::level, (all)(trace)(debug)(info)(notice)(warn)(error)(fatal)(off))
