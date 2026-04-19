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

#ifndef MC_SHM_CONTAINER_JOURNAL_H
#define MC_SHM_CONTAINER_JOURNAL_H

#include <atomic>
#include <cstdint>

// 共享内存 crash-safe 容器的事务 journal
//
// 每个容器实例自带一个 64B journal；write-side 在修改链接前 journal_begin
// 写入全部 payload 并 release store op，崩溃后 recover 路径 acquire load op
// 即可看到一致 payload，按 op 类型执行 redo/rollback
//
// 与 allocator_journal 共享相同设计模式（op 是唯一提交点）

namespace mc::shm::container {

// ---- 通用 op 集合 ----
//
// 每种容器对同一 op 的 payload 语义可能略有差异（见 design doc §4.3）
// 但基础 op 代码是公共的；容器特定的"不必 journal 的内部重排"不进此枚举
enum class container_op : std::uint8_t {
    none   = 0, // journal 空闲
    insert = 1, // 往容器中插入节点
    erase  = 2, // 从容器中删除节点
    update = 3, // 就地修改（如 RB-tree 染色、string 切换大分配）
};

inline const char* container_op_name(container_op op) noexcept
{
    switch (op) {
        case container_op::none:   return "none";
        case container_op::insert: return "insert";
        case container_op::erase:  return "erase";
        case container_op::update: return "update";
    }
    return "unknown";
}

// ---- 64B journal 结构 ----
//
// op 是 atomic 提交点（release store / acquire load）
// payload 字段在 begin 时一次性写入，commit 前的 release 保证读者能看到
// 完整快照。每个字段的具体语义按 op 类型在各容器处注释
struct alignas(64) container_journal {
    std::atomic<std::uint8_t> op;                  // 提交点；0 = none
    std::uint8_t              _pad0[7];

    std::uint64_t             target_node_offset;  // 被操作的节点 offset
    std::uint64_t             anchor_offset;       // 邻居 / 根 / 桶头 offset
    std::uint64_t             saved_link_a;        // 回滚字段 A
    std::uint64_t             saved_link_b;        // 回滚字段 B
    std::uint64_t             extra;               // 容器特定（head/tail 前值等）
    std::uint32_t             misc;                // 比如桶 index / 颜色
    std::uint8_t              _pad1[12];
};

static_assert(sizeof(container_journal) == 64, "container_journal 必须占 64 字节");
static_assert(alignof(container_journal) == 64, "container_journal 必须 64 字节对齐");

// ---- payload 快照 ----
//
// reader 侧一次性读取全部字段，保证 op != none 时后续按字段值执行 recover
// 调用者必须先 acquire load op；若 op == none 不要构造快照
struct container_journal_view {
    container_op op;
    std::uint64_t target_node_offset;
    std::uint64_t anchor_offset;
    std::uint64_t saved_link_a;
    std::uint64_t saved_link_b;
    std::uint64_t extra;
    std::uint32_t misc;
};

// ---- 初始化 ----
//
// 在容器构造 / arena 首次写入时调用，写全 0（op = none）
void journal_init(container_journal& j) noexcept;

// ---- begin / end ----
//
// begin：先填所有 payload 字段（relaxed store），最后 release store op → 非 none
// 调用者必须持有容器锁；调用前需把涉及节点置为 pending
void journal_begin(container_journal& j, container_op op, std::uint64_t target,
                   std::uint64_t anchor, std::uint64_t saved_a, std::uint64_t saved_b,
                   std::uint64_t extra, std::uint32_t misc) noexcept;

// end：release store op → none；不清 payload（减少写放大，读者先看 op）
void journal_end(container_journal& j) noexcept;

// ---- 读取 ----
//
// acquire load op；若 op != none 一并把 payload 打到 view 返回 true
// 否则返回 false（journal 空闲）。仅持锁者 / recover 路径调用
bool journal_load(const container_journal& j, container_journal_view& out) noexcept;

// 仅读 op（不消耗 payload 字段）
container_op journal_load_op(const container_journal& j) noexcept;

// ---- RAII 窗口 ----
//
// 用于把 container_op 的 begin/end 对称化；析构时若 op 仍非 none 表示
// 未正常提交（异常或死锁），析构中不自动回滚 —— 崩溃恢复路径专门处理
//
// 标准用法：
//   journal_window win(j, container_op::insert, target, anchor, sa, sb, e, m);
//   ... 修改链接 ...
//   node.state.store(linked, release);
//   win.commit();  // 显式调用；若忘记调用则 op 保留，恢复路径会识别
//
// 不在析构里自动 commit 是为了防止"异常发生在链接修改中途但 state 还是 pending"
// 被误判为已完成
class journal_window {
public:
    journal_window(container_journal& j, container_op op, std::uint64_t target,
                   std::uint64_t anchor, std::uint64_t saved_a, std::uint64_t saved_b,
                   std::uint64_t extra, std::uint32_t misc) noexcept;

    // 提交：release store op = none；可被多次调用但仅首次生效
    void commit() noexcept;

    // 是否已经 commit
    bool is_committed() const noexcept { return m_committed; }

    // 析构不自动提交（见上注释）
    ~journal_window() noexcept = default;

    journal_window(const journal_window&)            = delete;
    journal_window& operator=(const journal_window&) = delete;
    journal_window(journal_window&&)                 = delete;
    journal_window& operator=(journal_window&&)      = delete;

private:
    container_journal& m_journal;
    bool               m_committed = false;
};

} // namespace mc::shm::container

#endif // MC_SHM_CONTAINER_JOURNAL_H
