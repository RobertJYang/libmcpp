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

#ifndef MC_DBUS_TRANSPORT_H
#define MC_DBUS_TRANSPORT_H

#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <mc/dbus/address.h>
#include <mc/dbus/message.h>

namespace mc::dbus {

/**
 * @brief DBus传输层抽象类
 *
 * 定义了DBus不同传输方式的抽象接口。支持Unix套接字、TCP等多种传输通道。
 */
class transport {
public:
    using error_code      = boost::system::error_code;
    using io_context_type = boost::asio::io_context;
    using buffer          = std::string_view;
    using writable_buffer = mc::io::io_stream::writeable_data;
    using read_handler    = std::function<void(const error_code&, std::size_t)>;
    using write_handler   = std::function<void(const error_code&, std::size_t)>;
    using connect_handler = std::function<void(const error_code&)>;

    explicit transport(io_context_type& io_context, address_entry_ptr entry);
    virtual ~transport();

    virtual void connect(connect_handler handler)                = 0;
    virtual void close()                                         = 0;
    virtual void write(buffer buf, write_handler handler)        = 0;
    virtual void read(writable_buffer buf, read_handler handler) = 0;

protected:
    io_context_type&  m_io_context; ///< IO上下文引用
    address_entry_ptr m_entry;      ///< 地址条目
};

using transport_ptr = std::unique_ptr<transport>;

} // namespace mc::dbus

#endif // MC_DBUS_TRANSPORT_H