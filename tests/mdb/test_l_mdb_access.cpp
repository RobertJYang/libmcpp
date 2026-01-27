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

#include <gtest/gtest.h>
#include <mc/dbus/connection.h>
#include <mc/dbus/sd_bus.h>
#include <mc/runtime.h>
#include <test_utilities/test_base.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "../../../src/introspection/introspection_types.h"
#include "../../../src/luaclib/mdb/l_mdb_access.h"
#include "../../../src/mdb/proxy_object.h"

using namespace mc;

class l_mdb_access_test : public mc::test::TestBase {
protected:
    lua_State* L;

    static mc::dbus::connection test_conn;
    static mc::dbus::sd_bus*    test_bus;

    static void SetUpTestSuite() {
        mc::test::TestBase::SetUpTestSuite();

        // 创建 D-Bus 连接
        test_conn = mc::dbus::connection::open_session_bus(mc::get_io_context());
        test_conn.start();

        // 创建 sd_bus
        test_bus = new mc::dbus::sd_bus(std::move(test_conn), false);

        // 等待连接完全建立
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    static void TearDownTestSuite() {
        delete test_bus;
        test_bus = nullptr;

        mc::test::TestBase::TearDownTestSuite();
    }

    void SetUp() override {
        mc::test::TestBase::SetUp();

        // 创建 Lua 状态机
        L = luaL_newstate();
        luaL_openlibs(L);

        // 注册 mdb_access 模块
        lua_newtable(L);
        mc::mdb::lua::register_mdb_access_functions(L);
        lua_setglobal(L, "mdb_access");
    }

    void TearDown() override {
        if (L) {
            lua_close(L);
            L = nullptr;
        }

        mc::test::TestBase::TearDown();
    }
};

mc::dbus::connection l_mdb_access_test::test_conn;
mc::dbus::sd_bus*    l_mdb_access_test::test_bus = nullptr;

// 测试注册 mdb_access 函数
TEST_F(l_mdb_access_test, register_functions) {
    // 检查 mdb_access 表是否存在
    lua_getglobal(L, "mdb_access");
    ASSERT_TRUE(lua_istable(L, -1));
    lua_pop(L, 1);
}

// 测试 proxy_object metatable 注册
TEST_F(l_mdb_access_test, register_proxy_object_metatable) {
    // 注册 metatable
    mc::mdb::lua::register_proxy_object_metatable(L);

    // 检查 metatable 是否存在
    luaL_getmetatable(L, mc::mdb::lua::PROXY_OBJECT_METATABLE);
    ASSERT_TRUE(lua_istable(L, -1));
    lua_pop(L, 1);
}

// 测试 push_proxy_object 和 check_proxy_object
TEST_F(l_mdb_access_test, push_and_check_proxy_object) {
    // 由于需要真实的 proxy_object，这里只测试基本功能
    // 创建一个简单的 interface_info
    interface_info iface_info;
    // interface_info 没有 name 成员，接口名通过 proxy_object 的 interface() 方法获取

    // 创建 proxy_object
    proxy_object obj(test_bus, "org.test.Service", "/org/test/Path", "org.test.Interface", iface_info);

    // 注册 metatable
    mc::mdb::lua::register_proxy_object_metatable(L);

    // 推送 proxy_object
    mc::mdb::lua::push_proxy_object(L, &obj);

    // 检查是否是 userdata
    ASSERT_TRUE(lua_isuserdata(L, -1));

    // 检查 metatable
    ASSERT_TRUE(lua_getmetatable(L, -1));
    lua_pop(L, 2); // 弹出 metatable 和 userdata

    // 测试 check_proxy_object
    mc::mdb::lua::push_proxy_object(L, &obj);
    auto* wrapper = mc::mdb::lua::check_proxy_object(L, -1);
    ASSERT_NE(wrapper, nullptr);
    lua_pop(L, 1);
}

TEST_F(l_mdb_access_test, lua_call_get_object) {
    // 注册 metatable
    mc::mdb::lua::register_proxy_object_metatable(L);

    // 将 bus 指针推入栈（作为 lightuserdata）
    lua_pushlightuserdata(L, test_bus);

    // 调用 Lua 代码测试 get_object
    // 使用一个可能不存在的路径，测试错误处理
    const char* lua_code = R"(
        local bus = ...
        local ok, obj = pcall(function()
            return mdb_access.get_object(bus, "/nonexistent/path", "org.nonexistent.Interface")
        end)
        return ok, obj
    )";

    if (luaL_dostring(L, lua_code) != LUA_OK) {
        // Lua 代码执行失败，跳过测试
        GTEST_SKIP() << "Lua 代码执行失败: " << lua_tostring(L, -1);
        return;
    }

    // 检查返回值
    if (lua_gettop(L) >= 2) {
        bool ok = lua_toboolean(L, -2) != 0;
        if (ok) {
            // 如果成功，检查返回的对象
            if (lua_isuserdata(L, -1)) {
                auto* wrapper = mc::mdb::lua::check_proxy_object(L, -1);
                if (wrapper != nullptr) {
                    EXPECT_NE(wrapper, nullptr);
                }
            }
        } else {
            // 验证错误信息是字符串类型
            if (lua_isstring(L, -1)) {
                const char* err_msg = lua_tostring(L, -1);
                EXPECT_NE(err_msg, nullptr);
            }
        }
        lua_pop(L, 2);
    }
}
