#ifndef EXAMPLE_PLUGIN_H
#define EXAMPLE_PLUGIN_H

#include "mc/core/plugin.h"
#include <iostream>

namespace examples {

/**
 * @brief 示例插件类
 */
class example_plugin : public mc::plugin_base<example_plugin> {
public:
    static const char* plugin_name() {
        return "example";
    }

    bool initialize() override {
        std::cout << "初始化示例插件" << std::endl;
        return true;
    }

    void startup() override {
        std::cout << "启动示例插件" << std::endl;
    }

    void shutdown() override {
        std::cout << "关闭示例插件" << std::endl;
    }
};

/**
 * @brief 基础服务插件
 */
class base_service_plugin : public mc::plugin_base<base_service_plugin> {
public:
    static const char* plugin_name() {
        return "base_service";
    }

    bool initialize() override {
        std::cout << "初始化基础服务插件" << std::endl;
        return true;
    }

    void startup() override {
        std::cout << "启动基础服务插件" << std::endl;
    }

    void shutdown() override {
        std::cout << "关闭基础服务插件" << std::endl;
    }
};

/**
 * @brief 网络插件，依赖于基础服务插件
 */
class network_plugin : public mc::plugin_base<network_plugin> {
public:
    network_plugin() {
        // 添加对基础服务插件的依赖
        depends_on(base_service_plugin::plugin_name());
    }

    static const char* plugin_name() {
        return "network";
    }

    bool initialize() override {
        std::cout << "初始化网络插件" << std::endl;
        return true;
    }

    void startup() override {
        std::cout << "启动网络插件" << std::endl;
    }

    void shutdown() override {
        std::cout << "关闭网络插件" << std::endl;
    }
};

/**
 * @brief 数据库插件，依赖于基础服务插件
 */
class database_plugin : public mc::plugin_base<database_plugin> {
public:
    database_plugin() {
        // 添加对基础服务插件的依赖
        depends_on(base_service_plugin::plugin_name());
    }

    static const char* plugin_name() {
        return "database";
    }

    bool initialize() override {
        std::cout << "初始化数据库插件" << std::endl;
        return true;
    }

    void startup() override {
        std::cout << "启动数据库插件" << std::endl;
    }

    void shutdown() override {
        std::cout << "关闭数据库插件" << std::endl;
    }
};

/**
 * @brief API插件，依赖于网络插件和数据库插件
 */
class api_plugin : public mc::plugin_base<api_plugin> {
public:
    api_plugin() {
        // 添加对网络插件和数据库插件的依赖
        depends_on(network_plugin::plugin_name()).depends_on(database_plugin::plugin_name());
    }

    static const char* plugin_name() {
        return "api";
    }

    bool initialize() override {
        std::cout << "初始化API插件" << std::endl;
        return true;
    }

    void startup() override {
        std::cout << "启动API插件" << std::endl;
    }

    void shutdown() override {
        std::cout << "关闭API插件" << std::endl;
    }
};

} // namespace examples

#endif // EXAMPLE_PLUGIN_H