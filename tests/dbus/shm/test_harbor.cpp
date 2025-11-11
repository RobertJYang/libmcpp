/*
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* openUBMC is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan
* PSL v2. You may obtain a copy of Mulan PSL v2 at:
*         http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
* KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
* NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
* Mulan PSL v2 for more details.
*/

#include <mc/dbus/shm/harbor.h>

#include <thread>

#include <gtest/gtest.h>

using namespace mc::dbus;

class HarborTest : public ::testing::Test
{
protected:
    void SetUp() override
    {}

    void TearDown() override
    {}
};

TEST_F(HarborTest, test_harbor_get_instance)
{
    auto& harbor1 = harbor::get_instance();
    auto& harbor2 = harbor::get_instance();
    EXPECT_EQ(&harbor1, &harbor2);
}

TEST_F(HarborTest, test_harbor_set_harbor_name)
{
    auto& harbor_instance = harbor::get_instance();
    harbor_instance.set_harbor_name("test_harbor");
    EXPECT_EQ(harbor_instance.get_harbor_name(), "test_harbor");
}

TEST_F(HarborTest, test_harbor_set_harbor_name_if_empty)
{
    auto& harbor_instance = harbor::get_instance();
    harbor_instance.set_harbor_name("");
    harbor_instance.set_harbor_name_if_empty("test_harbor_if_empty");
    EXPECT_EQ(harbor_instance.get_harbor_name(), "test_harbor_if_empty");

    harbor_instance.set_harbor_name_if_empty("another_name");
    EXPECT_EQ(harbor_instance.get_harbor_name(), "test_harbor_if_empty");
}

TEST_F(HarborTest, test_harbor_start_stop)
{
    auto& harbor_instance = harbor::get_instance();
    harbor_instance.set_harbor_name("org.test.Harbor");

    try
    {
        harbor_instance.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        harbor_instance.stop();
    }
    catch (const std::exception& e)
    {
        harbor_instance.stop();
        GTEST_SKIP() << "harbor start failed: " << e.what()
                    << " (可能缺少共享内存环境)";
    }
}
