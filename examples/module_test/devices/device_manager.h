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

#ifndef MC_DEVICE_MANAGER_H
#define MC_DEVICE_MANAGER_H

#include "base.h"
#include "sensor.h"

namespace mc::devices {

/**
 * @brief 设备管理器 - C++实现，导出给脚本使用
 *
 * 提供设备管理功能，可在expr2脚本中使用
 */
class device_manager {
public:
    MC_REFLECTABLE("DeviceManager")

    device_manager()  = default;
    ~device_manager() = default;

    /**
     * @brief 创建传感器 - 脚本可调用
     */
    void create_sensor(const std::string& name) {
        sensor s;
        s.set_name(name);
        s.initialize();
        m_sensors.push_back(s);
    }

    /**
     * @brief 获取传感器数量 - 脚本可调用
     */
    size_t sensor_count() const {
        return m_sensors.size();
    }

    /**
     * @brief 读取所有传感器数据 - 脚本可调用
     */
    void read_all_sensors() {
        for (auto& s : m_sensors) {
            s.read();
        }
    }

    /**
     * @brief 获取平均值 - 脚本可调用
     */
    double get_average_value() const {
        if (m_sensors.empty()) {
            return 0.0;
        }

        double sum = 0.0;
        for (const auto& s : m_sensors) {
            sum += s.get_value();
        }
        return sum / m_sensors.size();
    }

    /**
     * @brief 获取传感器列表 - 脚本可调用
     */
    std::vector<std::string> list_sensors() const {
        std::vector<std::string> names;
        for (const auto& s : m_sensors) {
            names.push_back(s.get_name());
        }
        return names;
    }

private:
    std::vector<sensor> m_sensors;
};

} // namespace mc::devices

#endif // MC_DEVICE_MANAGER_H