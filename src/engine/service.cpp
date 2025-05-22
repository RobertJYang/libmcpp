/**
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
#include <mc/db/database.h>
#include <mc/dbus/connection.h>
#include <mc/dbus/message.h>
#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/dbus/validator.h>
#include <mc/engine.h>
#include <mc/engine/path_iterator.h>
#include <mc/engine/utils.h>
#include <mc/exception.h>
#include <mc/expr/lexer.h>
#include <mc/expr/parser.h>
#include <mc/log.h>

namespace mdb = mc::db;

namespace mc::engine {

using object_table_ptr = std::shared_ptr<service_object_table>;
using strand_type      = boost::asio::strand<boost::asio::io_context::executor_type>;
template <typename T>
using future = mc::future<T, strand_type>;

struct service_interface : public mc::engine::interface<service_interface> {
    MC_INTERFACE("bmc.kepler.maca")

    std::string m_service_name;
    std::string m_service_path;
};

struct service_object : public mc::engine::object<service_object> {
    MC_OBJECT(service_object, "ServiceObject", "/bmc/kepler/maca", (service_interface))

    void init(service* s) {
        m_interface.m_service_path = path_pattern;
        m_interface.m_service_path += "/";
        m_interface.m_service_path += s->name();

        set_object_name(s->name());
        set_object_path(m_object_path);
    }

    const std::string& get_name() const {
        return m_interface.m_service_name;
    }

    void set_name(std::string_view name) {
        m_interface.m_service_name = name;
    }

    service_interface m_interface;
};

struct root_object : public mc::engine::object<root_object> {
    MC_OBJECT(root_object, "RootObject", "/")

    root_object() {
        set_object_name("root");
        set_object_path("/");
    }
};

struct service_impl {
    service_impl();

    bool init(service* s);
    bool start();
    void stop();

    DBusHandlerResult on_path_message(mc::dbus::message& msg, abstract_object& obj);
    DBusHandlerResult on_filter_message(mc::dbus::message& msg);

    DBusHandlerResult on_method_call(abstract_object& obj, mc::dbus::message& msg);

    void register_object(abstract_object& obj);
    void unregister_object(std::string_view path);

    void                       adjust_object_parent(abstract_object& obj);
    invoke_result              invoke_method(std::string_view path, std::string_view interface,
                                             std::string_view method, const variants& args);
    mc::variant                timeout_call(mc::milliseconds timeout, std::string_view service_name,
                                            std::string_view path, std::string_view interface,
                                            std::string_view method, std::string_view signature,
                                            const variants& args);
    std::optional<mc::variant> shm_timeout_call(mc::milliseconds timeout,
                                                std::string_view service_name,
                                                std::string_view path, std::string_view interface,
                                                std::string_view method, std::string_view signature,
                                                const variants& args);
    static abstract_object*    find_owner(abstract_object& obj, std::string_view path);

    std::mutex                      m_mutex;
    service*                        m_service;
    dbus::connection_ptr            m_connection;
    object_table_ptr                m_object_table;
    mc::im::ref_ptr<service_object> m_service_object;
    root_object                     m_root;
    mc::dbus::shm_tree*             m_shm_tree;
};
} // namespace mc::engine

MC_REFLECT(mc::engine::service_interface, ((m_service_name, "name")))
MC_REFLECT(mc::engine::service_object, ((m_interface, "interface")))
MC_REFLECT(mc::engine::root_object, ())

using service_table =
    mdb::table<mc::engine::service_object,
               mdb::indexed_by<mdb::ordered_unique<&mc::engine::service_object::get_name>>>;

namespace mc::engine {

service_impl::service_impl() {
}

bool service_impl::init(service* s) {
    m_service        = s;
    m_service_object = mc::core::make_ref<service_object>();
    m_service_object->init(s);
    return true;
}

bool service_impl::start() {
    std::lock_guard lock(m_mutex);

    auto connection = mc::dbus::connection::open_session_bus(m_service->get_strand());
    if (!connection || !connection->start()) {
        elog("start service failed: cannot open dbus session");
        return false;
    }
    auto service_name = m_service->name();
    if (!connection->request_name(service_name)) {
        elog("start service failed: cannot request dbus name");
        return false;
    }

    connection->on_filter_message.connect([this](mc::dbus::message& msg) {
        return on_filter_message(msg);
    });

    m_connection   = connection;
    m_object_table = std::make_shared<service_object_table>(m_service->name());
    mc::engine::get_engine().register_table(m_object_table);
    auto  unique_name = connection->get_unique_name();
    auto& harbor      = mc::dbus::harbor::get_instance();
    // 如果harbor名未设置，则设置为"harbor.服务名"
    harbor.set_harbor_name_if_empty("harbor." + service_name);
    harbor.register_unique_name(unique_name, service_name);
    m_shm_tree   = new mc::dbus::shm_tree(m_service->get_strand(), harbor.get_harbor_name(),
                                          service_name, unique_name);
    auto handler = [this](std::string_view path, std::string_view interface,
                          std::string_view method, const mc::variants& args) {
        return invoke_method(path, interface, method, args);
    };
    harbor.register_method_handler(service_name, unique_name, handler);
    harbor.start();

    register_object(*m_service_object);

    return true;
}

void service_impl::stop() {
    std::lock_guard lock(m_mutex);

    if (m_connection) {
        m_connection->disconnect();
        m_connection.reset();
    }

    auto& engine   = mc::engine::engine::get_instance();
    auto  services = engine.get_table<service_table>("services");
    services->remove(m_service_object);
    m_object_table->clear();
    engine.unregister_table(m_object_table);
    auto& harbor = mc::dbus::harbor::get_instance();
    harbor.unregister_service(m_service->name());
}

void service_impl::register_object(abstract_object& obj) {
    m_object_table->add(mc::im::ref_ptr<abstract_object>(&obj));
    obj.set_service(m_service);
    m_connection->register_path(obj.get_object_path(), [this, &obj](auto& msg) {
        return on_path_message(msg, obj);
    });

    auto* owner = find_owner(m_root, obj.get_object_path());
    if (owner) {
        obj.set_owner(owner);
    }

    mc::dbus::shm_lock_call([this, &obj]() {
        m_shm_tree->register_object(obj);
    });
}

abstract_object* service_impl::find_owner(abstract_object& obj, std::string_view path) {
    if (path.empty() || path[0] != '/') {
        return nullptr;
    }

    // 如果不是 obj 的子路径，直接返回失败
    auto my_path = obj.get_object_path();
    if (!detail::path_starts_with(path, my_path)) {
        return nullptr;
    }

    // 在子对象中递归查找更接近的 owner
    auto child_objects = obj.get_managed_objects();
    auto it            = child_objects.lower_bound(path);
    if (it != child_objects.end() && it->first.size() == path.size()) {
        return it->second; // 如果精确匹配直接返回
    }

    // 如果没有精确匹配且已经到达 map 末尾或者找到的键不是 path 的前缀
    // 则需要向前查找可能的前缀
    if (it == child_objects.end() || !detail::path_starts_with(path, it->first)) {
        if (it == child_objects.begin()) {
            return &obj;
        }
        --it; // 向前查找可能的前缀
    }

    if (detail::path_starts_with(path, it->first)) {
        return find_owner(*(it->second), path);
    }

    return &obj;
}

void service_impl::unregister_object(std::string_view path) {
    m_connection->unregister_path(path);

    auto it = m_object_table->find<by_path>(path);
    if (it.is_end()) {
        return;
    }

    auto& obj = const_cast<abstract_object&>(*it);
    m_object_table->remove(mc::im::ref_ptr<abstract_object>(&obj));

    obj.set_owner(nullptr);
    obj.set_service(nullptr);

    mc::dbus::shm_lock_call([this, path]() {
        m_shm_tree->unregister_object(path);
    });
}

static mc::variant convert_method_result(const mc::variants& arr) {
    if (arr.empty()) {
        return mc::variant();
    }
    if (arr.size() == 1) {
        return arr[0];
    }
    return arr;
}

invoke_result service_impl::invoke_method(std::string_view path, std::string_view interface,
                                          std::string_view method, const variants& args) {
    auto& idx = m_object_table->get<by_path>();
    auto  it  = idx.find(std::string(path));
    if (it == idx.end()) {
        MC_THROW(mc::exception, "failed to find object: ${path}", ("path", path));
    }
    auto& obj = const_cast<abstract_object&>(*it);
    return obj.invoke(method, args, interface);
}

std::optional<mc::variant>
service_impl::shm_timeout_call(mc::milliseconds timeout, std::string_view service_name,
                               std::string_view path, std::string_view interface,
                               std::string_view method, std::string_view signature,
                               const variants& args) {
    auto result =
        m_shm_tree->timeout_call(timeout, service_name, path, interface, method, signature, args);
    if (result != std::nullopt) {
        return convert_method_result(result.value());
    }
    return std::nullopt;
}

mc::variant service_impl::timeout_call(mc::milliseconds timeout, std::string_view service_name,
                                       std::string_view path, std::string_view interface,
                                       std::string_view method, std::string_view signature,
                                       const variants& args) {
    if (service_name == m_service->name()) {
        // 自己服务的方法直接调用
        return invoke_method(path, interface, method, args);
    }
    auto result = shm_timeout_call(timeout, service_name, path, interface, method, signature, args);
    if (result != std::nullopt) {
        return result.value();
    }
    auto msg   = mc::dbus::message::new_method_call(service_name, path, interface, method);
    auto reply = m_connection->send_with_reply(std::move(msg), timeout);
    if (reply.is_valid() && reply.is_method_return()) {
        return convert_method_result(reply.read_args());
    }
    MC_THROW(mc::exception, "dbus call failed, error name: ${error_name}",
             ("error_name", reply.get_error_name()));
}

DBusHandlerResult service_impl::on_filter_message(mc::dbus::message& msg) {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult service_impl::on_path_message(mc::dbus::message& msg, abstract_object& obj) {
    if (msg.is_method_call()) {
        return on_method_call(obj, msg);
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult service_impl::on_method_call(abstract_object& object, mc::dbus::message& msg) {
    context ctx(*m_service, object);
    ctx.set_call_info(detail::dbus_call{msg});
    auto& info = std::get<detail::dbus_call>(ctx.get_call_info());

    context_stack::context call_ctx(m_service, ctx);
    try {
        auto interface_name = msg.get_interface();
        auto method_name    = msg.get_member();
        auto args           = msg.read_args();

        auto result = object.invoke(method_name, args, interface_name);
        if (!result.is_valid()) {
            info.response =
                mc::dbus::message::new_error(msg, errors::unknown_method.name, "method not found");
        } else {
            info.response = mc::dbus::message::new_method_return(msg);
            mc::dbus::signature_iterator it(result.method->get_result_signature());
            if (!it.at_end()) {
                info.response.writer().write_variant(it, result, 0);
            }
        }
    } catch (const mc::method_call_exception& e) {
        // 用户主动抛出的调用错误只记录 debug 日志
        dlog("method call failed: ${error}", ("error", e.what()));

        auto& err     = ctx.get_error();
        info.response = mc::dbus::message::new_error(msg, err.name, err.to_string());
    } catch (const std::exception& e) {
        // 未知错误记录 error 日志
        elog("unknow method call failed: ${error}", ("error", e.what()));
        // TODO:: 目前为了调试方便将 e.what() 作为错误内容访问，后续应该隐藏程序内部信息避免安全隐患
        info.response = mc::dbus::message::new_error(msg, errors::failed.name, e.what());
    }

    m_connection->send(std::move(info.response));
    return DBUS_HANDLER_RESULT_HANDLED;
}

service::service(std::string_view name) : mc::core::service_base(std::string(name)) {
    m_impl = std::make_unique<service_impl>();
}

service::~service() {
}

bool service::init(dict args) {
    MC_UNUSED(args);

    if (!dbus::validator::is_valid_bus_name(this->name())) {
        elog("initialize service failed: invalid service name ${name}", ("name", this->name()));
        return false;
    }

    if (!m_impl->init(this)) {
        elog("initialize service failed: initialization failed");
        return false;
    }
    if (args.contains("service_path")) {
        auto service_path = args["service_path"].as_string();
        m_impl->m_service_object->set_object_path(service_path);
        m_impl->m_service_object->set_name(args["service_name"].as_string());
    }

    auto& engine   = mc::engine::engine::get_instance();
    auto  services = engine.get_table<service_table>("services");
    services->add(m_impl->m_service_object);
    return true;
}

bool service::start() {
    try {
        return m_impl->start();
    } catch (const std::exception& e) {
        elog("start service failed: ${error}", ("error", e.what()));
        return false;
    }
}

bool service::stop() {
    m_impl->stop();
    return true;
}

void service::cleanup() {
}

bool service::is_healthy() const {
    return true;
}

void service::register_object(abstract_object& obj) {
    auto path = obj.get_object_path();
    MC_ASSERT(!path.empty(), "object path is empty");
    MC_ASSERT(mc::dbus::validator::is_valid_path(path), "invalid object path ${path}",
              ("path", path));

    MC_ASSERT(m_impl->m_object_table, "service is not started, cannot register object");
    m_impl->register_object(obj);
}

void service::unregister_object(std::string_view path) {
    m_impl->unregister_object(path);
}

service_object_table& service::get_object_table() const {
    return *m_impl->m_object_table;
}

// 解析对象路径
// 将路径求解放在 service 中的目的是，后续看是否需要将 service 和 engine 的属性
// 注册到路径表达式引擎，目前只允许路径表达式使用对象本身的属性以及表达式引擎的内建函数库
std::string service::resolve_object_path(std::string_view       path_pattern,
                                         const abstract_object& obj) {
    std::string path;
    if (mc::expr::lexer::is_template_string(path_pattern)) {
        // 是路径表达式，使用表达式引擎计算路径
        auto& expr_engine = mc::engine::engine::get_instance().get_expr_engine();
        auto  ctx         = expr_engine.make_context(const_cast<abstract_object*>(&obj));

        mc::expr::lexer  lex(path_pattern);
        auto             tokens = lex.scan_template_string_tokens();
        mc::expr::parser p(std::move(tokens));
        auto             node     = p.parse();
        auto             path_val = node->evaluate(ctx);
        MC_ASSERT_THROW(path_val.is_string(), mc::invalid_arg_exception,
                        "resolve object path ${path} failed", ("path", path_pattern));
        path = std::string(path_val.get_string());
    } else {
        // 是普通字符串，直接使用
        path = std::string(path_pattern);
    }

    mc::string::trim_inplace(path);

    // 是绝对路径则直接返回
    if (!path.empty() && path.front() == '/') {
        return path;
    }

    // 否则拼接上父路径
    auto parent = obj.get_parent();
    MC_ASSERT_THROW(parent, mc::invalid_arg_exception,
                    "object parent is nullptr or not abstract_object");
    auto parent_path = parent->get_object_path();

    std::string tmp;
    tmp.reserve(parent_path.size() + 1 + path.size());
    tmp = parent_path;
    tmp += "/";
    tmp += path;
    return tmp;
}

mc::variant service::timeout_call(mc::milliseconds timeout, std::string_view service_name,
                                  std::string_view path, std::string_view interface,
                                  std::string_view method, std::string_view signature,
                                  const mc::variants& args) {
    return m_impl->timeout_call(timeout, service_name, path, interface, method, signature, args);
}

std::optional<mc::variant>
service::shm_timeout_call(mc::milliseconds timeout, std::string_view service_name,
                          std::string_view path, std::string_view interface,
                          std::string_view method, std::string_view signature,
                          const mc::variants& args) {
    return m_impl->shm_timeout_call(timeout, service_name, path, interface, method, signature,
                                    args);
}

dbus::connection_ptr service::get_connection() {
    return m_impl->m_connection;
}

} // namespace mc::engine
