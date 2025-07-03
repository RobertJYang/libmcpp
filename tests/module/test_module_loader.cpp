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
#include <test_utilities/test_base.h>

#include "module/include/module_loader.h"

#include <map>
#include <set>

namespace {

/**
 * @brief 模拟库加载器，用于测试而不需要真实的动态库文件
 */
class mock_lib_loader {
public:
    // 模拟已存在的库和其导出函数
    struct mock_lib_info {
        std::string           path;
        std::set<std::string> exported_symbols;
        void*                 mock_open_func  = nullptr;
        void*                 mock_close_func = nullptr;
    };

    // 添加模拟库
    void add_mock_lib(const std::string& path, const std::set<std::string>& symbols) {
        mock_lib_info info;
        info.path             = path;
        info.exported_symbols = symbols;
        info.mock_open_func   = reinterpret_cast<void*>(0x1000); // 模拟函数指针
        info.mock_close_func  = reinterpret_cast<void*>(0x2000); // 模拟函数指针
        m_mock_libs[path]     = info;
    }

    // 清除所有模拟库
    void clear() {
        m_mock_libs.clear();
        m_loaded_libs.clear();
    }

    // 获取当前加载的库数量
    size_t loaded_count() const {
        return m_loaded_libs.size();
    }

    // 检查指定路径的库是否被加载
    bool is_loaded(const std::string& path) const {
        for (const auto& pair : m_loaded_libs) {
            if (pair.second == path) {
                return true;
            }
        }
        return false;
    }

    // 静态方法，用于实现打桩函数
    static void* mock_load(std::string_view path, bool /*glb*/) {
        return instance().load_impl(path);
    }

    static void mock_unload(void* handle) {
        instance().unload_impl(handle);
    }

    static void* mock_sym(void* handle, std::string_view symbol_name) {
        return instance().sym_impl(handle, symbol_name);
    }

    static bool mock_is_readable(std::string_view path) {
        return instance().is_readable_impl(path);
    }

    static mock_lib_loader& instance() {
        static mock_lib_loader inst;
        return inst;
    }

private:
    void* load_impl(std::string_view path) {
        std::string path_str(path);
        auto        it = m_mock_libs.find(path_str);
        if (it == m_mock_libs.end()) {
            return nullptr; // 库不存在
        }

        // 使用路径的哈希值作为句柄，确保唯一性
        void* handle          = reinterpret_cast<void*>(std::hash<std::string>{}(path_str));
        m_loaded_libs[handle] = path_str;
        return handle;
    }

    void unload_impl(void* handle) {
        m_loaded_libs.erase(handle);
    }

    void* sym_impl(void* handle, std::string_view symbol_name) {
        auto lib_it = m_loaded_libs.find(handle);
        if (lib_it == m_loaded_libs.end()) {
            return nullptr; // 句柄无效
        }

        auto mock_it = m_mock_libs.find(lib_it->second);
        if (mock_it == m_mock_libs.end()) {
            return nullptr;
        }

        const auto& symbols = mock_it->second.exported_symbols;
        std::string symbol_str(symbol_name);
        if (symbols.find(symbol_str) == symbols.end()) {
            return nullptr; // 符号不存在
        }

        // 返回模拟的函数指针
        if (symbol_str.find("mc_open_") == 0) {
            return mock_it->second.mock_open_func;
        } else if (symbol_str.find("mc_close_") == 0) {
            return mock_it->second.mock_close_func;
        }

        return reinterpret_cast<void*>(0x3000); // 其他符号的模拟指针
    }

    bool is_readable_impl(std::string_view path) {
        std::string path_str(path);
        return m_mock_libs.find(path_str) != m_mock_libs.end();
    }

    std::map<std::string, mock_lib_info> m_mock_libs;
    std::map<void*, std::string>         m_loaded_libs; // handle -> path
};

/**
 * @brief 测试 module_loader 类的功能
 */
class ModuleLoaderTest : public mc::test::TestBase {
protected:
    void SetUp() override {
        // 清理模拟库状态
        mock_lib_loader::instance().clear();

        // 设置默认的打桩函数
        mock_funcs.load        = mock_lib_loader::mock_load;
        mock_funcs.unload      = mock_lib_loader::mock_unload;
        mock_funcs.sym         = mock_lib_loader::mock_sym;
        mock_funcs.is_readable = mock_lib_loader::mock_is_readable;
    }

    void TearDown() override {
        mock_lib_loader::instance().clear();
    }

    // 辅助方法：创建一个配置好打桩函数的 loader
    mc::module::module_loader create_mock_loader() {
        mc::module::module_loader loader;
        loader.set_load_lib_func(mock_funcs);
        return loader;
    }

protected:
    mc::module::load_lib_func_t mock_funcs;
};

/**
 * @brief 测试构造函数和默认搜索路径
 */
TEST_F(ModuleLoaderTest, TestDefaultSearchPaths) {
    mc::module::module_loader loader;

    const auto& paths = loader.search_paths();
    EXPECT_FALSE(paths.empty()) << "默认搜索路径不应为空";

    // 检查是否包含预期的默认路径
    bool found_basic_so   = false;
    bool found_modules_so = false;

    for (const auto& path : paths) {
        if (path == "./?.so") {
            found_basic_so = true;
        } else if (path == "./modules/?.so") {
            found_modules_so = true;
        }
    }

    EXPECT_TRUE(found_basic_so) << "应包含 './?.so' 路径";
    EXPECT_TRUE(found_modules_so) << "应包含 './modules/?.so' 路径";
}

/**
 * @brief 测试添加和清除搜索路径
 */
TEST_F(ModuleLoaderTest, TestSearchPathManagement) {
    mc::module::module_loader loader;

    // 获取初始路径数量
    size_t initial_count = loader.search_paths().size();

    // 添加自定义路径
    loader.add_search_path("/custom/path/?.so");
    EXPECT_EQ(loader.search_paths().size(), initial_count + 1);

    // 检查路径是否正确添加
    const auto& paths        = loader.search_paths();
    bool        found_custom = std::find(paths.begin(), paths.end(), "/custom/path/?.so") != paths.end();
    EXPECT_TRUE(found_custom) << "自定义路径应被正确添加";

    // 重复添加相同路径，应该不会重复
    loader.add_search_path("/custom/path/?.so");
    EXPECT_EQ(loader.search_paths().size(), initial_count + 1) << "重复路径不应被添加";

    // 清除所有路径
    loader.clear_search_paths();
    EXPECT_TRUE(loader.search_paths().empty()) << "清除后搜索路径应为空";
}

/**
 * @brief 测试设置自定义库加载函数
 */
TEST_F(ModuleLoaderTest, TestSetLoadLibFunc) {
    mc::module::module_loader loader;

    // 使用 SetUp 中配置的 mock_funcs
    loader.set_load_lib_func(mock_funcs);

    // 此时还不能直接验证函数是否设置成功，需要通过 load_module 来验证
    // 下面的测试会验证这个功能
}

/**
 * @brief 测试模块加载成功的情况
 */
TEST_F(ModuleLoaderTest, TestLoadModuleSuccess) {
    auto loader = create_mock_loader();
    loader.clear_search_paths();
    loader.add_search_path("./?.so");

    // 添加模拟库，模拟 mc.test.module 模块
    // 根据实现，模块名 "mc.test.module" 会转换为导出函数 "mc_open_mc_test_module" 和 "mc_close_mc_test_module"
    mock_lib_loader::instance().add_mock_lib(
        "./mc/test/module.so",
        {"mc_open_mc_test_module", "mc_close_mc_test_module"});

    bool                         callback_called = false;
    mc::module::library_info_ptr received_info;

    auto callback = [&](auto info, bool&) -> bool {
        callback_called = true;
        received_info   = info;
        return true; // 表示成功处理
    };

    bool result = loader.load_module("mc.test.module", callback);

    EXPECT_TRUE(result) << "模块加载应该成功";
    EXPECT_TRUE(callback_called) << "回调函数应被调用";
    EXPECT_EQ(received_info->path, "./mc/test/module.so") << "路径应正确";
    EXPECT_NE(received_info->handle, nullptr) << "句柄应非空";
    EXPECT_NE(received_info->open_func, nullptr) << "open_func 应非空";
    EXPECT_NE(received_info->close_func, nullptr) << "close_func 应非空";
}

/**
 * @brief 测试模块加载失败的情况 - 库文件不存在
 */
TEST_F(ModuleLoaderTest, TestLoadModuleLibraryNotFound) {
    auto loader = create_mock_loader();
    loader.clear_search_paths();
    loader.add_search_path("./?.so");

    // 不添加任何模拟库

    bool callback_called = false;
    auto callback        = [&](auto, bool&) -> bool {
        callback_called = true;
        return true;
    };

    bool result = loader.load_module("nonexistent.module", callback);

    EXPECT_FALSE(result) << "不存在的模块加载应该失败";
    EXPECT_FALSE(callback_called) << "回调函数不应被调用";
}

/**
 * @brief 测试模块加载失败的情况 - 缺少导出函数
 */
TEST_F(ModuleLoaderTest, TestLoadModuleMissingExportFunction) {
    auto loader = create_mock_loader();
    loader.clear_search_paths();
    loader.add_search_path("./?.so");

    // 添加模拟库，但缺少必要的导出函数
    mock_lib_loader::instance().add_mock_lib(
        "./test/module.so",
        {"some_other_function"} // 缺少 mc_open_test_module 和 mc_close_test_module
    );

    bool callback_called = false;
    auto callback        = [&](auto, bool&) -> bool {
        callback_called = true;
        return true;
    };

    bool result = loader.load_module("test.module", callback);

    EXPECT_FALSE(result) << "缺少导出函数的模块加载应该失败";
    EXPECT_FALSE(callback_called) << "回调函数不应被调用";
}

/**
 * @brief 测试回调函数返回false的情况
 */
TEST_F(ModuleLoaderTest, TestLoadModuleCallbackReturnsFalse) {
    auto loader = create_mock_loader();
    loader.clear_search_paths();
    loader.add_search_path("./?.so");

    // 添加有效的模拟库
    mock_lib_loader::instance().add_mock_lib(
        "./test/module.so",
        {"mc_open_test_module", "mc_close_test_module"});

    bool callback_called = false;
    auto callback        = [&](auto, bool&) -> bool {
        callback_called = true;
        return false; // 回调返回false，表示拒绝加载
    };

    bool result = loader.load_module("test.module", callback);

    EXPECT_FALSE(result) << "回调返回false时，加载应该失败";
    EXPECT_TRUE(callback_called) << "回调函数应被调用";

    // 验证库被正确卸载（通过检查加载计数）
    EXPECT_EQ(mock_lib_loader::instance().loaded_count(), 0) << "库应被卸载";
}

/**
 * @brief 测试多种搜索路径模式
 */
TEST_F(ModuleLoaderTest, TestMultipleSearchPathPatterns) {
    auto loader = create_mock_loader();
    loader.clear_search_paths();

    // 添加多种路径模式
    loader.add_search_path("./?.so");
    loader.add_search_path("./?/init.so");
    loader.add_search_path("./lib/?.so");

    // 在第二种路径模式下添加模拟库
    mock_lib_loader::instance().add_mock_lib(
        "./my/module/init.so",
        {"mc_open_my_module", "mc_close_my_module"});

    bool                         callback_called = false;
    mc::module::library_info_ptr received_info;

    auto callback = [&](auto info, bool&) -> bool {
        callback_called = true;
        received_info   = info;
        return true;
    };

    bool result = loader.load_module("my.module", callback);

    EXPECT_TRUE(result) << "应能通过第二种路径模式找到模块";
    EXPECT_TRUE(callback_called) << "回调函数应被调用";
    EXPECT_EQ(received_info->path, "./my/module/init.so") << "路径应匹配第二种模式";
}

/**
 * @brief 测试复杂模块名的处理
 */
TEST_F(ModuleLoaderTest, TestComplexModuleName) {
    auto loader = create_mock_loader();
    loader.clear_search_paths();
    loader.add_search_path("./?.so");

    // 测试复杂的模块名：mc.devices.network.driver
    // 根据实现，这会转换为导出函数：mc_open_mc_devices_network_driver 和 mc_close_mc_devices_network_driver
    mock_lib_loader::instance().add_mock_lib(
        "./mc/devices/network/driver.so",
        {"mc_open_mc_devices_network_driver", "mc_close_mc_devices_network_driver"});

    bool callback_called = false;
    auto callback        = [&](auto, bool&) -> bool {
        callback_called = true;
        return true;
    };

    bool result = loader.load_module("mc.devices.network.driver", callback);

    EXPECT_TRUE(result) << "复杂模块名应能正确处理";
    EXPECT_TRUE(callback_called) << "回调函数应被调用";
}

} // anonymous namespace