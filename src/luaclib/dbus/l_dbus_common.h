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

#ifndef MC_DBUS_L_DBUS_COMMON_H
#define MC_DBUS_L_DBUS_COMMON_H

#include <mc/dbus/bus_mode_impl.h>
#include <mc/dbus/connection.h>
#include <mc/dbus/enums.h>
#include <mc/dbus/error.h>
#include <mc/dbus/message.h>
#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/dbus/signal.h>
#include <mc/runtime.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <memory>
#include <mutex>

#include "l_dbus_connection.h"
#include "l_dbus_message.h"
#include "l_skynet_syms.h"

namespace mc::dbus::lua {

// io_context 包装器
struct io_context_wrapper {
    std::shared_ptr<mc::io_context> ctx;
    bool                            should_delete;

    io_context_wrapper(std::shared_ptr<mc::io_context> c, bool del = true)
        : ctx(std::move(c)), should_delete(del) {
    }
};

// DBus 连接包装器
struct dbus_wrapper {
    std::shared_ptr<mc::io_context>          io_ctx;
    std::unique_ptr<mc::dbus::bus_mode_impl> bus_impl;      // 阻塞式或非阻塞式实现（持有 connection）
    bool                                     owns_bus_impl; // 是否拥有 bus_impl 的所有权

    // 拥有模式构造函数：创建并拥有 bus_impl
    dbus_wrapper(mc::dbus::connection conn, std::shared_ptr<mc::io_context> ctx, bool is_blocking)
        : io_ctx(std::move(ctx)), owns_bus_impl(true) {
        // 根据模式创建对应的 bus_mode_impl
        if (is_blocking) {
            bus_impl = std::make_unique<mc::dbus::blocking_bus_impl>();
        } else {
            bus_impl = std::make_unique<mc::dbus::nonblocking_bus_impl>();
        }
        // 将 connection 初始化到 bus_impl
        bus_impl->init_connection(std::move(conn));
    }

    // 借用模式构造函数：借用现有的 bus_impl（不拥有所有权）
    dbus_wrapper(mc::dbus::bus_mode_impl* borrowed_impl, std::shared_ptr<mc::io_context> ctx)
        : io_ctx(std::move(ctx)), owns_bus_impl(false) {
        bus_impl.reset(borrowed_impl); // 设置指针但不拥有所有权
    }

    ~dbus_wrapper() {
        if (!owns_bus_impl && bus_impl) {
            // 借用模式：释放指针但不删除对象
            bus_impl.release();
        }
        // 拥有模式：unique_ptr 自动删除
    }
};

// 辅助函数：从 Lua 栈获取 io_context
inline std::shared_ptr<mc::io_context> get_io_context(lua_State* L, int index,
                                                      const char* metatable_name) {
    if (lua_isuserdata(L, index)) {
        auto* wrapper = static_cast<io_context_wrapper*>(luaL_checkudata(L, index, metatable_name));
        return wrapper->ctx;
    }
    return nullptr;
}

// 通用方法实现

// shutdown 方法（静态）
inline int dbus_shutdown(lua_State* L) {
    try {
        ::dbus_shutdown();
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to shutdown dbus: %s", e.what());
    }
}

// start 方法
inline int dbus_start(lua_State* L) {
    try {
        auto* wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));
        bool  result  = wrapper->bus_impl->start();
        lua_pushboolean(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to start connection: %s", e.what());
    }
}

// request_name 方法
// 返回两个参数：(success: bool, error: string|nil)
inline int dbus_request_name(lua_State* L) {
    try {
        auto*       wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));
        const char* name    = luaL_checkstring(L, 2);
        uint32_t    flags   = luaL_optinteger(L, 3, 0);

        // 通过 bus_impl 调用 request_name，会自动处理阻塞/非阻塞逻辑
        wrapper->bus_impl->request_name(name, flags);

        lua_pushboolean(L, true);
        lua_pushnil(L); // 成功时第二个参数为 nil
        return 2;
    } catch (const std::exception& e) {
        lua_pushboolean(L, false);   // 异常时第一个参数为 false
        lua_pushstring(L, e.what()); // 异常时第二个参数为错误信息
        return 2;
    }
}

// close 方法
inline int dbus_close(lua_State* L) {
    try {
        auto* wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));
        wrapper->bus_impl->disconnect();
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to close connection: %s", e.what());
    }
}

// flush 方法
inline int dbus_flush(lua_State* L) {
    try {
        auto* wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));
        wrapper->bus_impl->flush();
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to flush connection: %s", e.what());
    }
}

// dispatch 方法
inline int dbus_dispatch(lua_State* L) {
    try {
        auto* wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));
        wrapper->bus_impl->dispatch();
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to dispatch messages: %s", e.what());
    }
}

// __index 元方法（处理 conn 属性）
inline int dbus_index(lua_State* L) {
    // 检查是否是方法调用
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2); // 复制 key
    lua_rawget(L, -2);   // 查找方法表

    if (!lua_isnil(L, -1)) {
        return 1; // 找到方法，返回
    }

    lua_pop(L, 2); // 弹出 nil 和 metatable

    // 检查是否是属性访问
    const char* key = lua_tostring(L, 2);
    if (key) {
        try {
            auto* wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));

            // conn 属性：返回 connection 对象
            if (strcmp(key, "conn") == 0) {
                // 检查 metatable 名称来判断是阻塞式还是非阻塞式
                bool is_blocking = true; // 默认为阻塞式

                // 获取当前对象的 metatable
                if (lua_getmetatable(L, 1)) {
                    // 检查是否是 "dbus.nonblock" metatable
                    luaL_getmetatable(L, "dbus.nonblock");
                    if (lua_rawequal(L, -1, -2)) {
                        // 匹配到 nonblock metatable，是非阻塞式
                        is_blocking = false;
                    }
                    lua_pop(L, 2); // 弹出两个 metatable
                } else {
                    lua_pop(L, 1); // 弹出 nil
                }

                // 创建 connection 的拷贝并推入 Lua 栈，传递 is_blocking 标志
                return push_connection(L, wrapper->bus_impl->get_connection(), is_blocking);
            }
        } catch (const std::exception& e) {
            return luaL_error(L, "Failed to access property '%s': %s", key, e.what());
        }
    }

    lua_pushnil(L);
    return 1;
}

// __gc 元方法
inline int dbus_gc(lua_State* L) {
    auto* wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));
    wrapper->~dbus_wrapper();
    return 0;
}

// 通用的 dbus.xxx.new(arg) 实现
inline int dbus_new_impl(lua_State* L, const char* metatable_name, const char* io_name, bool is_blocking) {
    try {
        // 创建 io_context
        auto io_ctx = std::make_shared<mc::io_context>(1, io_name);

        mc::dbus::connection conn;

        if (lua_isnil(L, 1) || lua_isnoneornil(L, 1)) {
            // 参数为 nil：使用 connection 静态方法连接到会话总线
            conn = mc::dbus::connection::open_session_bus(*io_ctx);
        } else if (lua_isstring(L, 1)) {
            // 参数为字符串：根据内容判断是 bus 类型还是地址
            const char* arg = lua_tostring(L, 1);
            if (strcmp(arg, "session") == 0) {
                conn = mc::dbus::connection::open_session_bus(*io_ctx);
            } else if (strcmp(arg, "system") == 0) {
                conn = mc::dbus::connection::open_system_bus(*io_ctx);
            } else {
                // 假设是地址，使用 connection::open_address
                conn = mc::dbus::connection::open_address(*io_ctx, arg);
            }
        } else if (lua_isuserdata(L, 1)) {
            // 参数为 userdata：使用已有的 DBus 连接
            auto* raw_conn = *static_cast<DBusConnection**>(lua_touserdata(L, 1));
            conn           = mc::dbus::connection(*io_ctx, raw_conn, true);
        } else {
            return luaL_error(L, "Invalid argument type: expected nil, string, or userdata");
        }

        // 创建 userdata，传递 is_blocking 参数
        auto* wrapper = static_cast<dbus_wrapper*>(lua_newuserdata(L, sizeof(dbus_wrapper)));
        new (wrapper) dbus_wrapper(std::move(conn), io_ctx, is_blocking);

        // 设置 metatable
        luaL_getmetatable(L, metatable_name);
        lua_setmetatable(L, -2);

        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to create dbus: %s", e.what());
    }
}

// 通用的 dbus.xxx.open_user() 实现
inline int dbus_open_user_impl(lua_State*                                 L,
                               std::shared_ptr<mc::io_context>&           static_io_ctx,
                               const char*                                metatable_name,
                               bool                                       is_blocking,
                               std::function<void(mc::dbus::connection&)> post_connect = nullptr) {
    try {
        // 使用 connection 静态方法创建连接
        auto conn = mc::dbus::connection::open_session_bus(*static_io_ctx);

        // 执行连接后的操作（如果有）
        if (post_connect) {
            post_connect(conn);
        }

        // 创建 wrapper，传递 is_blocking 参数
        void* userdata = lua_newuserdata(L, sizeof(dbus_wrapper));
        new (userdata) dbus_wrapper(std::move(conn), static_io_ctx, is_blocking);

        // 设置 metatable
        luaL_getmetatable(L, metatable_name);
        lua_setmetatable(L, -2);

        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to open user session: %s", e.what());
    }
}

// 通用的 register_metatable 实现
inline void register_metatable_impl(lua_State*      L,
                                    const char*     metatable_name,
                                    const luaL_Reg* methods,
                                    lua_CFunction   index_func,
                                    lua_CFunction   gc_func) {
    // 创建 metatable
    luaL_newmetatable(L, metatable_name);

    // 注册方法到 metatable
    luaL_setfuncs(L, methods, 0);

    // 设置 __index 元方法
    lua_pushcfunction(L, index_func);
    lua_setfield(L, -2, "__index");

    // 设置 __gc
    lua_pushcfunction(L, gc_func);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1); // 弹出 metatable
}

// 辅助函数，从lua调用栈中解析一个新的消息
inline mc::dbus::message get_new_message(lua_State* L, int index, int arg_count) {
    const char* path   = luaL_checkstring(L, index);
    const char* iface  = luaL_checkstring(L, index + 1);
    const char* member = luaL_checkstring(L, index + 2);

    auto new_message = mc::dbus::message::new_signal(path, iface, member);

    if (arg_count >= index + 3) {
        const char* destination = luaL_checkstring(L, index + 3);
        new_message.set_destination(destination);
    }

    return new_message;
}

// 辅助函数，初始化harbor
inline void set_harbor(const char* harbor_name) {
    std::string_view harbor_name_view{harbor_name};
    auto&            harbor = mc::dbus::harbor::get_instance();
    harbor.set_harbor_name_if_empty(harbor_name_view);
    harbor.start();
}

// Lua回调包装器，用于存储Lua函数引用
struct lua_match_callback {
    lua_State* L;
    int        callback_ref;

    lua_match_callback(lua_State* l, int ref)
        : L(l), callback_ref(ref) {
    }

    ~lua_match_callback() {
        if (L && callback_ref != LUA_REFNIL) {
            luaL_unref(L, LUA_REGISTRYINDEX, callback_ref);
        }
    }

    // 禁止拷贝，只允许移动
    lua_match_callback(const lua_match_callback&)            = delete;
    lua_match_callback& operator=(const lua_match_callback&) = delete;
    lua_match_callback(lua_match_callback&& other) noexcept
        : L(other.L), callback_ref(other.callback_ref) {
        other.callback_ref = LUA_REFNIL;
    }
    lua_match_callback& operator=(lua_match_callback&& other) noexcept {
        if (this != &other) {
            if (L && callback_ref != LUA_REFNIL) {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref);
            }
            L                  = other.L;
            callback_ref       = other.callback_ref;
            other.callback_ref = LUA_REFNIL;
        }
        return *this;
    }

    void operator()(mc::dbus::message& msg) {
        if (!L || callback_ref == LUA_REFNIL) {
            return;
        }
        // 从registry获取Lua函数
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback_ref);
        // 将message推入栈（创建拷贝并move）
        mc::dbus::message msg_copy(msg); // 使用拷贝构造函数
        push_message(L, std::move(msg_copy));
        // 调用Lua函数，1个参数，0个返回值
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            // 错误处理：获取错误信息并记录（如果需要）
            const char* error = lua_tostring(L, -1);
            lua_pop(L, 1); // 弹出错误信息
            // 可以在这里记录错误日志
        }
    }
};

inline std::unordered_map<std::string, mc::dbus::shm_tree*> shm_tree_map;
inline std::mutex                                           shm_tree_map_mutex;

inline void set_shm_tree(const char* harbor_name, const char* service_name, const char* unique_name) {
    set_harbor(harbor_name);
    auto& harbor = mc::dbus::harbor::get_instance();
    harbor.register_unique_name(unique_name, service_name);
    create_shm_tree(harbor_name, service_name, unique_name);

    auto* shm_tree                          = new mc::dbus::shm_tree(harbor_name, service_name, unique_name);
    shm_tree_map[std::string(service_name)] = shm_tree;
}

// 获取一个shm_tree对象
inline mc::dbus::shm_tree* get_shm_tree(const char* unique_name, const char* harbor_name, const char* service_name) {
    // 第一次检查：不加锁快速检查
    std::string service_name_str(service_name);
    auto        it = shm_tree_map.find(service_name_str);
    if (it != shm_tree_map.end() && it->second != nullptr) {
        return it->second;
    }

    std::lock_guard<std::mutex> lock(shm_tree_map_mutex);

    it = shm_tree_map.find(service_name_str);
    if (it != shm_tree_map.end() && it->second != nullptr) {
        return it->second;
    }

    set_shm_tree(harbor_name, service_name, unique_name);
    it = shm_tree_map.find(service_name_str);
    if (it != shm_tree_map.end() && it->second != nullptr) {
        return it->second;
    }
    return nullptr;
}

// 从shm发送消息
inline int notify_signal(lua_State* L) {
    int n = lua_gettop(L);
    if (n < 4) {
        return luaL_error(L, "Too few args for send shm message");
    }
    auto* wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));

    try {
        auto new_message = get_new_message(L, 2, n);
        wrapper->bus_impl->notify_signal(new_message);
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Send shm message failed: %s", e.what());
    }
}

// 辅助函数，从lua调用栈中解析一个新的订阅规则
inline mc::dbus::match_rule get_new_rule(lua_State* L, int index, int arg_count) {
    const char* member    = luaL_checkstring(L, index);
    const char* interface = luaL_checkstring(L, index + 1);
    auto        new_rule  = mc::dbus::match_rule::new_signal(member, interface);
    if (arg_count >= index + 3) {
        if (lua_isboolean(L, index + 2) && lua_isstring(L, index + 3)) {
            bool        is_path_namespace      = lua_toboolean(L, index + 2);
            const char* path_or_path_namespace = lua_tostring(L, index + 3);
            if (is_path_namespace) {
                new_rule.with_path_namespace(path_or_path_namespace);
            } else {
                new_rule.with_path(path_or_path_namespace);
            }
        }
    } else {
        return new_rule;
    }
    if (arg_count >= index + 4) {
        if (lua_isstring(L, index + 4)) {
            const char* sender = lua_tostring(L, index + 4);
            new_rule.with_sender(sender);
        }
    } else {
        return new_rule;
    }
    if (arg_count >= index + 5) {
        if (lua_isnumber(L, index + 5)) {
            int type = lua_tonumber(L, index + 5);
            new_rule.with_type(static_cast<DBus::Match::MessageType>(type));
        }
    } else {
        return new_rule;
    }
    if (arg_count >= index + 6) {
        if (lua_isstring(L, index + 6)) {
            const char* destination = lua_tostring(L, index + 6);
            new_rule.with_destination(destination);
        }
    }
    return new_rule;
}

// 参数顺序：
//   1. dbus_wrapper
//   2. callback id
//   3. match id
//   4. harbor_name
//   5. 之后: get_new_rule 的参数 (member, interface, ...)
inline int add_match(lua_State* L) {
    int n = lua_gettop(L);
    // wrapper + callback id + harbor_name + member + interface
    if (n < 5) {
        return luaL_error(L, "Too few args for add match");
    }

    try {
        auto* wrapper     = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));
        int   callback_id = luaL_checknumber(L, 2);

        const char* harbor_name = luaL_checkstring(L, 3);

        auto new_rule = get_new_rule(L, 4, n);

        set_harbor(harbor_name);
        const char* unique_name  = wrapper->bus_impl->get_unique_name().data();
        const char* service_name = wrapper->bus_impl->get_service_name().data();
        auto*       shm_tree     = get_shm_tree(unique_name, harbor_name, service_name);
        if (!shm_tree) {
            return luaL_error(L, "Failed to get shm tree");
        }

        auto& harbor = mc::dbus::harbor::get_instance();
        // 先声明rule_id，然后通过引用捕获，在harbor.add_match返回后rule_id会被赋值
        uint64_t rule_id = 0;
        rule_id          = harbor.add_match(new_rule, [callback_id, &rule_id](mc::dbus::message& msg) mutable {
            auto skynet = mc::dbus::lua::SkynetSyms::get_instance();
            if (!skynet.skynet_send) {
                return;
            }
            dbus_message_ref(msg.get_dbus_message());
            constexpr std::size_t N = ((sizeof(std::uintptr_t) - 2) * 8);

            std::uintptr_t data      = reinterpret_cast<std::uintptr_t>(msg.get_dbus_message());
            uint32_t       data_high = data >> N;
            uint32_t       source    = (data_high << 16) | callback_id;
            std::size_t    sz        = data & ~(std::size_t(0xFFFF) << N);
            skynet.skynet_send_with_priority(nullptr, source, rule_id,
                                                      PTYPE_SIGNAL_PROCESS | PTYPE_TAG_DONTCOPY, 0, 0, sz, 0);
        });

        auto shm_harbor_name = harbor.get_harbor_name();
        shm_tree->add_shm_match(new_rule, shm_harbor_name, rule_id);

        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Add match failed: %s", e.what());
    }
}

} // namespace mc::dbus::lua

#endif // MC_DBUS_L_DBUS_COMMON_H
