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

#ifndef MC_TEST_REFLECT_SUBHEADER_FIXTURE_H
#define MC_TEST_REFLECT_SUBHEADER_FIXTURE_H

#include <mc/reflect/reflect.h>

namespace test_reflect_subheaders {

class demo_type {
public:
    MC_REFLECTABLE("test.reflect.subheaders.demo_type");

    demo_type() = default;

    int get_value() const
    {
        return m_value;
    }

    int add(int delta)
    {
        return m_value + delta;
    }

    int m_value{7};
};

} // namespace test_reflect_subheaders

MC_REFLECT(test_reflect_subheaders::demo_type, (m_value)(get_value)(add))

#endif // MC_TEST_REFLECT_SUBHEADER_FIXTURE_H
