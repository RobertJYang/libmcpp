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

#include <gtest/gtest.h>
#include <mc/app/application.h>
#include <mc/app/base_app.h>
#include <mc/engine.h>

namespace {

class test_base_app : public mc::app::base_app {
public:
    test_base_app() = default;
};

class test_application : public mc::app::application {
public:
    test_application() = default;
};

class base_app_test : public ::testing::Test {
protected:
    void SetUp() override
    {
        mc::engine::engine::reset_for_test();
    }

    void TearDown() override
    {
        mc::app::application::reset_for_test();
        mc::app::base_app::reset_for_test();
        mc::engine::engine::reset_for_test();
    }
};

TEST_F(base_app_test, base_app_get_and_instance_return_current_object)
{
    test_base_app app;

    EXPECT_EQ(mc::app::base_app::get(), &app);
    EXPECT_EQ(&mc::app::base_app::instance(), &app);
    EXPECT_EQ(mc::app::application::get(), nullptr);
}

TEST_F(base_app_test, application_get_and_instance_return_typed_current_object)
{
    test_application app;

    EXPECT_EQ(mc::app::base_app::get(), &app);
    EXPECT_EQ(mc::app::application::get(), &app);
    EXPECT_EQ(&mc::app::application::instance(), &app);
}

TEST_F(base_app_test, second_base_app_family_object_is_rejected)
{
    test_base_app first_app;

    EXPECT_THROW({ test_base_app second_app; }, std::exception);
}

} // namespace
