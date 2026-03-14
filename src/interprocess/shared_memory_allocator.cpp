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

#include "mc/interprocess/shared_memory_allocator.h"
#include "mc/exception.h"
#include "mc/log.h"

#include <cstring>
#include <stdexcept>

namespace mc {
namespace interprocess {

// 定义魔数常量 - 使用有效的十六进制值
constexpr uint32_t BLOCK_MAGIC = 0xB10C4; // BLOCK的一种十六进制表示

// 共享内存分配器实现
shared_memory_allocator::shared_memory_allocator(void* base_addr, size_t size)
    : m_base_addr(base_addr), m_size(size), m_allocated(0)
{
    // 初始化为一个大的空闲块
    if (base_addr && size > sizeof(memory_block_header)) {
        auto header = static_cast<memory_block_header*>(base_addr);
        header->size.store(size, std::memory_order_relaxed);
        header->is_free.store(true, std::memory_order_relaxed);
        header->magic.store(BLOCK_MAGIC, std::memory_order_relaxed); // 魔数，用于检查块完整性
    }
}

void* shared_memory_allocator::allocate(size_t size)
{
    // 对齐大小到8字节边界
    size = (size + 7) & ~7;

    // 添加头部大小
    size_t total_size = size + sizeof(memory_block_header);

    // 查找空闲块
    auto block = find_free_block(total_size);
    if (!block) {
        return nullptr;
    }

    // 如果块太大，分割它
    if (block->size.load(std::memory_order_relaxed) > total_size + sizeof(memory_block_header) + 64) {
        split_block(block, total_size);
    }

    // 标记为已使用
    block->is_free.store(false, std::memory_order_release);

    // 更新分配计数
    m_allocated.fetch_add(block->size.load(std::memory_order_relaxed), std::memory_order_relaxed);

    // 返回用户可用的内存部分
    return get_user_ptr(block);
}

void shared_memory_allocator::deallocate(void* ptr)
{
    if (!ptr) {
        return;
    }

    // 获取块头部
    auto header = get_header(ptr);
    // 检查块魔数
    if (header->magic.load(std::memory_order_relaxed) != BLOCK_MAGIC) {
        elog("无效的内存块释放尝试，魔数不匹配");
        return;
    }

    // 更新分配计数
    m_allocated.fetch_sub(header->size.load(std::memory_order_relaxed), std::memory_order_relaxed);

    // 标记为空闲
    header->is_free.store(true, std::memory_order_release);

    // 合并相邻的空闲块
    merge_adjacent_blocks();
}

size_t shared_memory_allocator::get_allocated_size() const
{
    return m_allocated.load(std::memory_order_relaxed);
}

size_t shared_memory_allocator::get_available_size() const
{
    return m_size - m_allocated.load(std::memory_order_relaxed);
}

void* shared_memory_allocator::get_base_address() const
{
    return m_base_addr;
}

size_t shared_memory_allocator::get_total_size() const
{
    return m_size;
}

shared_memory_allocator::memory_block_header* shared_memory_allocator::find_free_block(size_t size)
{
    auto current  = static_cast<memory_block_header*>(m_base_addr);
    auto end_addr = static_cast<char*>(m_base_addr) + m_size;

    while (current && static_cast<void*>(current) < end_addr) {
        // 检查魔数
        if (current->magic.load(std::memory_order_relaxed) != BLOCK_MAGIC) {
            elog("内存块已损坏，魔数不匹配");
            return nullptr;
        }

        // 检查是否空闲且大小足够
        if (current->is_free.load(std::memory_order_acquire) && current->size.load(std::memory_order_relaxed) >= size) {
            return current;
        }

        // 移动到下一个块
        current = reinterpret_cast<memory_block_header*>(reinterpret_cast<char*>(current) +
                                                         current->size.load(std::memory_order_relaxed));
    }

    return nullptr;
}

shared_memory_allocator::memory_block_header* shared_memory_allocator::get_header(void* ptr)
{
    return reinterpret_cast<memory_block_header*>(static_cast<char*>(ptr) - sizeof(memory_block_header));
}

void* shared_memory_allocator::get_user_ptr(memory_block_header* header)
{
    return static_cast<void*>(reinterpret_cast<char*>(header) + sizeof(memory_block_header));
}

void shared_memory_allocator::split_block(memory_block_header* block, size_t size)
{
    size_t old_size = block->size.load(std::memory_order_relaxed);

    // 设置当前块的新大小
    block->size.store(size, std::memory_order_relaxed);

    // 创建新的块
    auto new_block = reinterpret_cast<memory_block_header*>(reinterpret_cast<char*>(block) + size);

    // 初始化新块
    new_block->size.store(old_size - size, std::memory_order_relaxed);
    new_block->is_free.store(true, std::memory_order_relaxed);
    new_block->magic.store(BLOCK_MAGIC, std::memory_order_relaxed);
}

void shared_memory_allocator::merge_adjacent_blocks()
{
    auto current  = static_cast<memory_block_header*>(m_base_addr);
    auto end_addr = static_cast<char*>(m_base_addr) + m_size;

    while (current && static_cast<void*>(current) < end_addr) {
        // 检查魔数
        if (current->magic.load(std::memory_order_relaxed) != BLOCK_MAGIC) {
            elog("内存块已损坏，魔数不匹配");
            return;
        }

        // 如果当前块是空闲的
        if (current->is_free.load(std::memory_order_acquire)) {
            // 查找下一个块
            auto next = reinterpret_cast<memory_block_header*>(reinterpret_cast<char*>(current) +
                                                               current->size.load(std::memory_order_relaxed));
            // 如果下一个块在范围内且也是空闲的，合并它们
            if (static_cast<void*>(next) < end_addr && next->magic.load(std::memory_order_relaxed) == BLOCK_MAGIC &&
                next->is_free.load(std::memory_order_acquire)) {
                // 增加当前块的大小以包含下一个块
                current->size.store(current->size.load(std::memory_order_relaxed) +
                                        next->size.load(std::memory_order_relaxed),
                                    std::memory_order_relaxed);

                // 继续查找更多可合并的块
                continue;
            }
        }

        // 移动到下一个块
        current = reinterpret_cast<memory_block_header*>(reinterpret_cast<char*>(current) +
                                                         current->size.load(std::memory_order_relaxed));
    }
}

} // namespace interprocess
} // namespace mc
