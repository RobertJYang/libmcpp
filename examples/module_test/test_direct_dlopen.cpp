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
#include <dlfcn.h>
#include <iostream>
#include <thread>

int main() {
    const char* devices_lib_path = "./examples/module_test/modules/mc/devices.so";
    const char* network_lib_path = "./examples/module_test/modules/mc/network.so";

    std::cout << "=== 直接 dlopen/dlclose 测试 ===" << std::endl;

    // 测试 devices.so
    std::cout << "\n--- 测试 devices.so ---" << std::endl;
    std::cout << "调用 dlopen(" << devices_lib_path << ")..." << std::endl;
    void* devices_handle = dlopen(devices_lib_path, RTLD_NOW | RTLD_LOCAL);

    if (!devices_handle) {
        std::cerr << "无法加载 " << devices_lib_path << ": " << dlerror() << std::endl;
    } else {
        std::cout << "✓ 成功加载 " << devices_lib_path << std::endl;

        // 等待一下确保构造函数完成
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        std::cout << "调用 dlclose()..." << std::endl;
        std::cout.flush(); // 确保输出

        int result = dlclose(devices_handle);
        if (result != 0) {
            std::cerr << "dlclose 失败: " << dlerror() << std::endl;
        } else {
            std::cout << "✓ dlclose 成功" << std::endl;
        }

        // 强制刷新并等待一下，看看析构函数是否被调用
        std::cout.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "等待析构函数完成..." << std::endl;
    }

    // 测试 network.so
    std::cout << "\n--- 测试 network.so ---" << std::endl;
    std::cout << "调用 dlopen(" << network_lib_path << ")..." << std::endl;
    void* network_handle = dlopen(network_lib_path, RTLD_NOW | RTLD_LOCAL);

    if (!network_handle) {
        std::cerr << "无法加载 " << network_lib_path << ": " << dlerror() << std::endl;
    } else {
        std::cout << "✓ 成功加载 " << network_lib_path << std::endl;

        // 等待一下确保构造函数完成
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        std::cout << "调用 dlclose()..." << std::endl;
        std::cout.flush(); // 确保输出

        int result = dlclose(network_handle);
        if (result != 0) {
            std::cerr << "dlclose 失败: " << dlerror() << std::endl;
        } else {
            std::cout << "✓ dlclose 成功" << std::endl;
        }

        // 强制刷新并等待一下，看看析构函数是否被调用
        std::cout.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "等待析构函数完成..." << std::endl;
    }

    std::cout << "\n=== 直接 dlopen/dlclose 测试完成 ===" << std::endl;
    std::cout << "如果看到 '[MODULE] 动态库真正卸载!' 消息，说明 __attribute__((destructor)) 工作正常" << std::endl;

    return 0;
}