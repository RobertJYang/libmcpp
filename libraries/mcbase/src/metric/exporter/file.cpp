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

#include <mc/metric/exporter/file.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mc::metric::exporter {
namespace {

std::string _format_errno(const char* op, const std::string& path, int err)
{
    return std::string(op) + " '" + path + "': " + std::strerror(err);
}

bool _write_all(int fd, const char* data, std::size_t len, std::string& err)
{
    while (len > 0) {
        const auto n = ::write(fd, data, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            err = _format_errno("write", "<fd>", errno);
            return false;
        }
        data += n;
        len  -= static_cast<std::size_t>(n);
    }
    return true;
}

} // namespace

file_exporter::file_exporter(registry& reg, file_exporter_options opts)
    : m_registry(reg), m_options(std::move(opts))
{}

std::size_t file_exporter::write_once()
{
    m_last_error.clear();
    if (m_options.output_path.empty()) {
        m_last_error = "output_path is empty";
        return 0;
    }

    auto              snap = m_registry.collect();
    const std::string text = render_prometheus(snap, m_options.prometheus);
    if (text.empty()) {
        // 仍写一个空文件，让消费方知道服务存活但暂无 metric
    }

    // 临时文件：<path>.tmp.<pid>，同目录下保证 rename 原子
    const std::string tmp_path =
        m_options.output_path + ".tmp." + std::to_string(::getpid());

    const int fd =
        ::open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
               static_cast<mode_t>(m_options.file_mode));
    if (fd < 0) {
        m_last_error = _format_errno("open", tmp_path, errno);
        return 0;
    }

    if (!_write_all(fd, text.data(), text.size(), m_last_error)) {
        ::close(fd);
        ::unlink(tmp_path.c_str());
        return 0;
    }

    // fsync 让 rename 之后的内容真正落到设备；嵌入式磁盘小，影响可控
    if (::fsync(fd) != 0) {
        // fsync 失败不致命：记录但继续 rename，让消费方看到尽可能新的内容
        m_last_error = _format_errno("fsync", tmp_path, errno);
    }
    ::close(fd);

    if (::rename(tmp_path.c_str(), m_options.output_path.c_str()) != 0) {
        const int err = errno;
        ::unlink(tmp_path.c_str());
        m_last_error = _format_errno("rename", m_options.output_path, err);
        return 0;
    }

    return text.size();
}

} // namespace mc::metric::exporter
