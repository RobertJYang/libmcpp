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
#include <mc/app/service.h>
#include <mc/dbus/validator.h>
#include <mc/exception.h>

namespace test_bus_name_invalid {

class stub_service : public mc::app::service {
public:
    explicit stub_service(mc::string name) : mc::app::service(std::move(name))
    {}

    bool on_start() override
    {
        return true;
    }

    bool on_stop() override
    {
        return true;
    }
};

TEST(bus_name_invalid_test, single_segment_name_is_rejected)
{
    EXPECT_FALSE(mc::dbus::validator::is_valid_bus_name("alpha"));
    EXPECT_THROW({ stub_service svc("alpha"); }, mc::exception);
}

TEST(bus_name_invalid_test, name_with_space_is_rejected)
{
    EXPECT_FALSE(mc::dbus::validator::is_valid_bus_name("mc test.alpha"));
    EXPECT_THROW({ stub_service svc("mc test.alpha"); }, mc::exception);
}

TEST(bus_name_invalid_test, dashed_name_is_rejected)
{
    EXPECT_FALSE(mc::dbus::validator::is_valid_bus_name("mc-test.alpha"));
    EXPECT_THROW({ stub_service svc("mc-test.alpha"); }, mc::exception);
}

TEST(bus_name_invalid_test, empty_name_is_rejected)
{
    EXPECT_FALSE(mc::dbus::validator::is_valid_bus_name(""));
    EXPECT_THROW({ stub_service svc(""); }, mc::exception);
}

TEST(bus_name_invalid_test, valid_multi_segment_name_is_accepted)
{
    EXPECT_TRUE(mc::dbus::validator::is_valid_bus_name("mc.test.alpha"));
    EXPECT_NO_THROW({ stub_service svc("mc.test.alpha"); });
}

} // namespace test_bus_name_invalid
