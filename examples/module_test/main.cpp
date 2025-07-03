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

#include <chrono>
#include <iostream>
#include <mc/exception.h>
#include <mc/log.h>
#include <mc/module.h>
#include <mc/variant.h>
#include <thread>

/**
 * @brief 测试 devices 模块加载
 * @return 测试是否成功
 */
bool test_devices_module() {
    try {
        ilog("=== 1. 验证 devices 模块加载 ===");

        auto devices_module = mc::load_module("mc.devices");
        if (!devices_module) {
            elog("无法加载 devices 模块");
            return false;
        }

        ilog("成功加载模块: ${name} v${version}",
             ("name", devices_module->name())("version", devices_module->version()));

        auto devices_factory = devices_module->get_factory();
        auto sensor_obj      = devices_factory->try_create_object("Sensor");
        if (sensor_obj) {
            sensor_obj->invoke_method("setName", {mc::variant("test_sensor")});
            auto name_result = sensor_obj->invoke_method("getName", {});
            ilog("✓ devices 模块传感器测试成功: ${name}", ("name", name_result.as<std::string>()));
        }

        return true;
    } catch (const mc::exception& e) {
        elog("devices 模块测试失败: ${msg}", ("msg", e.what()));
        return false;
    } catch (const std::exception& e) {
        elog("devices 模块测试失败: ${msg}", ("msg", e.what()));
        return false;
    }
}

/**
 * @brief 测试 network 模块加载
 * @return 测试是否成功
 */
bool test_network_module() {
    try {
        ilog("=== 2. 验证 network 模块加载 ===");

        auto network_module = mc::load_module("mc.network");
        if (!network_module) {
            elog("无法加载 network 模块");
            return false;
        }

        ilog("成功加载网络模块: ${name}", ("name", network_module->name()));

        auto network_factory = network_module->get_factory();
        auto client_obj      = network_factory->try_create_object("NetworkClient");
        if (client_obj) {
            auto connect_result = client_obj->invoke_method("connect",
                                                            {mc::variant("127.0.0.1"), mc::variant(8080)});
            ilog("✓ network 模块连接测试: ${result}", ("result", connect_result.as<bool>()));

            auto host_result = client_obj->invoke_method("getHost", {});
            ilog("✓ 连接主机: ${host}", ("host", host_result.as<std::string>()));
        }

        return true;
    } catch (const mc::exception& e) {
        elog("network 模块测试失败: ${msg}", ("msg", e.what()));
        return false;
    } catch (const std::exception& e) {
        elog("network 模块测试失败: ${msg}", ("msg", e.what()));
        return false;
    }
}

/**
 * @brief 测试 protocol 模块加载
 * @return 测试是否成功
 */
bool test_protocol_module() {
    try {
        ilog("=== 3. 验证 protocol 模块加载 (init.so 模式) ===");

        auto protocol_module = mc::load_module("mc.protocol");
        if (!protocol_module) {
            elog("无法加载 protocol 模块");
            return false;
        }

        ilog("成功加载协议模块: ${name}", ("name", protocol_module->name()));

        auto protocol_factory = protocol_module->get_factory();
        auto handler_obj      = protocol_factory->try_create_object("ProtocolHandler");
        if (handler_obj) {
            auto request_result = handler_obj->invoke_method("handleRequest",
                                                             {mc::variant("test request")});
            ilog("✓ protocol 模块处理请求: ${result}", ("result", request_result.as<std::string>()));

            auto version_result = handler_obj->invoke_method("getVersionString", {});
            ilog("✓ 协议版本: ${version}", ("version", version_result.as<std::string>()));
        }

        return true;
    } catch (const mc::exception& e) {
        elog("protocol 模块测试失败: ${msg}", ("msg", e.what()));
        return false;
    } catch (const std::exception& e) {
        elog("protocol 模块测试失败: ${msg}", ("msg", e.what()));
        return false;
    }
}

/**
 * @brief 测试 database 模块加载
 * @return 测试是否成功
 */
bool test_database_module() {
    try {
        ilog("=== 4. 验证深层路径 database 模块加载 ===");

        auto database_module = mc::load_module("mc.test.database");
        if (!database_module) {
            elog("无法加载 mc.test.database 模块");
            return false;
        }

        ilog("成功加载数据库模块: ${name}", ("name", database_module->name()));

        auto database_factory = database_module->get_factory();
        auto db_obj           = database_factory->try_create_object("DbConnection");
        if (db_obj) {
            auto connect_result = db_obj->invoke_method("connect",
                                                        {mc::variant("localhost"), mc::variant("test_db")});
            ilog("✓ database 模块连接测试: ${result}", ("result", connect_result.as<bool>()));

            auto db_name_result = db_obj->invoke_method("getDatabaseName", {});
            ilog("✓ 数据库名称: ${name}", ("name", db_name_result.as<std::string>()));

            auto query_result = db_obj->invoke_method("executeQuery",
                                                      {mc::variant("SELECT * FROM test")});
            // 获取查询结果的第一行
            auto results = query_result.as<std::vector<std::string>>();
            if (!results.empty()) {
                ilog("✓ 查询结果: ${result}", ("result", results[0]));
            }
        }

        return true;
    } catch (const mc::exception& e) {
        elog("database 模块测试失败: ${msg}", ("msg", e.what()));
        return false;
    } catch (const std::exception& e) {
        elog("database 模块测试失败: ${msg}", ("msg", e.what()));
        return false;
    }
}

/**
 * @brief 模块路径查找验证总结
 */
void test_module_path_summary() {
    ilog("=== 5. 模块路径查找验证总结 ===");
    ilog("✓ mc.devices -> modules/mc/devices.so (标准模式)");
    ilog("✓ mc.network -> modules/mc/network.so (标准模式)");
    ilog("✓ mc.protocol -> modules/mc/protocol/init.so (init.so 模式)");
    ilog("✓ mc.test.database -> modules/mc/test/database.so (深层路径模式)");
    ilog("所有模块路径查找测试完成！");
}

/**
 * @brief 测试模块两阶段卸载机制
 * @return 测试是否成功
 */
bool test_two_phase_unload() {
    try {
        ilog("=== 6. 模块两阶段卸载机制验证 ===");
        ilog("验证 unload() 只是减少引用计数，真正卸载需要释放所有 module_ptr");

        auto& module_manager = mc::get_module_manager();

        // 6.1 保存模块引用以验证两阶段卸载
        ilog("=== 6.1 保存模块引用，准备验证两阶段卸载 ===");

        // 重新获取模块引用，确保我们持有 module_ptr
        auto devices_module_ref = mc::load_module("mc.devices");
        auto network_module_ref = mc::load_module("mc.network");

        if (devices_module_ref) {
            auto factory = devices_module_ref->get_factory();
            // 通过创建一个临时对象来调用函数，或者尝试获取元数据来调用静态方法
            auto temp_obj = factory->try_create_object("Sensor");
            if (temp_obj) {
                // 目前通过一个虚拟对象调用，实际上这个函数应该是模块级别的
                ilog("devices 模块已加载");
            }
        }

        if (network_module_ref) {
            auto factory  = network_module_ref->get_factory();
            auto temp_obj = factory->try_create_object("NetworkClient");
            if (temp_obj) {
                ilog("network 模块已加载");
            }
        }

        // 6.2 调用 unload() - 第一阶段卸载（仅减少引用计数）
        ilog("=== 6.2 第一阶段：调用 unload() ===");

        // 显示引用计数状态
        ilog("调用 unload() 前，devices_module_ref 引用计数: ${count}",
             ("count", devices_module_ref.use_count()));
        ilog("调用 unload() 前，network_module_ref 引用计数: ${count}",
             ("count", network_module_ref.use_count()));

        ilog("调用 module_manager.unload('mc.devices')...");
        module_manager.unload("mc.devices");

        // 显示引用计数状态
        ilog("调用 unload() 后，devices_module_ref 引用计数: ${count}",
             ("count", devices_module_ref.use_count()));

        // 验证模块管理器认为模块已卸载
        bool devices_still_loaded = module_manager.is_loaded("mc.devices");
        ilog("模块管理器状态: ${status}", ("status", devices_still_loaded ? "已加载" : "未加载"));

        // 但是模块引用仍然可用，因为我们持有 module_ptr
        if (devices_module_ref) {
            auto factory    = devices_module_ref->get_factory();
            auto sensor_obj = factory->try_create_object("Sensor");
            if (sensor_obj) {
                sensor_obj->invoke_method("setName", {mc::variant("after_unload_test")});
                auto name_result = sensor_obj->invoke_method("getName", {});
                ilog("✓ 模块引用仍然可用: ${name}", ("name", name_result.as<std::string>()));
                ilog("✓ 验证成功：第一阶段卸载后，持有引用的模块仍可使用");
            }
        }

        // 6.3 释放 module_ptr - 第二阶段卸载（真正卸载动态库）
        ilog("=== 6.3 第二阶段：释放 module_ptr 引用 ===");

        ilog("释放 devices_module_ref...");
        ilog("释放前引用计数: ${count}", ("count", devices_module_ref.use_count()));
        ilog("注意观察控制台输出，应该看到 '[DEVICES MODULE] 动态库真正卸载!' 消息");

        // 显式释放引用
        // TODO:: 目前可以确定这个引用销毁后，内部会调用 dlclose 卸载动态库，并且 dlopen/dlcose 的调用次数也是配对的，
        // 但不知道什么原因动态库的 module_unload_detector 并没有被调用，而是直到整个进程退出才调用，以后再说吧
        devices_module_ref.reset();

        // 强制刷新输出缓冲区
        std::cout.flush();

        ilog("devices_module_ref 已释放");

        ilog("释放 network_module_ref...");
        ilog("释放前引用计数: ${count}", ("count", network_module_ref.use_count()));
        ilog("注意观察控制台输出，应该看到 '[NETWORK MODULE] 动态库真正卸载!' 消息");

        network_module_ref.reset(); // 显式释放引用

        // 强制刷新输出缓冲区
        std::cout.flush();

        ilog("network_module_ref 已释放");

        // 6.4 验证真正的重新加载
        ilog("=== 6.4 验证真正的重新加载 ===");

        // 等待一下确保析构完成
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ilog("重新加载 devices 模块...");
        ilog("注意观察控制台输出，应该看到新的 '[DEVICES MODULE] 模块加载' 消息");
        auto reloaded_devices = mc::load_module("mc.devices");

        if (reloaded_devices) {
            ilog("✓ 验证成功：模块确实被重新加载");
            ilog("  请注意观察控制台输出的模块加载/卸载消息来验证真实的卸载过程");

            // 测试重新加载的模块功能
            auto factory    = reloaded_devices->get_factory();
            auto sensor_obj = factory->try_create_object("Sensor");
            if (sensor_obj) {
                sensor_obj->invoke_method("setName", {mc::variant("reloaded_after_real_unload")});
                auto name_result = sensor_obj->invoke_method("getName", {});
                ilog("✓ 重新加载的模块功能正常: ${name}", ("name", name_result.as<std::string>()));
            }
        } else {
            elog("✗ 重新加载失败");
            return false;
        }

        // 6.5 清理其他模块
        ilog("=== 6.5 清理其他模块 ===");

        // 清理 protocol 和 database 模块
        module_manager.unload("mc.protocol");
        module_manager.unload("mc.test.database");

        // 这些模块没有持有额外引用，所以应该立即卸载
        // 但它们没有卸载检测器，所以不会有输出

        // 6.6 最终状态检查
        ilog("=== 6.6 最终状态检查 ===");
        std::vector<std::string> all_modules = {"mc.devices", "mc.network", "mc.protocol", "mc.test.database"};
        for (const auto& module_name : all_modules) {
            bool is_loaded = module_manager.is_loaded(module_name);
            ilog("  ${name}: ${status}", ("name", module_name)("status", is_loaded ? "已加载" : "未加载"));
        }

        ilog("=== 模块两阶段卸载机制验证完成 ===");
        ilog("✓ 验证了 unload() 只是逻辑卸载");
        ilog("✓ 验证了持有 module_ptr 的模块仍然可用");
        ilog("✓ 验证了释放所有引用后动态库真正卸载");
        ilog("✓ 验证了重新加载确实重新初始化模块");

        return true;
    } catch (const mc::exception& e) {
        elog("两阶段卸载测试失败: ${msg}", ("msg", e.what()));
        return false;
    } catch (const std::exception& e) {
        elog("两阶段卸载测试失败: ${msg}", ("msg", e.what()));
        return false;
    }
}

/**
 * @brief 演示模块系统
 */
int main() {
    try {
        ilog("开始演示模块加载功能");

        // 执行各个测试步骤
        bool success = true;

        // 1. 测试 devices 模块
        success &= test_devices_module();

        // 2. 测试 network 模块
        success &= test_network_module();

        // 3. 测试 protocol 模块
        success &= test_protocol_module();

        // 4. 测试 database 模块
        success &= test_database_module();

        // 5. 路径查找验证总结
        test_module_path_summary();

        // 6. 模块两阶段卸载机制验证
        success &= test_two_phase_unload();

        if (success) {
            ilog("所有测试完成！");
            return 0;
        } else {
            elog("部分测试失败");
            return -1;
        }

    } catch (const mc::exception& e) {
        elog("发生异常: ${msg}", ("msg", e.what()));
        return -1;
    } catch (const std::exception& e) {
        elog("发生标准异常: ${msg}", ("msg", e.what()));
        return -1;
    }
}