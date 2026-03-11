/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "mc/interprocess/shared_memory_manager.h"
#include "mc/exception.h"
#include "mc/log.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace mc {
namespace interprocess {

std::string shared_memory_manager::format_shm_name(const std::string& name)
{
    if (name.empty()) {
        return "";
    }

    if (name[0] != '/') {
        return "/" + name;
    }

    return name;
}

shared_memory_manager::shared_memory_manager(const std::string& base_name,
                                             size_t             size,
                                             uint32_t           opts)
    : m_name(base_name), m_remove_on_exit((opts & REMOVE_ON_EXIT) != 0)
{
    // 如果设置了REMOVE_IF_EXISTS选项，先尝试删除已存在的共享内存
    if (opts & REMOVE_IF_EXISTS) {
        if (remove_shared_memory(m_name)) {
            ilog("已删除已存在的共享内存: ${name}", ("name", m_name));
        }
    }

    // 创建共享内存对象
    size_t actual_size = (size > 0) ? size : shared_memory::MIN_MEMORY_SIZE;
    m_shared_memory    = shared_memory::create(m_name, actual_size);

    if (!m_shared_memory) {
        elog("无法创建共享内存管理器: 创建共享内存失败");
    } else {
        ilog("共享内存管理器创建成功: ${name}, 大小: ${size}字节",
             ("name", m_name)("size", m_shared_memory->get_size()));
    }
}

shared_memory_manager::~shared_memory_manager()
{
    if (m_remove_on_exit) {
        cleanup();
    }
}

std::shared_ptr<shared_memory> shared_memory_manager::get_shared_memory() const
{
    return m_shared_memory;
}

std::string shared_memory_manager::get_name() const
{
    return m_name;
}

void shared_memory_manager::cleanup()
{
    // 释放共享内存资源
    if (m_shared_memory) {
        // shared_memory对象的析构函数会处理共享内存的释放
        m_shared_memory.reset();

        // 确保共享内存对象被完全删除
        std::string shm_name = format_shm_name(m_name);
        if (!shm_name.empty()) {
            shm_unlink(shm_name.c_str());
            ilog("共享内存已释放: ${name}", ("name", m_name));
        }
    }
}

void shared_memory_manager::set_remove_on_exit(bool remove_on_exit)
{
    m_remove_on_exit = remove_on_exit;
}

bool shared_memory_manager::remove_shared_memory(const std::string& name)
{
    // 处理共享内存名称 (确保有/前缀)
    std::string shm_name = format_shm_name(name);
    if (shm_name.empty()) {
        return false;
    }

    // 尝试打开共享内存
    int fd = shm_open(shm_name.c_str(), O_RDWR, 0666);
    if (fd == -1) {
        // 共享内存不存在，无需删除
        return false;
    }

    // 关闭文件描述符
    close(fd);

    // 删除共享内存
    if (shm_unlink(shm_name.c_str()) == 0) {
        ilog("成功删除共享内存: ${name}", ("name", shm_name));
        return true;
    } else {
        elog("无法删除共享内存: ${name}, 错误: ${error}",
             ("name", shm_name)("error", strerror(errno)));
        return false;
    }
}

} // namespace interprocess
} // namespace mc