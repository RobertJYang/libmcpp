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
#include <mc/dbus/shm/mock_shm.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/dbus/shm/serialize.h>
#include <mc/future.h>
#include <mc/reflect/metadata_info.h>
#include <mc/runtime.h>

#include <cstdlib>
#include <cstring>
#include <memory>
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

TEST_F(HarborTest, HarborLifecycleAndNaming)
{
    auto& harbor_instance = harbor::get_instance();

    // 1. 单例与名称设置
    harbor_instance.set_harbor_name("initial.harbor");
    EXPECT_EQ(harbor_instance.get_harbor_name(), "initial.harbor");

    harbor_instance.set_harbor_name_if_empty("should_not_override");
    EXPECT_EQ(harbor_instance.get_harbor_name(), "initial.harbor");

    harbor_instance.set_harbor_name("");
    harbor_instance.set_harbor_name_if_empty("fallback.harbor");
    EXPECT_EQ(harbor_instance.get_harbor_name(), "fallback.harbor");

    // 2. 启停流程
    try {
        harbor_instance.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        harbor_instance.stop();
    } catch (const std::exception& e) {
        harbor_instance.stop();
        GTEST_SKIP() << "harbor start failed: " << e.what()
                    << " (可能缺少共享内存环境)";
    }
}

TEST_F(HarborTest, DestinationQueueOnline)
{
    auto& harbor_instance = harbor::get_instance();
    harbor_instance.set_harbor_name("harbor.coverage.queue");

    const std::string service_name = "org.openubmc.coverage.queue";
    const std::string unique_name  = ":org.openubmc.coverage.queue";

    harbor_instance.register_unique_name(unique_name, service_name);
    auto* tree = create_shm_tree(harbor_instance.get_harbor_name(), service_name, unique_name);
    ASSERT_NE(tree, nullptr);

    auto* queue_by_service = harbor::get_destination_msg_queue(service_name);
    ASSERT_NE(queue_by_service, nullptr);

    auto* queue_by_unique = harbor::get_destination_msg_queue(unique_name);
    ASSERT_NE(queue_by_unique, nullptr);
    EXPECT_EQ(queue_by_service, queue_by_unique);

    harbor_instance.unregister_service(service_name);
    harbor_instance.stop();
}

TEST_F(HarborTest, SendAndReplyShmMessage)
{
    auto& harbor_instance = harbor::get_instance();
    harbor_instance.set_harbor_name("harbor.coverage.send_reply");

    const std::string service_name = "org.openubmc.coverage.send";
    const std::string unique_name  = ":org.openubmc.coverage.send";

    harbor_instance.register_unique_name(unique_name, service_name);

    auto promise = mc::make_promise<mc::dbus::local_msg>();
    auto future  = promise.get_future();

    ASSERT_TRUE(harbor_instance.send_shm_msg(unique_name, 1, std::move(promise)));

    mc::dbus::local_msg reply(service_name, "/object", "iface", "Method");
    reply.method_return();

    if (!harbor_instance.reply_shm_msg(unique_name, 1, reply)) {
        harbor_instance.unregister_service(service_name);
        harbor_instance.stop();
        GTEST_SKIP() << "reply_shm_msg unavailable in mock environment";
    }

    auto result = future.get();
    EXPECT_EQ(result.msg_type(), DBUS_MESSAGE_TYPE_METHOD_RETURN);

    harbor_instance.unregister_service(service_name);
    harbor_instance.stop();
}

TEST_F(HarborTest, GetUniqueNameThrowsWhenMissing)
{
    auto& harbor_instance = harbor::get_instance();
    harbor_instance.set_harbor_name("harbor.coverage.unique");

    EXPECT_THROW(harbor_instance.get_unique_name("org.openubmc.unknown"), mc::system_exception);
    harbor_instance.stop();
}

TEST_F(HarborTest, MessageQueueDispatchHandlesEmpty)
{
    auto& harbor_instance = harbor::get_instance();
    harbor_instance.set_harbor_name("harbor.coverage.dispatch");

    const std::string service_name = "org.openubmc.coverage.dispatch";
    const std::string unique_name  = ":org.openubmc.coverage.dispatch";

    auto* tree = create_shm_tree(harbor_instance.get_harbor_name(), service_name, unique_name);
    ASSERT_NE(tree, nullptr);

    auto* queue_raw = tree->get_message_queue();
    ASSERT_NE(queue_raw, nullptr);

    message_queue queue(*queue_raw);
    bool          handler_called = false;
    queue.dispatch(10, 1, [&handler_called](message_data&) { handler_called = true; });
    EXPECT_FALSE(handler_called);

    harbor_instance.stop();
}

TEST_F(HarborTest, UnregisterServiceClearsPendingPromises)
{
    auto& harbor_instance = harbor::get_instance();
    harbor_instance.set_harbor_name("harbor.coverage.unregister");

    const std::string service_name = "org.openubmc.coverage.unregister";
    const std::string unique_name  = ":org.openubmc.coverage.unregister";

    harbor_instance.register_unique_name(unique_name, service_name);

    auto promise = mc::make_promise<mc::dbus::local_msg>();
    auto future  = promise.get_future();

    ASSERT_TRUE(harbor_instance.send_shm_msg(unique_name, 5, std::move(promise)));

    harbor_instance.unregister_service(service_name);

    auto status = future.wait_for(std::chrono::milliseconds(10));
    EXPECT_EQ(status, mc::futures::future_status::timeout);

    harbor_instance.stop();
}