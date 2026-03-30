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

#ifndef MC_IO_FUTURE_H
#define MC_IO_FUTURE_H

#include <system_error>

#include <mc/future.h>
#include <mc/io/tcp_acceptor.h>
#include <mc/io/tcp_socket.h>
#include <mc/memory.h>

namespace mc::io {

MC_API mc::future<void> connect(tcp_socket& socket, mc::string_view address, uint16_t port);

MC_API mc::future<mc::shared_ptr<tcp_socket>> accept(tcp_acceptor& acceptor);

} // namespace mc::io

#endif // MC_IO_FUTURE_H
