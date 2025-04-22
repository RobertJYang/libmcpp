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

#include <mc/dbus/transport/transport.h>
#include <mc/log.h>

namespace mc::dbus {

transport::transport(io_context_type& io_context, address_entry_ptr entry)
    : m_io_context(io_context), m_entry(std::move(entry)) {
}

transport::~transport() {
}

std::size_t transport::write_all(buffer buf, error_code& ec) {
    if (!m_is_connected) {
        ec = boost::asio::error::not_connected;
        return 0;
    }

    std::cout << "准备写入 " << buf.length() << " 字节" << std::endl;
    for (size_t i = 0; i < buf.length(); i++) {
        printf("字节[%zu]: %02x\n", i, (unsigned char)buf.data()[i]);
    }

    std::size_t write_bytes = 0;
    while (write_bytes < buf.length()) {
        buffer      write_buf(buf.data() + write_bytes, buf.length() - write_bytes);
        std::size_t bytes = write_some(write_buf, ec);
        std::cout << "成功写入 " << bytes << " 字节" << std::endl;
        if (ec) {
            return 0;
        }

        write_bytes += bytes;
    }

    return write_bytes;
}

std::size_t transport::write_line(buffer buf, error_code& ec) {
    std::string buf_with_line(buf);
    buf_with_line += "\r\n";
    return write_all(buf_with_line, ec);
}

std::size_t transport::read_line(buffer buf, error_code& ec) {
    if (!m_is_connected) {
        ec = boost::asio::error::not_connected;
        return 0;
    }

    std::size_t read_bytes = 0;
    while (read_bytes < buf.length()) {
        buffer      read_buf(buf.data() + read_bytes, buf.length() - read_bytes);
        std::size_t bytes = read_some(read_buf, ec);
        if (ec) {
            if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
                std::cout << "读取错误: " << ec.message() << ", 错误码: " << ec.value()
                          << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                continue;
            }

            std::cout << "读取错误: " << ec.message() << ", 错误码: " << ec.value() << std::endl;
            return 0;
        }

        std::cout << "读取成功: " << bytes << std::endl;

        read_bytes += bytes;
        std::string_view line(buf.data(), read_bytes);
        if (line.find("\r\n") != std::string_view::npos) {
            return read_bytes;
        }
    }

    ec = boost::asio::error::not_found;
    return 0;
}

mc::future<transport::buffer> transport::async_write_all(buffer buf) {
    auto promise = mc::make_promise<std::string_view>(m_io_context);
    auto future  = promise.get_future();

    async_write(buf).then([this, buf, promise](const auto& result) mutable {
        if (result.first) {
            MC_THROW(mc::exception, "DBus写入失败: ${error}", ("error", result.first.message()));
        }

        if (result.second == buf.size()) {
            promise.set_value(buf);
            return;
        }

        auto remaining = buf.substr(result.second);
        async_write_all(remaining).then([promise, remaining](auto) mutable {
            promise.set_value(remaining);
        });
    });

    return future;
}

mc::future<transport::buffer>
transport::async_read_until_impl(std::string_view buf, std::size_t pos, std::string_view delim) {
    auto promise = mc::make_promise<buffer>(m_io_context);
    auto future  = promise.get_future();

    auto new_buf = buf.substr(pos);
    async_read(new_buf)
        .then([this, promise, buf, pos, delim](const auto& result) mutable {
            if (result.first) {
                MC_THROW(mc::exception, "DBus读取失败: ${error}",
                         ("error", result.first.message()));
            }

            auto new_pos = buf.find(delim, pos);
            if (new_pos != std::string_view::npos) {
                promise.set_value(std::string_view(buf.data(), new_pos));
                return;
            }

            async_read_until_impl(buf, buf.size(), delim)
                .then([promise](const auto& result) mutable {
                    promise.set_value(result);
                });
        })
        .catch_error([promise](const std::exception& ec) mutable {
            promise.set_exception(std::current_exception());
            return 0;
        });

    return future;
}

mc::future<transport::buffer> transport::async_read_until(std::string_view buf,
                                                          std::string_view delim) {
    return async_read_until_impl(buf, 0, delim);
}

} // namespace mc::dbus