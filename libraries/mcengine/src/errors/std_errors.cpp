/**
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

#include <mc/engine/errors/std_errors.h>

namespace mc::engine::errors {
REGISTER_CONST_ERROR(internal_error, "mc.engine.internal_error", "Internal engine error");
REGISTER_CONST_ERROR(not_supported, "mc.engine.not_supported", "Operation not supported");
REGISTER_CONST_ERROR(invalid_args, "mc.engine.invalid_args", "Invalid arguments");
REGISTER_CONST_ERROR(unknown_method, "mc.engine.unknown_method", "Unknown method ${method}");
REGISTER_CONST_ERROR(unknown_object, "mc.engine.unknown_object", "Unknown object ${path}");
REGISTER_CONST_ERROR(unknown_interface, "mc.engine.unknown_interface", "Unknown interface ${interface}");
REGISTER_CONST_ERROR(unknown_property, "mc.engine.unknown_property", "Unknown property ${property}");
REGISTER_CONST_ERROR(property_read_only, "mc.engine.property_read_only", "Property ${property} is read-only");

} // namespace mc::engine::errors
