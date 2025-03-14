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

#include <mc/core/plugin.h>
#include <mc/log.h>

namespace mc {
namespace example {

// 自定义日志 appender
class custom_appender : public log::appender {
public:
    explicit custom_appender(const std::string& name) {
        m_name = name;
    }

    ~custom_appender() override = default;

    void append(const log::message& msg) override {
        std::cout << "[CustomAppender] " << msg.get_message() << std::endl;
        std::cout.flush();
    }

    bool init(const variant& args) override {
        return true;
    }
};

// 创建自定义appender的工厂函数
static std::shared_ptr<log::appender> create_custom_appender() {
    return std::make_shared<custom_appender>("custom_appender");
}

// 插件类
class custom_appender_plugin : public plugin_base<custom_appender_plugin> {
public:
    static const char* plugin_name() {
        return "custom_appender";
    }

    bool initialize() override {
        // 注册自定义appender
        log::appender_factory::instance().register_creator("custom_appender",
                                                           create_custom_appender);
        return true;
    }

    void startup() override {
    }

    void shutdown() override {
    }
};

// 创建插件实例
extern "C" plugin* create_plugin() {
    return new custom_appender_plugin();
}

} // namespace example
} // namespace mc