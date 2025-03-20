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

/**
 * @file shared_tree_db.h
 * @brief 基于共享内存的树结构数据库
 */
#ifndef MC_DATABASE_SHARED_TREE_DB_H
#define MC_DATABASE_SHARED_TREE_DB_H

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mc/database/shared_memory.h"
#include "mc/database/shared_mutex.h"
#include "mc/variant.h"

namespace mc {
namespace database {

// 前置声明
class shared_tree_db;
class shared_tree_node;
class shared_tree_transaction;

// 定义智能指针类型
using shared_tree_db_ptr = std::shared_ptr<shared_tree_db>;
using shared_tree_node_ptr = shared_ptr<shared_tree_node>;
using shared_tree_transaction_ptr = std::shared_ptr<shared_tree_transaction>;

/**
 * @brief 共享内存树节点
 */
class shared_tree_node {
public:
    /**
     * @brief 创建根节点
     * @param allocator 共享内存分配器
     * @return 根节点指针
     */
    static shared_tree_node_ptr create_root(shared_memory_allocator& allocator);
    
    /**
     * @brief 创建节点
     * @param allocator 共享内存分配器
     * @param name 节点名称
     * @param data 节点数据
     * @return 节点指针
     */
    static shared_tree_node_ptr create(shared_memory_allocator& allocator, 
                                     std::string name, variant data = variant());
    
    /**
     * @brief 析构函数
     */
    ~shared_tree_node();
    
    /**
     * @brief 获取节点名称
     * @return 节点名称
     */
    const std::string& get_name() const;
    
    /**
     * @brief 获取节点数据
     * @return 节点数据
     */
    const variant& get_data() const;
    
    /**
     * @brief 设置节点数据
     * @param data 节点数据
     */
    void set_data(const variant& data);
    
    /**
     * @brief 获取子节点
     * @param name 子节点名称
     * @return 子节点指针，如果不存在返回nullptr
     */
    shared_tree_node_ptr get_child(std::string_view name) const;
    
    /**
     * @brief 获取所有子节点
     * @return 子节点列表
     */
    std::vector<shared_tree_node_ptr> get_children() const;
    
    /**
     * @brief 获取子节点数量
     * @return 子节点数量
     */
    size_t get_child_count() const;
    
    /**
     * @brief 添加子节点
     * @param name 子节点名称
     * @param node 子节点指针
     * @return 是否添加成功
     */
    bool add_child(std::string_view name, shared_tree_node_ptr node);
    
    /**
     * @brief 替换子节点
     * @param name 子节点名称
     * @param node 子节点指针
     * @return 是否替换成功
     */
    bool replace_child(std::string_view name, shared_tree_node_ptr node);
    
    /**
     * @brief 删除子节点
     * @param name 子节点名称
     * @return 是否删除成功
     */
    bool remove_child(std::string_view name);
    
    /**
     * @brief 清空所有子节点
     */
    void clear_children();
    
    /**
     * @brief 克隆节点（深拷贝）
     * @param allocator 目标分配器
     * @return 新节点指针
     */
    shared_tree_node_ptr clone(shared_memory_allocator& allocator) const;
    
    /**
     * @brief 计算节点哈希值
     * @return 哈希值
     */
    size_t hash() const;

private:
    /**
     * @brief 构造函数
     * @param allocator 共享内存分配器
     * @param name 节点名称
     * @param data 节点数据
     */
    shared_tree_node(shared_memory_allocator& allocator, std::string name, variant data);
    
    // 内部结构和数据
    shared_memory_allocator& m_allocator;                               // 分配器
    std::string m_name;                                                 // 节点名称
    variant m_data;                                                     // 节点数据
    std::unordered_map<std::string, shared_tree_node_ptr, 
                      std::hash<std::string>> m_children;               // 子节点映射表
    mutable size_t m_hash_cache{0};                                     // 哈希值缓存
};

/**
 * @brief 共享内存树数据库
 */
class shared_tree_db {
public:
    /**
     * @brief 创建或打开数据库
     * @param name 数据库名称
     * @param size 共享内存大小（字节）
     * @return 数据库指针
     */
    static shared_tree_db_ptr create(const std::string& name, size_t size);
    
    /**
     * @brief 析构函数
     */
    ~shared_tree_db();
    
    /**
     * @brief 获取节点
     * @param path 节点路径
     * @return 节点指针，如果不存在返回nullptr
     */
    shared_tree_node_ptr get_node(std::string_view path) const;
    
    /**
     * @brief 获取节点数据
     * @param path 节点路径
     * @return 节点数据，如果节点不存在返回空variant
     */
    variant get_data(std::string_view path) const;
    
    /**
     * @brief 检查节点是否存在
     * @param path 节点路径
     * @return 是否存在
     */
    bool has_node(std::string_view path) const;
    
    /**
     * @brief 设置节点数据
     * @param path 节点路径
     * @param data 节点数据
     * @return 是否设置成功
     */
    bool set_data(std::string_view path, const variant& data);
    
    /**
     * @brief 创建节点
     * @param path 节点路径
     * @param data 节点数据
     * @return 是否创建成功
     */
    bool create_node(std::string_view path, const variant& data = variant());
    
    /**
     * @brief 删除节点
     * @param path 节点路径
     * @return 是否删除成功
     */
    bool delete_node(std::string_view path);
    
    /**
     * @brief 遍历所有节点
     * @param callback 回调函数，接收节点路径和节点数据
     */
    void traverse(std::function<void(std::string_view, const variant&)> callback) const;
    
    /**
     * @brief 创建事务
     * @return 事务对象
     */
    shared_tree_transaction_ptr create_transaction();
    
    /**
     * @brief 导出为标准树结构（序列化为dict）
     * @return 表示树结构的字典
     */
    dict export_to_dict() const;
    
    /**
     * @brief 从标准树结构导入（从dict反序列化）
     * @param tree_dict 表示树结构的字典
     * @return 是否导入成功
     */
    bool import_from_dict(const dict& tree_dict);
    
    /**
     * @brief 获取数据库名称
     * @return 数据库名称
     */
    std::string_view get_name() const;
    
    /**
     * @brief 获取共享内存大小
     * @return 共享内存大小（字节）
     */
    size_t get_memory_size() const;
    
    /**
     * @brief 获取已使用内存大小
     * @return 已使用内存大小（字节）
     */
    size_t get_used_memory_size() const;
    
    /**
     * @brief 获取共享内存分配器
     * @return 分配器引用
     */
    shared_memory_allocator& get_allocator();

    // 友元类
    friend class shared_tree_transaction;

private:
    /**
     * @brief 构造函数
     * @param shm 共享内存对象
     * @param root 根节点
     */
    shared_tree_db(std::shared_ptr<shared_memory> shm, shared_tree_node_ptr root);
    
    // 禁止拷贝构造和赋值
    shared_tree_db(const shared_tree_db&) = delete;
    shared_tree_db& operator=(const shared_tree_db&) = delete;
    
    // 内部方法
    shared_tree_node_ptr find_node(shared_tree_node_ptr start_node, std::string_view path) const;
    bool create_node_internal(shared_tree_node_ptr parent, std::string_view name, const variant& data);
    
    // 路径解析辅助方法
    static std::vector<std::string> parse_path(std::string_view path);
    static std::pair<std::string, std::string> split_path(std::string_view path);
    
    // 成员变量
    std::shared_ptr<shared_memory> m_shm;     // 共享内存
    shared_tree_node_ptr m_root;              // 根节点
    mutable shared_mutex m_mutex;             // 读写锁
};

/**
 * @brief 共享内存树事务
 */
class shared_tree_transaction {
public:
    /**
     * @brief 构造函数
     * @param db 数据库对象
     */
    explicit shared_tree_transaction(shared_tree_db_ptr db);
    
    /**
     * @brief 析构函数（自动回滚未提交的事务）
     */
    ~shared_tree_transaction();
    
    /**
     * @brief 获取节点
     * @param path 节点路径
     * @return 节点指针，如果不存在返回nullptr
     */
    shared_tree_node_ptr get_node(std::string_view path) const;
    
    /**
     * @brief 获取节点数据
     * @param path 节点路径
     * @return 节点数据，如果节点不存在返回空variant
     */
    variant get_data(std::string_view path) const;
    
    /**
     * @brief 检查节点是否存在
     * @param path 节点路径
     * @return 是否存在
     */
    bool has_node(std::string_view path) const;
    
    /**
     * @brief 设置节点数据
     * @param path 节点路径
     * @param data 节点数据
     * @return 是否设置成功
     */
    bool set_data(std::string_view path, const variant& data);
    
    /**
     * @brief 创建节点
     * @param path 节点路径
     * @param data 节点数据
     * @return 是否创建成功
     */
    bool create_node(std::string_view path, const variant& data = variant());
    
    /**
     * @brief 删除节点
     * @param path 节点路径
     * @return 是否删除成功
     */
    bool delete_node(std::string_view path);
    
    /**
     * @brief 提交事务
     * @return 是否提交成功
     */
    bool commit();
    
    /**
     * @brief 回滚事务
     */
    void rollback();

private:
    // 内部方法
    shared_tree_node_ptr find_node(shared_tree_node_ptr start_node, std::string_view path) const;
    bool create_node_internal(shared_tree_node_ptr parent, std::string_view name, const variant& data);
    
    // 成员变量
    shared_tree_db_ptr m_db;                   // 数据库对象
    shared_tree_node_ptr m_snapshot_root;      // 快照根节点
    shared_tree_node_ptr m_working_root;       // 工作根节点
    bool m_committed;                          // 是否已提交
};

// 外部可见别名，用于兼容之前的代码
using shared_tree_db_transaction = shared_tree_transaction;

} // namespace database
} // namespace mc

#endif // MC_DATABASE_SHARED_TREE_DB_H 