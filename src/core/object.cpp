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

#include <mc/core/object.h>
#include <mc/sync/mutex_box.h>
#include <mc/sync/shared_mutex.h>

#include "include/connection_manager.h"
#include <algorithm>

namespace mc::core {

/* ------------------------ object_data & object_impl ----------------------- */

struct object_data {
    std::string        name;           // 对象名称
    object_weak_ptr    parent;         // 父对象指针
    child_list         children;       // 子对象列表
    bool               is_deleted;     // 标记对象是否已被删除
    connection_manager connection_mgr; // 连接管理器

    object_data() : parent(nullptr), is_deleted(false) {
    }

    // 拷贝构造函数
    object_data(const object_data& other)
        : name(other.name), parent(other.parent),
          children(other.children), is_deleted(other.is_deleted) {
        // 连接管理器不应该被复制，每个对象应该有自己的连接
    }

    // 拷贝赋值运算符
    object_data& operator=(const object_data& other) {
        if (this != &other) {
            name       = other.name;
            parent     = other.parent;
            children   = other.children;
            is_deleted = other.is_deleted;
            // 连接管理器不应该被复制
        }
        return *this;
    }
};

class object_impl {
public:
    object_impl() = default;

    object_impl(const object_impl& other);
    object_impl& operator=(const object_impl& other);

    object_impl(object_impl&& other)            = delete;
    object_impl& operator=(object_impl&& other) = delete;

    // 线程安全的数据访问方法
    std::string get_name() const;
    void        set_name(std::string_view name);
    object_ptr  get_parent() const;
    void        set_parent(object* parent);
    child_list  get_children() const;
    object_ptr  find_child(std::string_view name) const;
    void        add_child(object* child);
    void        remove_child(object* child);

    // 清理方法，返回需要清理的子对象列表和父对象
    std::pair<child_list, object_ptr> cleanup_data();

    // 连接管理方法
    connection_id_type add_connection(signal_type sig, mc::connection_type conn,
                                      connection_id_type id);
    void               remove_connection(connection_id_type id);
    size_t             remove_connections(signal_type sig);
    void               clear_connections();

private:
    mc::mutex_box<object_data, mc::shared_mutex> m_data;
};

object_impl::object_impl(const object_impl& other) {
    // 复制基本数据但不复制父子关系
    auto other_data = other.m_data.rlock();
    auto my_data    = m_data.wlock();
    my_data->name   = other_data->name;
    // 注意：不复制 parent 和 children，新对象是独立的
    // 连接管理器不需要复制，每个对象应该有自己的连接
}

object_impl& object_impl::operator=(const object_impl& other) {
    if (this != &other) {
        // 复制基本数据但不复制父子关系
        auto other_data = other.m_data.rlock();
        auto my_data    = m_data.wlock();
        my_data->name   = other_data->name;
        // 注意：不复制 parent 和 children
        // 连接管理器不需要复制
    }
    return *this;
}

std::string object_impl::get_name() const {
    return m_data.rlock()->name;
}

void object_impl::set_name(std::string_view name) {
    m_data.wlock()->name = std::string(name);
}

object_ptr object_impl::get_parent() const {
    return m_data.rlock()->parent.lock();
}

void object_impl::set_parent(object* parent) {
    m_data.wlock()->parent = parent->weak_from_this();
}

child_list object_impl::get_children() const {
    return m_data.rlock()->children;
}

object_ptr object_impl::find_child(std::string_view name) const {
    return m_data.with_lock([name](auto& data) -> object_ptr {
        auto it = std::find_if(data.children.begin(), data.children.end(),
                               [name](const object_ptr& child) {
            if (!child) {
                return false;
            }
            return child->get_name() == name;
        });

        return (it != data.children.end()) ? *it : nullptr;
    });
}

void object_impl::add_child(object* child) {
    if (!child) {
        return;
    }

    m_data.with_lock([child](auto& data) {
        // 当前对象已经进入删除流程，不允许增加子对象
        if (data.is_deleted) {
            return;
        }

        auto it = std::find(data.children.begin(), data.children.end(), child);
        if (it == data.children.end()) {
            data.children.emplace_back(child);
        }
    });
}

void object_impl::remove_child(object* child) {
    if (!child) {
        return;
    }

    m_data.with_lock([child](auto& data) {
        auto it = std::find(data.children.begin(), data.children.end(), child);
        if (it != data.children.end()) {
            data.children.erase(it);
        }
    });
}

std::pair<child_list, object_ptr> object_impl::cleanup_data() {
    return m_data.with_lock([](auto& data) {
        data.is_deleted = true;

        auto parent = data.parent.lock();
        if (parent) {
            data.parent.reset();
        }

        // 移动子对象列表，这样就不会持有对子对象的引用了
        child_list children_to_cleanup = std::move(data.children);
        data.children.clear();

        // 清理所有连接
        data.connection_mgr.clear();

        return std::make_pair(std::move(children_to_cleanup), parent);
    });
}

connection_id_type object_impl::add_connection(signal_type sig, mc::connection_type conn,
                                               connection_id_type id) {
    return m_data.wlock()->connection_mgr.add_connection(sig, std::move(conn), id);
}

void object_impl::remove_connection(connection_id_type id) {
    m_data.wlock()->connection_mgr.remove_connection(id);
}

size_t object_impl::remove_connections(signal_type sig) {
    return m_data.wlock()->connection_mgr.remove_connections(sig);
}

void object_impl::clear_connections() {
    m_data.wlock()->connection_mgr.clear();
}

/* ------------------------ object ----------------------- */
object::object() {
    // impl 将在需要时延迟创建
}

object::object(object* parent) {
    // 现在可以安全地设置父对象，因为 shared_from_this() 总是可用
    if (parent) {
        set_parent(parent);
    }
}

object::~object() noexcept {
    cleanup_on_destroy();
}

void object::cleanup_on_destroy() noexcept {
    auto impl = std::move(m_object_impl);
    if (!impl) {
        return;
    }

    try {
        auto [to_cleanup, parent] = impl->cleanup_data();

        // 从父对象中移除自己
        if (parent) {
            parent->remove_child(this);
        }

        // 清空所有子对象的父指针，让它们知道父对象已经不存在了
        for (auto& child : to_cleanup) {
            child->set_parent(nullptr);
        }
        to_cleanup.clear();
    } catch (...) {
        // 析构函数中不能抛出异常，静默处理
    }

    impl.reset();
}

object::object(const object& other) : object_base(other) {
    if (other.m_object_impl) {
        m_object_impl = std::make_unique<object_impl>(*other.m_object_impl);
    }
    // 复制基本数据但不复制父子关系在 object_impl 的构造函数中处理
}

object& object::operator=(const object& other) {
    if (this != &other) {
        object_base::operator=(other);

        if (other.m_object_impl) {
            if (!m_object_impl) {
                m_object_impl = std::make_unique<object_impl>(*other.m_object_impl);
            } else {
                *m_object_impl = *other.m_object_impl;
            }
        } else {
            m_object_impl.reset();
        }
        // 复制基本数据但不复制父子关系在 object_impl 的赋值运算符中处理
    }
    return *this;
}

void object::set_parent(object* parent) {
    auto& impl       = ensure_impl();
    auto  old_parent = impl.get_parent();
    if (old_parent == parent) {
        return; // 父对象没有变化
    }

    // 设置新父对象
    impl.set_parent(parent);

    // 从旧父对象中移除
    if (old_parent) {
        old_parent->remove_child(this);
    }

    // 添加到新父对象（现在可以安全调用 shared_from_this()）
    if (parent) {
        parent->add_child(this->shared_from_this());
    }
}

object_ptr object::get_parent() const {
    if (!m_object_impl) {
        return nullptr;
    }
    return m_object_impl->get_parent();
}

void object::set_name(std::string_view name) {
    ensure_impl().set_name(name);
}

std::string object::get_name() const {
    return ensure_impl().get_name();
}

child_list object::get_children() const {
    if (!m_object_impl) {
        return {};
    }
    return m_object_impl->get_children();
}

object_ptr object::find_child(std::string_view name) const {
    if (!m_object_impl) {
        return nullptr;
    }
    return m_object_impl->find_child(name);
}

void object::add_child(object* child) {
    ensure_impl().add_child(child);
}

void object::remove_child(object* child) {
    if (!m_object_impl) {
        return;
    }
    m_object_impl->remove_child(child);
}

object_impl& object::ensure_impl() const {
    if (!m_object_impl) {
        m_object_impl = std::make_unique<object_impl>();
    }
    return *m_object_impl;
}

connection_id_type object::add_connection(signal_type sig, mc::connection_type conn,
                                          connection_id_type id) {
    return ensure_impl().add_connection(sig, std::move(conn), id);
}

void object::disconnect(connection_id_type id) const {
    if (!m_object_impl) {
        return;
    }
    m_object_impl->remove_connection(id);
}

void object::disconnect_all(signal_type sig) {
    if (!m_object_impl) {
        return;
    }
    m_object_impl->remove_connections(&sig);
}

object::executor_type object::get_executor() const {
    return mc::futures::detail::get_system_context().get_executor();
}

} // namespace mc::core