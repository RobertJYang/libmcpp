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

#ifndef MC_DEVICES_BASE_H
#define MC_DEVICES_BASE_H

#include <mc/module.h>

MC_MODULE(mc_devices)

namespace mc::devices {

/**
 * @brief 传感器状态枚举
 */
enum class sensor_status {
    INACTIVE, // 未激活
    ACTIVE,   // 正常工作
    ERROR     // 错误状态
};

} // namespace mc::devices

MC_REFLECTABLE("SensorStatus", mc::devices::sensor_status)

#endif // MC_DEVICES_BASE_H