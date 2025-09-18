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

/**
 * @file property_processors.h
 * @brief 所有属性处理器的统一头文件
 * 
 * 包含所有属性处理器的头文件，方便使用者一次性导入所有处理器
 */

#include <mc/engine/property/processor.h>
#include <mc/engine/property/processors/ref_object_processor.h>
#include <mc/engine/property/processors/ref_property_processor.h>
#include <mc/engine/property/processors/sync_property_processor.h>
#include <mc/engine/property/processors/function_call_processor.h>

namespace mc::engine {

/**
 * @brief 注册所有属性处理器
 * 
 * 确保所有属性处理器都已注册到工厂中
 */
MC_API void register_property_processors();

} // namespace mc::engine 