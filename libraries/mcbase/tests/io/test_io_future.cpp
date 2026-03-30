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

#include <mc/future.h>
#include <mc/io.h>
#include <test_utilities/base.h>

#include <memory>

namespace {

class io_future_test : public mc::test::TestWithRuntime {};

TEST_F(io_future_test, connect_future_and_accept_future_complete)
{
    mc::io::tcp_acceptor acceptor(mc::get_io_executor());
    acceptor.listen(0);

    mc::io::tcp_socket client(mc::get_io_executor());

    auto accepted  = mc::io::accept(acceptor);
    auto connected = mc::io::connect(client, "127.0.0.1", acceptor.local_port());

    EXPECT_NO_THROW(connected.get());

    auto server_socket = accepted.get();
    ASSERT_NE(server_socket, nullptr);
    EXPECT_TRUE(server_socket->is_open());
    EXPECT_TRUE(client.is_open());

    server_socket->close();
    client.close();
}

TEST_F(io_future_test, connect_future_propagates_connect_error)
{
    mc::io::tcp_socket client(mc::get_io_executor());

    auto connected = mc::io::connect(client, "127.0.0.1", 1);

    EXPECT_THROW(connected.get(), std::system_error);
}

} // namespace
