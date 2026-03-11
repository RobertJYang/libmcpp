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

#include "mc/interprocess/shared_memory.h"
#include "mc/interprocess/shared_memory_manager.h"
#include "mc/exception.h"
#include "mc/log.h"

#include <cstring>
#include <functional>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>    // 对于 O_* 常量
#include <sys/mman.h> // 对于 mmap, munmap
#include <unistd.h>   // 对于 ftruncate

namespace mc {
namespace interprocess {

std::shared_ptr<shared_memory> shared_memory::create(const std::string& name, size_t size)
{
    // 确保大小足够
    if (size < MIN_MEMORY_SIZE) {
        size = MIN_MEMORY_SIZE;
    }

    // 处理共享内存名称 (POSIX需要/开头)
    std::string shm_name = shared_memory_manager::format_shm_name(name);
    if (shm_name.empty()) {
        elog("无效的共享内存名称");
        return nullptr;
    }

    // 尝试打开现有的共享内存
    int  fd     = shm_open(shm_name.c_str(), O_RDWR, 0666);
    bool is_new = false;

    if (fd == -1) {
        // 如果不存在，创建新的
        fd = shm_open(shm_name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0666);
        if (fd == -1) {
            elog("无法创建共享内存: ${error}", ("error", strerror(errno)));
            return nullptr;
        }

        // 设置共享内存大小
        if (ftruncate(fd, size) == -1) {
            elog("无法设置共享内存大小: ${error}", ("error", strerror(errno)));
            close(fd);
            shm_unlink(shm_name.c_str());
            return nullptr;
        }

        is_new = true;
    }

    // 获取共享内存大小
    struct stat shm_stat;
    if (fstat(fd, &shm_stat) == -1) {
        elog("无法获取共享内存信息: ${error}", ("error", strerror(errno)));
        close(fd);
        return nullptr;
    }

    size_t actual_size = shm_stat.st_size;

    // 检查大小是否足够
    if (!is_new && actual_size < size) {
        elog("现有共享内存大小不足, 需要${need}字节，实际${actual}字节",
             ("need", size)("actual", actual_size));
        close(fd);
        return nullptr;
    }

    // 映射共享内存
    void* addr = mmap(nullptr, actual_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                      fd, 0);
    if (addr == MAP_FAILED) {
        elog("无法映射共享内存: ${error}", ("error", strerror(errno)));
        close(fd);
        return nullptr;
    }

    // 创建shared_memory对象
    auto shm = std::shared_ptr<shared_memory>(new shared_memory(name, fd, addr, actual_size));

    // 如果是新创建的共享内存，初始化它
    if (is_new && !shm->init_memory()) {
        munmap(addr, actual_size);
        close(fd);
        shm_unlink(shm_name.c_str());
        return nullptr;
    }

    // 注册当前进程
    if (!shm->register_process()) {
        munmap(addr, actual_size);
        close(fd);
        return nullptr;
    }

    return shm;
}

shared_memory::shared_memory(std::string name, int fd, void* addr, size_t size)
    : m_name(std::move(name)), m_fd(fd), m_addr(addr), m_size(size),
      m_header(static_cast<shared_memory_header*>(addr)),
      m_allocator(static_cast<char*>(addr) + sizeof(shared_memory_header),
                  size - sizeof(shared_memory_header)),
      m_process_slot(0)
{
}

shared_memory::~shared_memory()
{
    if (m_addr) {
        unregister_process();
        munmap(m_addr, m_size);
        close(m_fd);
    }
}

bool shared_memory::init_memory()
{
    if (!m_header) {
        return false;
    }

    // 初始化头部
    m_header->magic.store(HEADER_MAGIC, std::memory_order_relaxed);
    m_header->version.store(HEADER_VERSION, std::memory_order_relaxed);
    m_header->total_size.store(m_size, std::memory_order_relaxed);
    m_header->used_size.store(0, std::memory_order_relaxed);
    m_header->creator_pid.store(getpid(), std::memory_order_relaxed);
    m_header->process_count.store(0, std::memory_order_relaxed);

    // 初始化活跃进程列表
    for (size_t i = 0; i < 64; i++) {
        m_header->active_processes[i].store(0, std::memory_order_relaxed);
    }

    return true;
}

bool shared_memory::register_process()
{
    if (!m_header) {
        return false;
    }

    // 查找空闲槽位
    for (size_t i = 0; i < 64; i++) {
        pid_t pid = m_header->active_processes[i].load(std::memory_order_relaxed);
        if (pid == 0) {
            m_process_slot = i;
            m_header->active_processes[i].store(getpid(), std::memory_order_relaxed);
            m_header->process_count.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }

    elog("没有可用的进程槽位");
    return false;
}

void shared_memory::unregister_process()
{
    if (!m_header) {
        return;
    }

    // 清除当前进程的槽位
    if (m_process_slot < 64) {
        m_header->active_processes[m_process_slot].store(0, std::memory_order_relaxed);
        m_header->process_count.fetch_sub(1, std::memory_order_relaxed);
    }
}

shared_memory_allocator& shared_memory::get_allocator()
{
    return m_allocator;
}

std::string_view shared_memory::get_name() const
{
    return m_name;
}

size_t shared_memory::get_size() const
{
    return m_size;
}

int shared_memory::get_shmid() const
{
    return m_fd;
}

void* shared_memory::get_address() const
{
    return m_addr;
}

bool shared_memory::is_valid() const
{
    return m_addr != nullptr && m_header != nullptr &&
           m_header->magic.load(std::memory_order_relaxed) == HEADER_MAGIC &&
           m_header->version.load(std::memory_order_relaxed) == HEADER_VERSION;
}

size_t shared_memory::get_offset(const void* ptr) const
{
    if (!ptr || !m_addr) {
        return 0;
    }

    const char* ptr_char  = static_cast<const char*>(ptr);
    const char* base_char = static_cast<const char*>(m_addr);

    if (ptr_char < base_char || ptr_char >= base_char + m_size) {
        return 0;
    }

    return ptr_char - base_char;
}

void* shared_memory::get_ptr_from_offset(size_t offset) const
{
    if (!m_addr || offset >= m_size) {
        return nullptr;
    }

    return static_cast<char*>(m_addr) + offset;
}

void* shared_memory::get_data_address() const
{
    return static_cast<char*>(m_addr) + sizeof(shared_memory_header);
}

size_t shared_memory::get_data_size() const
{
    return m_size - sizeof(shared_memory_header);
}

std::string shared_memory::format_shm_name(const std::string& name)
{
    if (name.empty()) {
        return "";
    }

    if (name[0] == '/') {
        return name;
    }

    return "/" + name;
}

} // namespace interprocess
} // namespace mc