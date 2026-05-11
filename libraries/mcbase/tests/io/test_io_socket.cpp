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

#include <future>
#include <memory>
#include <string>

#include <mc/io.h>
#include <mc/runtime.h>
#include <test_utilities/base.h>

using namespace std::chrono_literals;

namespace {

class io_socket_test : public mc::test::TestWithRuntime {};

TEST_F(io_socket_test, tcp_socket_connect_and_transfer)
{
    mc::io::tcp_acceptor acceptor(mc::get_io_executor());
    acceptor.listen(0);

    auto     client = std::make_shared<mc::io::tcp_socket>(mc::get_io_executor());
    uint16_t port   = acceptor.local_port();

    auto server_done = std::make_unique<std::promise<void>>();
    auto server_wait = server_done->get_future();
    auto client_done = std::make_unique<std::promise<void>>();
    auto client_wait = client_done->get_future();

    auto received = std::make_shared<std::string>(5, '\0');

    acceptor.async_accept([received, server_done = std::move(server_done)](const std::error_code& ec,
                                                                           mc::io::tcp_socket socket) mutable {
        ASSERT_FALSE(ec);

        auto server_socket = std::make_shared<mc::io::tcp_socket>(std::move(socket));
        server_socket->async_read_some(received->data(), received->size(),
                                       [received, server_done = std::move(server_done), server_socket](
                                           const std::error_code& read_ec, std::size_t transferred) mutable {
                                           EXPECT_FALSE(read_ec);
                                           EXPECT_EQ(transferred, received->size());
                                           EXPECT_EQ(*received, "hello");
                                           server_socket->close();
                                           server_done->set_value();
                                       });
    });

    client->async_connect(
        "127.0.0.1", port,
        [client_done = std::move(client_done), payload = std::string("hello"), client](const std::error_code& ec) mutable {
            ASSERT_FALSE(ec);

            auto message = std::make_shared<std::string>(std::move(payload));
            client->async_write_some(
                message->data(), message->size(),
                [client_done = std::move(client_done), client, message](const std::error_code& write_ec,
                                                                        std::size_t transferred) mutable {
                    EXPECT_FALSE(write_ec);
                    EXPECT_EQ(transferred, message->size());
                    client->close();
                    client_done->set_value();
                });
        });

    EXPECT_EQ(server_wait.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(client_wait.wait_for(1s), std::future_status::ready);
}

} // namespace
