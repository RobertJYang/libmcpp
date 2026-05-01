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

// 导出传感器状态枚举到模块
MC_MODULE_REFLECT_ENUM(mc_devices, mc::devices::sensor_status, (INACTIVE)(ACTIVE)(ERROR))

// 导出设备管理器类到模块
MC_MODULE_REFLECT(mc_devices, mc::devices::device_manager,
                  ((create_sensor, "createSensor"))        // 创建传感器
                  ((sensor_count, "sensorCount"))          // 获取传感器数量
                  ((read_all_sensors, "readAllSensors"))   // 读取所有传感器数据
                  ((get_average_value, "getAverageValue")) // 获取平均值
                  ((list_sensors, "listSensors")))         // 获取传感器列表

// 导出传感器类到模块
MC_MODULE_REFLECT(mc_devices, mc::devices::sensor,
                  ((set_name, "setName"))      // 设置名称
                  ((get_name, "getName"))      // 获取名称
                  ((initialize, "initialize")) // 初始化
                  ((read, "read"))             // 读取
                  ((get_value, "getValue"))    // 获取值
                  ((self_test, "selfTest")))   // 自检

MC_EXPORT_MODULE(mc_devices)
