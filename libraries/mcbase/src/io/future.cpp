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

#include <mc/io/future.h>

namespace mc::io {

mc::future<void> connect(tcp_socket& socket, mc::string_view address, uint16_t port)
{
    auto promise = mc::make_promise<void>(socket.get_executor());
    auto future  = promise.get_future();

    socket.async_connect(address, port, [promise = std::move(promise)](const std::error_code& ec) mutable {
        if (ec) {
            promise.set_exception(std::system_error(ec));
            return;
        }

        promise.set_value();
    });

    return future;
}

mc::future<mc::shared_ptr<tcp_socket>> accept(tcp_acceptor& acceptor)
{
    auto promise = mc::make_promise<mc::shared_ptr<tcp_socket>>(acceptor.get_executor());
    auto future  = promise.get_future();

    acceptor.async_accept([promise = std::move(promise)](const std::error_code& ec, tcp_socket socket) mutable {
        if (ec) {
            promise.set_exception(std::system_error(ec));
            return;
        }

        promise.set_value(mc::make_shared<tcp_socket>(std::move(socket)));
    });

    return future;
}

} // namespace mc::io
