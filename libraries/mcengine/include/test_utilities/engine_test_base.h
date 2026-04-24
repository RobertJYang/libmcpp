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

#ifndef MC_TEST_UTILITIES_ENGINE_TEST_BASE_H
#define MC_TEST_UTILITIES_ENGINE_TEST_BASE_H

#include <mc/engine.h>

#include <test_utilities/base.h>

namespace mc {
namespace test {

class MC_API TestWithEngine : public TestWithRuntime {
protected:
    static mc::engine::engine& get_engine()
    {
        return mc::engine::get_engine();
    }

    static void SetUpTestSuite()
    {
        mc::engine::engine::reset_for_test();
        TestWithRuntime::SetUpTestSuite();
    }

    static void TearDownTestSuite()
    {
        TestWithRuntime::TearDownTestSuite();
        mc::engine::engine::reset_for_test();
    }
};

} // namespace test
} // namespace mc

#endif // MC_TEST_UTILITIES_ENGINE_TEST_BASE_H
