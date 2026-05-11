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

#pragma once

#include <mc/dict.h>
#include <mc/engine/property/processor.h>
#include <mc/engine/property/relation.h>

namespace mc::engine {

class abstract_object;

/**
 * @brief 同步属性处理器
 *
 * 处理格式为 "<=/object_name.property_name" 的同步属性
 */
class MC_API sync_property_processor : public property_processor_interface {
public:
    bool   matches(mc::string_view value_str) const override;
    void   process(property_helper* property, mc::string_view value_str) override;
    p_type get_property_type() const override;

    // 公开方法供其他处理器使用
    void hook_sync_properties(property_helper* property, mc::dict& relate_properties);

private:
    /**
     * @brief 处理单个同步属性
     */
    void hook_sync_property(property_helper* property, const mc::engine::relate_property& relate_property);

    /**
     * @brief 设置同步属性连接
     */
    void setup_sync_property_connection(property_helper* property, const mc::engine::relate_property& relate_property,
                                        abstract_object& target_object);

    /**
     * @brief 设置延迟同步连接
     */
    void setup_deferred_sync_connection(property_helper* property, const mc::engine::relate_property& relate_property);

    /**
     * @brief 处理单个对象的同步属性
     */
    void process_sync_properties_for_object(property_helper* property, mc::string_view object_name,
                                            const mc::dict& object_properties);

    /**
     * @brief 设置多属性同步连接
     */
    void setup_multi_sync_connection(property_helper* property, abstract_object& target_object,
                                     const mc::dict& object_properties);

    /**
     * @brief 设置延迟多属性同步连接
     */
    void setup_deferred_multi_sync_connection(property_helper* property, mc::string_view object_name,
                                              const mc::dict& object_properties);

    /**
     * @brief 更新同步属性值
     */
    void update_sync_value_from_function(property_helper* property);
};

} // namespace mc::engine