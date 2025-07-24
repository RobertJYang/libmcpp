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

#include "device_manager.h"
#include "sensor.h"
#include <iostream>
#include <mc/log.h>

namespace mc::devices {

// 模块加载计数器
static int   s_load_count     = 0;
static void* s_module_address = nullptr;

/**
 * @brief 模块加载时调用的构造函数
 */
__attribute__((constructor)) void module_load_detector() {
    // 记录模块加载
    ++s_load_count;
    s_module_address = (void*)&s_load_count; // 使用一个静态变量的地址作为模块地址标识
    std::cout << "[DEVICES MODULE] 模块加载 #" << s_load_count
              << " (地址: " << s_module_address << ")" << std::endl;
}

/**
 * @brief 模块卸载时调用的析构函数
 */
__attribute__((destructor)) void module_unload_detector() {
    // 记录模块卸载
    std::cout << "[DEVICES MODULE] 动态库真正卸载! 地址: "
              << s_module_address << std::endl;
}

} // namespace mc::devices

// 导出传感器状态枚举到模块
MC_MODULE_REFLECT_ENUM(mc_devices,
                       mc::devices::sensor_status,
                       (INACTIVE)(ACTIVE)(ERROR))

// 导出设备管理器类到模块
MC_MODULE_REFLECT(mc_devices,
                  mc::devices::device_manager,
                  ((create_sensor, "createSensor"))        // 创建传感器
                  ((sensor_count, "sensorCount"))          // 获取传感器数量
                  ((read_all_sensors, "readAllSensors"))   // 读取所有传感器数据
                  ((get_average_value, "getAverageValue")) // 获取平均值
                  ((list_sensors, "listSensors")))         // 获取传感器列表

// 导出传感器类到模块
MC_MODULE_REFLECT(mc_devices,
                  mc::devices::sensor,
                  ((set_name, "setName"))      // 设置名称
                  ((get_name, "getName"))      // 获取名称
                  ((initialize, "initialize")) // 初始化
                  ((read, "read"))             // 读取
                  ((get_value, "getValue"))    // 获取值
                  ((get_status, "getStatus"))  // 获取状态
                  ((self_test, "selfTest")))   // 自检

MC_EXPORT_MODULE(mc_devices)
