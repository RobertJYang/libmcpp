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

#include "mc/core/application.h"
#include "mc/core/event.h"
#include "mc/core/object.h"
#include "mc/core/signal_slot.h"
#include "mc/signal_slot.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace mc;
using namespace mc::core;

// 自定义事件类型
class custom_event : public event {
public:
    explicit custom_event(const std::string& message)
        : event(event_type::user)
        , m_message(message) {}
    
    const std::string& message() const {
        return m_message;
    }
    
    std::shared_ptr<event> clone() const override {
        return std::make_shared<custom_event>(m_message);
    }
    
private:
    std::string m_message;
};

// 自定义服务类
class my_service : public object {
public:
    // 使用父类的构造函数
    using object::object;
    
    // 初始化方法
    bool initialize() {
        std::cout << "Initializing service: " << name() << std::endl;
        
        // 注册信号处理函数
        m_data_changed_signal.connect([this](int value) {
            std::cout << name() << ": Data changed to " << value << std::endl;
        });
        
        return true;
    }
    
    // 处理事件
    bool handle_event(std::shared_ptr<event> e) override {
        if (auto custom_evt = std::dynamic_pointer_cast<custom_event>(e)) {
            std::cout << name() << " received event: " << custom_evt->message() << std::endl;
            e->accept();
            return true;
        }
        
        return object::handle_event(e);
    }
    
    // 设置数据并触发信号
    void set_data(int value) {
        if (m_data != value) {
            m_data = value;
            m_data_changed_signal(value);
        }
    }
    
    // 获取数据变更信号
    mc::signal<void(int)>& data_changed_signal() {
        return m_data_changed_signal;
    }
    
private:
    int m_data = 0;
    mc::signal<void(int)> m_data_changed_signal;
};

// 自定义插件类
class my_plugin : public object {
public:
    // 使用父类的构造函数
    using object::object;
    
    // 初始化方法
    bool initialize() {
        std::cout << "Initializing plugin: " << name() << std::endl;
        
        // 创建一个服务作为子对象
        m_service = create<my_service>("plugin_service", io_context(), shared_from_this());
        m_service->initialize();
        
        // 连接服务的信号到插件的槽函数
        m_connection_manager.add(
            mc::core::connect_on_object(m_service->data_changed_signal(), shared_from_this(),
                [this](int value) {
                    std::cout << name() << ": Received data change from service: " << value << std::endl;
                }
            )
        );
        
        return true;
    }
    
    // 处理事件
    bool handle_event(std::shared_ptr<event> e) override {
        std::cout << name() << " handling event" << std::endl;
        
        // 事件过滤和处理逻辑
        if (e->type() == event::event_type::user) {
            std::cout << name() << " forwarding event to service" << std::endl;
            m_service->post_event(e);
            return true;
        }
        
        return object::handle_event(e);
    }
    
    // 更新服务数据
    void update_service_data(int value) {
        if (m_service) {
            // 在服务的执行器上执行数据更新
            post_to_object(m_service, [service = m_service, value]() {
                service->set_data(value);
            });
        }
    }
    
private:
    std::shared_ptr<my_service> m_service;
    mc::connection_manager m_connection_manager;
};

// 主函数
int main(int argc, char** argv) {
    // 初始化应用程序
    application& app = mc::app();
    app.initialize(argc, argv);
    
    std::cout << "Application object name: " << app.name() << std::endl;
    
    // 创建插件对象
    auto plugin = app.create_subsystem<my_plugin>("my_plugin");
    plugin->initialize();
    
    // 启动应用程序
    app.start();
    
    // 创建一个线程定期更新服务数据
    std::thread update_thread([plugin]() {
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 在插件的执行器上执行更新
            post_to_object(plugin, [plugin, i]() {
                plugin->update_service_data(i * 10);
            });
        }
    });
    
    // 发送自定义事件到插件
    std::shared_ptr<custom_event> evt = std::make_shared<custom_event>("Hello from main application");
    global_event_dispatcher()->dispatch(evt, plugin);
    
    // 等待更新线程完成
    update_thread.join();
    
    // 等待一段时间，让所有异步操作完成
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 停止应用程序
    app.stop();
    app.cleanup();
    
    return 0;
} 