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

    std::cout << "=== 测试 dlclose 是否立即触发析构函数 ===" << std::endl;

    std::cout << "\n1. 调用 dlopen..." << std::endl;
    void* handle = dlopen(devices_lib_path, RTLD_NOW | RTLD_LOCAL);

    if (!handle) {
        std::cerr << "无法加载 " << devices_lib_path << ": " << dlerror() << std::endl;
        return -1;
    }

    std::cout << "2. dlopen 成功" << std::endl;

    // 确保构造函数完成
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::cout << "\n3. 现在调用 dlclose..." << std::endl;
    std::cout.flush();

    int result = dlclose(handle);
    if (result != 0) {
        std::cerr << "dlclose 失败: " << dlerror() << std::endl;
        return -1;
    }

    std::cout << "4. dlclose 成功" << std::endl;
    std::cout.flush();

    std::cout << "\n5. 等待 1 秒，看看析构函数是否被调用..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "6. 等待完成" << std::endl;

    std::cout << "\n7. 再次调用 dlopen/dlclose..." << std::endl;
    handle = dlopen(devices_lib_path, RTLD_NOW | RTLD_LOCAL);
    if (handle) {
        std::cout << "8. 第二次 dlopen 成功" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        std::cout << "9. 第二次 dlclose..." << std::endl;
        std::cout.flush();

        dlclose(handle);
        std::cout << "10. 第二次 dlclose 成功" << std::endl;
        std::cout.flush();

        std::cout << "\n11. 再次等待 1 秒..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "12. 第二次等待完成" << std::endl;
    }

    std::cout << "\n=== 测试即将结束 ===" << std::endl;
    std::cout << "如果析构函数在 dlclose 后立即执行，应该在步骤 4 和 10 之后看到卸载消息" << std::endl;
    std::cout << "如果析构函数在程序结束时执行，应该在这条消息之后看到卸载消息" << std::endl;

    return 0;
}