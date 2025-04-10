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

#ifndef MC_DATABASE_TRANSACTION_H
#define MC_DATABASE_TRANSACTION_H

#include <atomic>
#include <mc/intrusive/intrusive.h>
#include <mc/singleton.h>
#include <memory>
#include <vector>

namespace mc::db {

/**
 * 资源标签
 */
enum class resource_tag {
    table_op,  ///< 表操作资源
    savepoint, ///< 保存点资源
    custom     ///< 自定义资源
};

// 前向声明
class transaction;

/**
 * 数据库资源基类，所有需要事务管理的资源都需要继承此类
 */
class db_resource : public mc::intrusive::unordered_set_hook {
public:
    db_resource() : m_savepoint_id(-1), m_next(nullptr), m_is_head(false), m_is_valid(true) {
    }

    virtual ~db_resource() = default;

    /**
     * 提交资源
     * @return 成功返回true，失败返回false
     */
    virtual bool commit() = 0;

    /**
     * 回滚资源
     * @return 成功返回true，失败返回false
     */
    virtual bool rollback() = 0;

    /**
     * 获取资源ID，每个资源在事务中的唯一标识，用于回合并时找到对应的资源
     * @return 资源ID
     */
    virtual uint64_t resource_id() const = 0;

    /**
     * 尝试与另一个资源合并
     * @param other 要合并的资源
     * @return 合并结果
     */
    virtual bool merge(const db_resource& other) {
        return false;
    }

    // 资源创建时的保存点ID
    int32_t m_savepoint_id;

    // 链接相同资源ID的下一个资源
    db_resource* m_next;
    bool         m_is_head;  // 是否是该资源ID链表的头节点
    bool         m_is_valid; // 是否有效

    friend class transaction;
};

/**
 * 资源哈希函数
 */
struct resource_hash {
    std::size_t operator()(const db_resource& resource) const {
        return std::hash<uint64_t>()(resource.resource_id());
    }

    std::size_t operator()(uint64_t id) const {
        return std::hash<uint64_t>()(id);
    }
};

/**
 * 资源相等比较函数
 */
struct resource_equal {
    bool operator()(const db_resource& lhs, const db_resource& rhs) const {
        return lhs.resource_id() == rhs.resource_id() && lhs.m_is_head == rhs.m_is_head;
    }

    bool operator()(uint64_t id, const db_resource& resource) const {
        return id == resource.resource_id() && resource.m_is_head;
    }

    bool operator()(const db_resource& resource, uint64_t id) const {
        return resource.resource_id() == id && resource.m_is_head;
    }
};

/**
 * 事务保存点
 */
class savepoint : public db_resource {
    friend class transaction;

public:
    bool     commit() override;
    bool     rollback() override;
    uint64_t resource_id() const override;

    savepoint(savepoint&&)                 = delete;
    savepoint& operator=(savepoint&&)      = delete;
    savepoint(const savepoint&)            = delete;
    savepoint& operator=(const savepoint&) = delete;

private:
    explicit savepoint(transaction* txn);

private:
    transaction* m_txn;
};

/**
 * 事务单例标签
 */
struct default_transaction_tag {};

/**
 * 数据库事务类
 */
class transaction {
public:
    ~transaction();

    /**
     * 获取事务单例
     * @return 事务单例引用
     */
    template <typename tag = default_transaction_tag>
    static transaction& get_instance() {
        return mc::singleton<transaction, tag>::instance_with_creator([]() {
            return new transaction();
        });
    }

    template <typename tag = default_transaction_tag>
    static void reset_for_test() {
        mc::singleton<transaction, tag>::reset_for_test();
    }

    /**
     * 分配一个新的保存点
     * @return 保存点引用
     */
    savepoint& alloc_savepoint();

    /**
     * 添加资源到事务中
     * @param resource 资源
     */
    void add_resource(std::shared_ptr<db_resource> resource);

    /**
     * 提交事务
     */
    void commit();

    /**
     * 回滚事务
     */
    void rollback();

    /**
     * 回滚到指定保存点
     * @param sp 保存点
     */
    void rollback_to(const savepoint& sp);

    int32_t last_savepoint_id() const {
        return m_current_savepoint_id;
    }

    static uint32_t alloc_table_id() {
        static std::atomic<uint32_t> table_id = 0;
        return ++table_id;
    }

    /**
     * 获取当前事务中的资源数量
     * @return 资源数量
     */
    size_t get_resource_count() const {
        return m_resources.size();
    }

    /**
     * 获取当前事务中的资源映射表大小
     * @return 资源映射表大小
     */
    size_t get_resource_map_size() const {
        return m_resource_map.size();
    }

    /**
     * 检查指定资源ID是否在事务中存在
     * @param resource_id 资源ID
     * @return 存在返回true，否则返回false
     */
    bool has_resource(uint64_t resource_id) const {
        return m_resource_map.find(resource_id) != m_resource_map.end();
    }

    /**
     * 获取指定资源ID对应的资源数量（链表长度）
     * @param resource_id 资源ID
     * @return 资源数量
     */
    size_t get_resource_chain_length(uint64_t resource_id) const {
        auto it = m_resource_map.find(resource_id);
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

private:
    transaction();
    bool merge_back(int sp_id, db_resource& resource, db_resource* head);
    void rollback_back(int sp_id, uint64_t resource_id, db_resource& resource);
    void init_bucket_arrays();

private:
    std::vector<std::shared_ptr<db_resource>> m_resources;

    // 按资源ID分组的资源链表
    static constexpr size_t BUCKET_COUNT = 64; // 默认桶数
    std::vector<void*>      m_buckets;
    using resource_map = mc::intrusive::unordered_set<db_resource, resource_hash, resource_equal>;
    resource_map m_resource_map;

    // 当前保存点ID计数
    int32_t m_current_savepoint_id = -1;
};

} // namespace mc::db

#endif // MC_DATABASE_TRANSACTION_H