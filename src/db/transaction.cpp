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

#include <mc/db/transaction.h>
#include <mc/exception.h>

namespace mc::db {

db_resource::db_resource()
    : m_savepoint_id(-1), m_next(nullptr), m_is_head(false), m_is_valid(true) {
}

// 事务保存点实现
savepoint::savepoint(transaction* txn) : m_txn(txn) {
}

bool savepoint::commit() {
    return true;
}

bool savepoint::rollback() {
    if (m_txn) {
        m_txn->rollback_to(*this);
    }
    return true;
}

uint64_t savepoint::resource_id() const {
    return 0;
}

// 数据库事务实现
transaction::transaction()
    : m_resource_map(resource_map::bucket_traits(m_buckets, MC_DICT_BUCKET_COUNT), resource_hash(), resource_equal()),
      m_current_savepoint_id(-1) {
}

transaction::~transaction() {
    if (!m_resources.empty()) {
        rollback();
    }
}

savepoint& transaction::alloc_savepoint() {
    // 增加保存点ID计数
    m_current_savepoint_id++;

    // 创建新的保存点
    auto sp            = std::shared_ptr<savepoint>(new savepoint(this));
    sp->m_savepoint_id = m_current_savepoint_id;

    // 添加到资源列表
    m_resources.push_back(sp);
    return *sp;
}

bool transaction::merge_back(int sp_id, db_resource& resource, db_resource* head) {
    if (!head) {
        return false;
    }

    // 与尾节点尝试合并
    db_resource* tail = head->m_next;
    if (!tail) {
        tail = head;
    }

    if (tail->m_savepoint_id != sp_id) {
        // 夸事务回滚点不允许合并
        return false;
    }

    MC_ASSERT(tail->m_is_valid, "tail is not valid");

    if (!tail->merge(resource)) {
        return false;
    }

    // 尾结点被合并掉了，需要更新头节点
    if (!tail->m_is_valid && tail != head) {
        head->m_next = tail->m_next;
    }
    return true;
}

void transaction::add_resource(std::shared_ptr<db_resource> resource) {
    uint64_t resource_id = resource->resource_id();
    auto     last_sp_id  = last_savepoint_id();
    if (resource_id == 0) {
        return;
    }

    // 查找对应资源ID的头节点
    auto it = m_resource_map.find(resource_id, m_resource_map.hash_function(), m_resource_map.key_eq());
    if (it == m_resource_map.end()) {
        // 没有找到头节点，将当前资源设为头节点
        resource->m_is_head      = true;
        resource->m_savepoint_id = last_sp_id;

        // 将资源插入哈希表
        m_resource_map.insert(*resource);
        m_resources.push_back(std::move(resource));
        return;
    }

    db_resource* head = const_cast<db_resource*>(&(*it));

    // 尝试合并资源
    if (merge_back(last_sp_id, *resource, head)) {
        if (!head->m_is_valid) {
            // 头节点被合并掉了，需要从哈希表中移除
            m_resource_map.erase(it);
        }
        return;
    }

    // 设置资源的保存点ID
    resource->m_savepoint_id = last_sp_id;

    // 添加新资源到链表
    // 始终将新资源插入到头节点之后（作为新的尾节点）
    // 原来的尾节点（如果存在）将成为下一个节点
    resource->m_next = head->m_next;
    head->m_next     = resource.get();

    // 保存资源的所有权
    m_resources.push_back(std::move(resource));
}

void transaction::commit() {
    // 按正向顺序提交所有资源
    for (auto& resource : m_resources) {
        if (resource->m_is_valid) {
            resource->commit();
        }
    }

    // 清理资源
    m_resource_map.clear();
    m_resources.clear();

    // 重置保存点计数
    m_current_savepoint_id = -1;
}

void transaction::rollback() {
    // 按反向顺序回滚所有资源
    for (auto it = m_resources.rbegin(); it != m_resources.rend(); ++it) {
        (*it)->rollback();
    }

    // 清理资源
    m_resource_map.clear();
    m_resources.clear();

    // 重置保存点计数
    m_current_savepoint_id = -1;
}

void transaction::rollback_to(const savepoint& sp) {
    // 指定位置之后的资源全部回滚（逆序）
    auto res_it = m_resources.end();
    for (auto it = m_resources.rbegin(); it != m_resources.rend(); ++it) {
        if (it->get() == &sp) {
            res_it = it.base() - 1;
            break;
        }

        if (!(*it)->m_is_valid) {
            continue;
        }

        rollback_back(sp.m_savepoint_id, (*it)->resource_id(), **it);
    }

    // 先更新当前保存点ID为回滚到的保存点ID
    m_current_savepoint_id = sp.m_savepoint_id - 1;

    // 再移除事务资源
    m_resources.erase(res_it, m_resources.end());
}

void transaction::rollback_back(int sp_id, uint64_t resource_id, db_resource& resource) {
    // 查找对应资源ID的头节点
    auto it = m_resource_map.find(resource_id, m_resource_map.hash_function(), m_resource_map.key_eq());
    if (it == m_resource_map.end()) {
        return;
    }

    // 如果头节点保存点ID大于回滚点，直接回滚所有资源
    db_resource* head = const_cast<db_resource*>(&(*it));
    if (head->m_savepoint_id >= sp_id) {
        head->rollback();
        head->m_is_valid = false;
        for (auto it = head->m_next; it; it = it->m_next) {
            it->m_is_valid = false;
        }
        m_resource_map.erase(m_resource_map.iterator_to(*head));
        return;
    }

    // 从头节点的下一个节点（尾节点）开始查找sp_id的边界
    db_resource* first_node = nullptr;
    for (db_resource* it = head->m_next; it && it->m_savepoint_id >= sp_id; it = it->m_next) {
        it->m_is_valid = false;
        first_node     = it;
    }

    if (first_node) {
        first_node->rollback();
        head->m_next = first_node->m_next;
    }
}

int32_t transaction::last_savepoint_id() const {
    return m_current_savepoint_id;
}

uint32_t transaction::alloc_table_id() {
    static std::atomic<uint32_t> table_id = 0;
    return ++table_id;
}

size_t transaction::get_resource_count() const {
    return m_resources.size();
}

size_t transaction::get_resource_map_size() const {
    return m_resource_map.size();
}

bool transaction::has_resource(uint64_t resource_id) const {
    return m_resource_map.find(resource_id, m_resource_map.hash_function(), m_resource_map.key_eq()) != m_resource_map.end();
}

size_t transaction::get_resource_chain_length(uint64_t resource_id) const {
    auto it = m_resource_map.find(resource_id, m_resource_map.hash_function(), m_resource_map.key_eq());
    if (it == m_resource_map.end()) {
        return 0;
    }

    size_t count = 1; // 头节点
    auto*  node  = const_cast<db_resource*>(&(*it))->m_next;
    while (node) {
        ++count;
        node = node->m_next;
    }
    return count;
}

} // namespace mc::db