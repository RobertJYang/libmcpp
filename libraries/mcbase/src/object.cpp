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

#include <mc/object.h>
#include <mc/runtime/runtime_strand.h>
#include <mc/runtime/runtime_context.h>
#include <mc/sync/mutex_box.h>
#include <mc/sync/shared_mutex.h>

#include <algorithm>
#include <limits>
#include <optional>

namespace mc {

namespace {

bool should_continue_propagation(const mc::event& e) noexcept
{
    return !e.is_accepted();
}

uint64_t next_event_receiver_token() noexcept
{
    static std::atomic<uint64_t> next_token{1};
    return next_token.fetch_add(1, std::memory_order_relaxed);
}

mc::runtime::runtime_strand make_event_lane(const mc::runtime::any_executor& executor)
{
    auto lane = mc::runtime::runtime_strand();
    if (auto* pool = executor.get_bound_pool()) {
        lane.bound_pool(pool);
    }
    return lane;
}

struct deferred_delete_event : mc::event {
    deferred_delete_event() : mc::event(mc::deferred_delete_event_type)
    {}
};

} // namespace

/* ------------------------ object_data & object_impl ----------------------- */

struct object_data {
    mc::string                           name;           // 对象名称
    object_weak_ptr                      parent;         // 父对象指针
    child_list                           children;       // 子对象列表
    bool                                 is_deleted;     // 标记对象是否已被删除
    mc::connection_manager               connection_mgr; // 连接管理器
    std::optional<mc::any_executor>      executor;       // 绑定的执行器
    std::optional<mc::runtime::runtime_strand> event_lane;
    uint64_t                             event_receiver_token{0};
    bool                                 delete_later_queued{false};
    std::shared_ptr<event_filter_signal> event_filters;  // 事件过滤器

    object_data() : parent(nullptr), is_deleted(false)
    {}

    // 拷贝构造函数
    object_data(const object_data& other)
        : name(other.name), parent(other.parent), children(other.children), is_deleted(other.is_deleted)
    {
        // 连接管理器不应该被复制，每个对象应该有自己的连接
    }

    // 拷贝赋值运算符
    object_data& operator=(const object_data& other)
    {
        if (this != &other) {
            name       = other.name;
            parent     = other.parent;
            children   = other.children;
            is_deleted = other.is_deleted;
            // 连接管理器不应该被复制，每个对象应该有自己的连接
        }
        return *this;
    }
};

class object_impl {
public:
    object_impl();

    object_impl(const object_impl& other);
    object_impl& operator=(const object_impl& other);

    object_impl(object_impl&& other)            = delete;
    object_impl& operator=(object_impl&& other) = delete;

    // 线程安全的数据访问方法
    mc::string_view                      get_name() const;
    void                                 set_name(mc::string_view name);
    object_ptr                           get_parent() const;
    void                                 set_parent(object* parent);
    child_list                           get_children() const;
    object_ptr                           find_child(mc::string_view name) const;
    void                                 add_child(object* child);
    void                                 remove_child(object* child);
    void                                 set_executor(mc::any_executor executor);
    mc::any_executor                     get_executor() const;
    mc::runtime::any_executor            get_event_executor() const;
    uint64_t                             get_event_receiver_token() const;
    bool                                 try_mark_delete_later();
    mc::connection                       install_event_filter(event_filter_handler filter, int priority);
    std::shared_ptr<event_filter_signal> get_event_filters() const;

    // 清理方法，返回需要清理的子对象列表和父对象
    std::pair<child_list, object_ptr> cleanup_data();

    // 连接管理方法
    void add_connection(mc::connection_type conn);

private:
    mc::mutex_box<object_data, mc::shared_mutex> m_data;
};

using impl_ptr = std::unique_ptr<object_impl>;

object_impl::object_impl()
{
    m_data.wlock()->event_receiver_token = next_event_receiver_token();
}

object_impl::object_impl(const object_impl& other)
{
    // 复制基本数据但不复制父子关系
    auto other_data = other.m_data.rlock();
    auto my_data    = m_data.wlock();
    my_data->name   = other_data->name;
    my_data->event_receiver_token = next_event_receiver_token();
    // 注意：不复制 parent 和 children，新对象是独立的
    // 连接管理器不需要复制，每个对象应该有自己的连接
}

object_impl& object_impl::operator=(const object_impl& other)
{
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

mc::string_view object_impl::get_name() const
{
    // 使用 thread_local 存储确保 string_view 的生命周期安全
    thread_local mc::string cached_name;
    cached_name = m_data.rlock()->name;
    return cached_name;
}

void object_impl::set_name(mc::string_view name)
{
    // 在同一个锁保护下进行检查和设置，确保原子性
    auto data = m_data.wlock();
    MC_ASSERT(data->name.empty(), "对象名称已设置，不能重复设置: 当前名称='${current}', 尝试设置='${new}'",
              ("current", data->name)("new", name));

    data->name = mc::string(name);
}

object_ptr object_impl::get_parent() const
{
    return m_data.rlock()->parent.lock();
}

void object_impl::set_parent(object* parent)
{
    m_data.wlock()->parent = parent->weak_from_this();
}

child_list object_impl::get_children() const
{
    return m_data.rlock()->children;
}

object_ptr object_impl::find_child(mc::string_view name) const
{
    return m_data.with_lock([name](auto& data) -> object_ptr {
        auto it = std::find_if(data.children.begin(), data.children.end(), [name](const object_ptr& child) {
            if (!child) {
                return false;
            }
            return child->get_name() == name;
        });

        return (it != data.children.end()) ? *it : nullptr;
    });
}

void object_impl::add_child(object* child)
{
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

void object_impl::remove_child(object* child)
{
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

std::pair<child_list, object_ptr> object_impl::cleanup_data()
{
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
        data.connection_mgr.disconnect_all();

        return std::make_pair(std::move(children_to_cleanup), parent);
    });
}

void object_impl::add_connection(mc::connection_type conn)
{
    m_data.wlock()->connection_mgr.add(std::move(conn));
}

void object_impl::set_executor(mc::any_executor executor)
{
    m_data.with_lock([&executor](auto& data) {
        data.executor             = std::move(executor);
        data.event_lane           = make_event_lane(*data.executor);
        data.event_receiver_token = next_event_receiver_token();
    });
}

mc::any_executor object_impl::get_executor() const
{
    return m_data.with_lock([](auto& data) -> mc::any_executor {
        if (data.executor.has_value()) {
            return *data.executor;
        }
        return mc::get_work_executor();
    });
}

mc::runtime::any_executor object_impl::get_event_executor() const
{
    {
        auto data = m_data.rlock();
        if (data->event_lane.has_value()) {
            return mc::runtime::any_executor(*data->event_lane);
        }
    }

    auto& self = const_cast<object_impl&>(*this);
    auto  data = self.m_data.wlock();
    if (!data->event_lane.has_value()) {
        auto base_executor = data->executor.has_value() ? *data->executor : mc::get_work_executor();
        data->event_lane   = make_event_lane(base_executor);
    }
    return mc::runtime::any_executor(*data->event_lane);
}

uint64_t object_impl::get_event_receiver_token() const
{
    {
        auto data = m_data.rlock();
        if (data->event_receiver_token != 0) {
            return data->event_receiver_token;
        }
    }

    auto& self = const_cast<object_impl&>(*this);
    auto  data = self.m_data.wlock();
    if (data->event_receiver_token == 0) {
        data->event_receiver_token = next_event_receiver_token();
    }
    return data->event_receiver_token;
}

bool object_impl::try_mark_delete_later()
{
    return m_data.with_lock([](auto& data) {
        if (data.delete_later_queued) {
            return false;
        }
        data.delete_later_queued = true;
        return true;
    });
}

mc::connection object_impl::install_event_filter(event_filter_handler filter, int priority)
{
    auto data = m_data.wlock();
    if (data->event_filters == nullptr) {
        data->event_filters = std::make_shared<event_filter_signal>();
    }

    auto filters = data->event_filters;
    return filters->connect([filter = std::move(filter)](object* child, mc::event& e) mutable {
        if (!should_continue_propagation(e)) {
            return;
        }

        if (filter(child, e)) {
            e.accept();
        }
    }, priority);
}

std::shared_ptr<event_filter_signal> object_impl::get_event_filters() const
{
    return m_data.rlock()->event_filters;
}

/* ------------------------ object ----------------------- */
object::object()
{
    // impl 将在需要时延迟创建
}

object::object(object* parent)
{
    // 现在可以安全地设置父对象，因为 shared_from_this() 总是可用
    if (parent) {
        set_parent(parent);
    }
}

object::~object() noexcept
{
    cleanup_on_destroy();
}

void object::cleanup_on_destroy() noexcept
{
    impl_ptr impl(m_object_impl.exchange(nullptr, std::memory_order_acq_rel));
    if (!impl) {
        return;
    }

    try {
        mc::runtime::get_runtime_context().remove_posted_events(this);
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
}

object::object(const object& other) : mc::object_base(other)
{
    auto* impl = other.m_object_impl.load(std::memory_order_acquire);
    if (impl) {
        m_object_impl.store(new object_impl(*impl), std::memory_order_release);
    }
    // 复制基本数据但不复制父子关系在 object_impl 的构造函数中处理
}

object& object::operator=(const object& other)
{
    if (this == &other) {
        return *this;
    }

    mc::object_base::operator=(other);

    auto* other_impl = other.m_object_impl.load(std::memory_order_acquire);
    if (!other_impl) {
        impl_ptr(m_object_impl.exchange(nullptr, std::memory_order_acq_rel)).reset();
        return *this;
    }

    auto* current_impl = m_object_impl.load(std::memory_order_acquire);
    if (!current_impl) {
        auto new_impl = std::make_unique<object_impl>(*other_impl);
        m_object_impl.store(new_impl.release(), std::memory_order_release);
    } else {
        *current_impl = *other_impl;
    }
    return *this;
}

void object::set_parent(object* parent)
{
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

    // 添加到新父对象
    if (parent) {
        parent->add_child(this);
    }
}

object_ptr object::get_parent() const
{
    auto* impl = m_object_impl.load(std::memory_order_acquire);
    if (!impl) {
        return nullptr;
    }
    return impl->get_parent();
}

void object::set_name(mc::string_view name)
{
    ensure_impl().set_name(name);
}

mc::string_view object::get_name() const
{
    return ensure_impl().get_name();
}

child_list object::get_children() const
{
    auto* impl = m_object_impl.load(std::memory_order_acquire);
    if (!impl) {
        return {};
    }
    return impl->get_children();
}

object_ptr object::find_child(mc::string_view name) const
{
    auto* impl = m_object_impl.load(std::memory_order_acquire);
    if (!impl) {
        return nullptr;
    }
    return impl->find_child(name);
}

void object::send_event(mc::event& e)
{
    mc::runtime::get_runtime_context().send_event(*this, e);
}

void object::post_event(std::unique_ptr<mc::event> e)
{
    mc::runtime::get_runtime_context().post_event(*this, std::move(e));
}

void object::delete_later()
{
    auto* impl = m_object_impl.load(std::memory_order_acquire);
    if (impl == nullptr) {
        return;
    }

    if (!impl->try_mark_delete_later()) {
        return;
    }

    mc::runtime::get_runtime_context().post_event(*this, std::make_unique<deferred_delete_event>(),
                                                  std::numeric_limits<int>::min());
}

mc::connection object::install_event_filter(event_filter_handler filter, int priority)
{
    return ensure_impl().install_event_filter(std::move(filter), priority);
}

void object::add_child(object* child)
{
    ensure_impl().add_child(child);
}

void object::remove_child(object* child)
{
    auto* impl = m_object_impl.load(std::memory_order_acquire);
    if (!impl) {
        return;
    }
    impl->remove_child(child);
}

void object::on_event(mc::event& e)
{
    MC_UNUSED(e);
}

bool object::event_filter(object* child, mc::event& e)
{
    MC_UNUSED(child);
    MC_UNUSED(e);
    return false;
}

object_impl& object::ensure_impl() const
{
    // 使用双重检查确保线程安全的延迟初始化
    object_impl* impl = m_object_impl.load(std::memory_order_acquire);
    if (impl) {
        return *impl;
    }

    impl_ptr new_impl = std::make_unique<object_impl>();
    if (m_object_impl.compare_exchange_strong(impl, new_impl.get(), std::memory_order_release,
                                              std::memory_order_acquire)) {
        impl = new_impl.release();
    }
    return *impl;
}

void object::track_connection(mc::connection_type conn)
{
    ensure_impl().add_connection(std::move(conn));
}

object::executor_type object::get_executor() const
{
    auto* impl = m_object_impl.load(std::memory_order_acquire);
    if (impl) {
        return impl->get_executor();
    }
    return mc::get_work_executor();
}

void object::set_executor(executor_type executor)
{
    ensure_impl().set_executor(std::move(executor));
}

mc::runtime::any_executor object::get_event_executor() const
{
    return ensure_impl().get_event_executor();
}

uint64_t object::get_event_receiver_token() const
{
    return ensure_impl().get_event_receiver_token();
}

bool object::dispatch_event_filters(object* child, mc::event& e)
{
    auto* impl = m_object_impl.load(std::memory_order_acquire);
    if (impl != nullptr) {
        auto filters = impl->get_event_filters();
        if (filters != nullptr) {
            (*filters)(child, e);
        }
    }

    if (!should_continue_propagation(e)) {
        return true;
    }

    if (child != nullptr && event_filter(child, e)) {
        e.accept();
    }

    return !should_continue_propagation(e);
}

void object::propagate_event_from_child(object* child, mc::event& e)
{
    dispatch_event_filters(child, e);
    if (!should_continue_propagation(e)) {
        return;
    }

    on_event(e);
    if (!should_continue_propagation(e)) {
        return;
    }

    auto parent = get_parent();
    if (!parent) {
        return;
    }

    parent->propagate_event_from_child(this, e);
}

object_ptr object::shared_from_this()
{
    return mc::object_base::shared_from_this().template static_pointer_cast<object>();
}

mc::weak_ptr<object> object::weak_from_this()
{
    return mc::weak_ptr<object>(this);
}

mc::weak_ptr<const object> object::weak_from_this() const
{
    return mc::weak_ptr<const object>(this);
}

} // namespace mc
