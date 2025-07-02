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

#ifndef MC_SENSOR_H
#define MC_SENSOR_H

#include "base.h"

namespace mc::devices {

/**
 * @brief 传感器类
 *
 */
class sensor {
public:
    sensor()  = default;
    ~sensor() = default;

    /**
     * @brief 设置传感器名称
     */
    void set_name(const std::string& name) {
        m_name = name;
    }

    /**
     * @brief 获取传感器名称
     */
    const std::string& get_name() const {
        return m_name;
    }

    /**
     * @brief 初始化传感器 - 脚本可调用
     */
    bool initialize() {
        m_status = sensor_status::ACTIVE;
        return true;
    }

    /**
     * @brief 读取传感器数据 - 脚本可调用
     */
    double read() {
        // 模拟读取数据
        m_value = 25.0 + (rand() % 100) / 10.0;
        return m_value;
    }

    /**
     * @brief 获取最后读取的值 - 脚本可调用
     */
    double get_value() const {
        return m_value;
    }

    /**
     * @brief 获取传感器状态 - 脚本可调用
     */
    sensor_status get_status() const {
        return m_status;
    }

    /**
     * @brief 传感器自检 - 脚本可调用
     */
    bool self_test() const {
        return m_status == sensor_status::ACTIVE;
    }

    // 公开成员变量以便反射访问
    std::string   m_name{"default_sensor"};
    double        m_value{0.0};
    sensor_status m_status{sensor_status::INACTIVE};
};

} // namespace mc::devices

// 导出传感器类到模块
MC_MODULE_REFLECT(mc_devices,
                  (mc::devices::sensor, "Sensor"),
                  ((set_name, "setName"))      // 设置名称
                  ((get_name, "getName"))      // 获取名称
                  ((initialize, "initialize")) // 初始化
                  ((read, "read"))             // 读取
                  ((get_value, "getValue"))    // 获取值
                  ((get_status, "getStatus"))  // 获取状态
                  ((self_test, "selfTest")))   // 自检

#endif // MC_SENSOR_H