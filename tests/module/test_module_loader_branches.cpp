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

#include <gtest/gtest.h>
#include <mc/exception.h>
#include <mc/filesystem.h>
#include <test_utilities/test_base.h>

#include "module/include/module_loader.h"

#include <cstdlib>
#include <map>
#include <set>

namespace {

/**
 * @brief 模拟库加载器，用于测试 module_loader 的 is_reused 分支
 */
class mock_lib_loader_for_reused {
public:
    struct mock_lib_info {
        std::string           path;
        std::set<std::string> exported_symbols;
        void*                 mock_open_func  = nullptr;
        void*                 mock_close_func = nullptr;
    };

    void add_mock_lib(const std::string& path, const std::set<std::string>& symbols)
    {
        mock_lib_info info;
        info.path             = path;
        info.exported_symbols = symbols;
        info.mock_open_func   = reinterpret_cast<void*>(0x1000);
        info.mock_close_func  = reinterpret_cast<void*>(0x2000);
        m_mock_libs[path]     = info;
    }

    void clear()
    {
        m_mock_libs.clear();
        m_loaded_libs.clear();
        m_unload_count.clear();
    }

    size_t loaded_count() const
    {
        return m_loaded_libs.size();
    }

    size_t unload_count_for_path(const std::string& path) const
    {
        auto it = m_unload_count.find(path);
        return (it != m_unload_count.end()) ? it->second : 0;
    }

    static void* mock_load(std::string_view path, bool /*glb*/)
    {
        return instance().load_impl(path);
    }

    static void mock_unload(void* handle)
    {
        instance().unload_impl(handle);
    }

    static void* mock_sym(void* handle, std::string_view symbol_name)
    {
        return instance().sym_impl(handle, symbol_name);
    }

    static bool mock_is_readable(std::string_view path)
    {
        return instance().is_readable_impl(path);
    }

    static mock_lib_loader_for_reused& instance()
    {
        static mock_lib_loader_for_reused inst;
        return inst;
    }

private:
    void* load_impl(std::string_view path)
    {
        std::string path_str(path);
        auto        it = m_mock_libs.find(path_str);
        if (it == m_mock_libs.end()) {
            return nullptr;
        }

        void* handle          = reinterpret_cast<void*>(std::hash<std::string>{}(path_str));
        m_loaded_libs[handle] = path_str;
        return handle;
    }

    void unload_impl(void* handle)
    {
        auto it = m_loaded_libs.find(handle);
        if (it != m_loaded_libs.end()) {
            m_unload_count[it->second]++;
            m_loaded_libs.erase(it);
        }
    }

    void* sym_impl(void* handle, std::string_view symbol_name)
    {
        auto lib_it = m_loaded_libs.find(handle);
        if (lib_it == m_loaded_libs.end()) {
            return nullptr;
        }

        auto mock_it = m_mock_libs.find(lib_it->second);
        if (mock_it == m_mock_libs.end()) {
            return nullptr;
        }

        const auto& symbols = mock_it->second.exported_symbols;
        std::string symbol_str(symbol_name);
        if (symbols.find(symbol_str) == symbols.end()) {
            return nullptr;
        }

        if (symbol_str.find("mc_open_") == 0) {
            return mock_it->second.mock_open_func;
        } else if (symbol_str.find("mc_close_") == 0) {
            return mock_it->second.mock_close_func;
        }

        return reinterpret_cast<void*>(0x3000);
    }

    bool is_readable_impl(std::string_view path)
    {
        std::string path_str(path);
        return m_mock_libs.find(path_str) != m_mock_libs.end();
    }

    std::map<std::string, mock_lib_info> m_mock_libs;
    std::map<void*, std::string>         m_loaded_libs;
    std::map<std::string, size_t>        m_unload_count;
};

// 辅助函数：用于测试 is_readable 异常处理
bool throw_exception_is_readable(std::string_view)
{
    throw std::runtime_error("filesystem error");
}

/**
 * @brief 测试 module_loader 的额外分支覆盖
 */
class ModuleLoaderExtraBranchTest : public mc::test::TestBase {
protected:
    void SetUp() override
    {
        mock_lib_loader_for_reused::instance().clear();

        mock_funcs.load        = mock_lib_loader_for_reused::mock_load;
        mock_funcs.unload      = mock_lib_loader_for_reused::mock_unload;
        mock_funcs.sym         = mock_lib_loader_for_reused::mock_sym;
        mock_funcs.is_readable = mock_lib_loader_for_reused::mock_is_readable;
    }

    void TearDown() override
    {
        mock_lib_loader_for_reused::instance().clear();
    }

    mc::module::module_loader create_mock_loader()
    {
        mc::module::module_loader loader;
        loader.set_load_lib_func(mock_funcs);
        return loader;
    }

protected:
    mc::module::load_lib_func_t mock_funcs;
};

/**
 * @brief 测试 load_path 中 is_reused=true 的分支
 */
TEST_F(ModuleLoaderExtraBranchTest, TestLoadPathIsReusedBranch)
{
    auto loader = create_mock_loader();
    loader.clear_search_paths();
    loader.add_search_path("./?.so");

    mock_lib_loader_for_reused::instance().add_mock_lib(
        "./test/module.so",
        {"mc_open_test_module", "mc_close_test_module"});

    bool is_reused_set = false;
    auto callback      = [&](auto, bool& is_reused) -> bool {
        is_reused     = true; // 设置 is_reused=true，触发 unload 分支
        is_reused_set = true;
        return true;
    };

    bool result = loader.load_module("test.module", callback);
    EXPECT_TRUE(result) << "加载应成功";
    EXPECT_TRUE(is_reused_set) << "回调应被调用";

    // 当 is_reused=true 时，load_path 会调用 unload 减少引用计数
    // 检查 unload 是否被调用
    size_t unload_count = mock_lib_loader_for_reused::instance().unload_count_for_path("./test/module.so");
    EXPECT_EQ(unload_count, 1) << "is_reused=true 时应调用 unload";
}

/**
 * @brief 测试 module_loader 构造函数中的环境变量处理
 */
TEST_F(ModuleLoaderExtraBranchTest, TestConstructorEnvVar)
{
    // 设置环境变量
    const char* test_path = "  ;/invalid/path;/custom/path/?.so; /another/path/?.so  ";
    setenv("MC_MODULE_PATH", test_path, 1);

    // 创建新的 loader，它会读取环境变量
    mc::module::module_loader loader;

    // 检查环境变量路径是否被添加
    const auto& paths         = loader.search_paths();
    bool        found_custom  = false;
    bool        found_another = false;
    bool        found_invalid = false;
    for (const auto& path : paths) {
        if (path == "/custom/path/?.so") {
            found_custom = true;
        } else if (path == "/another/path/?.so") {
            found_another = true;
        } else if (path == "/invalid/path") {
            found_invalid = true;
        }
    }

    // 清理环境变量
    unsetenv("MC_MODULE_PATH");

    EXPECT_TRUE(found_custom) << "环境变量中的路径应被添加";
    EXPECT_TRUE(found_another) << "环境变量中的路径应被添加";
    EXPECT_FALSE(found_invalid) << "非法模板不应被保留";
}

/**
 * @brief 测试 is_readable 的异常处理分支
 */
TEST_F(ModuleLoaderExtraBranchTest, TestIsReadableExceptionBranch)
{
    // 这个测试通过 load_module 间接测试 is_readable 的异常处理
    // 由于 is_readable 是私有的，我们通过 load_path 间接测试
    // 但 load_path 也是私有的，所以我们通过 load_module 测试
    // 当 is_readable 抛出异常时，应该返回 false
    auto loader = create_mock_loader();
    loader.clear_search_paths();
    loader.add_search_path("./?.so");

    // 不添加模拟库，这样 is_readable 会返回 false
    // 但我们需要测试异常分支，所以我们需要一个会抛出异常的 is_readable
    mock_funcs.is_readable = throw_exception_is_readable;
    loader.set_load_lib_func(mock_funcs);

    bool callback_called = false;
    auto callback        = [&](auto, bool&) -> bool {
        callback_called = true;
        return true;
    };

    // 注意：load_path 中的 is_readable 调用没有捕获异常，所以异常会传播
    // 但根据代码，m_load_lib_func.is_readable 是在 load_path 中调用的
    // 如果它抛出异常，load_path 没有捕获，所以异常会传播到 load_module
    // 但 load_module 也没有捕获，所以异常会传播到测试
    EXPECT_THROW({
        bool result = loader.load_module("test.module", callback);
        EXPECT_FALSE(result);
        EXPECT_FALSE(callback_called);
    },
                 std::runtime_error);
}

} // anonymous namespace
