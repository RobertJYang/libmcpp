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
#include <mc/engine/base.h>
#include <mc/engine/service.h>
#include <mc/exception.h>

namespace mc::engine {

service* abstract_interface::get_service() const {
    auto owner = get_owner();
    if (!owner) {
        return nullptr;
    }

    return owner->get_service();
}

} // namespace mc::engine

MC_REFLECT(mc::engine::abstract_object,                           // 配置计算属性（只读）
           (MC_COMPUTED_PROPERTY("path", get_object_path))        // path
           (MC_COMPUTED_PROPERTY("class_name", get_class_name))   // class_name
           (MC_COMPUTED_PROPERTY("object_name", get_object_name)) // object_name
           (MC_COMPUTED_PROPERTY("position", get_position))       // position
           (MC_COMPUTED_PROPERTY("object_id", get_object_id))     // object_id
)