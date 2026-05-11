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

#include <algorithm>
#include <mc/engine/property/helper.h>
#include <mc/engine/property/processor.h>

namespace mc::engine {

property_processor_factory& property_processor_factory::get_instance()
{
    static property_processor_factory instance;
    return instance;
}

void property_processor_factory::register_processor(std::unique_ptr<property_processor_interface> processor)
{
    m_processors.push_back(std::move(processor));
}

bool property_processor_factory::process_property_value(property_helper* property, mc::string_view value_str)
{
    for (auto& processor : m_processors) {
        if (processor->matches(value_str)) {
            processor->process(property, value_str);
            return true;
        }
    }
    return false;
}

} // namespace mc::engine