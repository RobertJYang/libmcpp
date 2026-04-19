/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_ENGINE_SERVICE_OPERATION_H
#define MC_ENGINE_SERVICE_OPERATION_H

#include <mc/dict.h>
#include <mc/string.h>
#include <mc/time.h>
#include <mc/variant.h>

#include <variant>

namespace mc::engine {

enum class service_operation_kind {
    register_object,
    unregister_object,
    update_property,
    invoke_method,
};

struct service_operation_target {
    mc::string endpoint;
    mc::string path;
    mc::string interface_name;
};

struct register_object_operation {
    mc::string path;
    mc::string class_name;
};

struct unregister_object_operation {
    mc::string path;
};

struct update_property_operation {
    mc::string path;
    mc::string interface_name;
    mc::string property_name;
    mc::variant value;
};

struct invoke_method_operation {
    service_operation_target target;
    mc::string               method_name;
    mc::string               signature;
    mc::variants             args;
    mc::dict                 context;
    mc::milliseconds         timeout{mc::minutes(10)};
};

using service_operation_payload =
    std::variant<register_object_operation, unregister_object_operation, update_property_operation,
                 invoke_method_operation>;

struct service_operation {
    service_operation_kind    kind;
    service_operation_payload payload;
};

} // namespace mc::engine

#endif // MC_ENGINE_SERVICE_OPERATION_H
