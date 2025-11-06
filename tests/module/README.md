# Module 模块测试

Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.

## 概述

本文档描述了 Module 模块的测试用例。Module 模块负责模块（内置与动态库）的加载、管理与生命周期维护，核心组件包括：
- `mc::module::module_loader`：解析搜索路径、检查可读、加载/卸载动态库、查找导出符号。
- `mc::module::module_manager`：管理已加载模块与句柄、支持工厂内置模块与动态模块、卸载与资源清理。

## 测试文件列表

测试文件命名与 `src/module` 目录结构对应：

1. `test_module_loader.cpp` — 对应 `src/module/module_loader.cpp`，覆盖 `module_loader` 的路径处理、错误处理、回调流程、函数指针注入等核心逻辑。
2. `test_module_loader_branches.cpp` — 对应 `src/module/module_loader.cpp`，覆盖 `module_loader` 的额外分支，包括 `is_reused` 标志处理、环境变量处理、异常处理分支等。
3. `test_module_manager.cpp` — 对应 `src/module/module_manager.cpp`，覆盖 `module_manager` 的完整功能，包括内置模块加载、管理器功能、反射、便利函数、注销、模块卸载、多次加载、单例、搜索路径、已加载列表等。

## 重点测试用例简介

### test_module_loader.cpp
模块加载器核心功能测试（18 个用例）：
- `TestDefaultSearchPaths`：测试默认搜索路径。
- `TestSearchPathManagement`：测试搜索路径管理功能。
- `TestSetLoadLibFunc`：测试设置加载库函数。
- `TestLoadModuleSuccess`：测试成功加载模块。
- `TestLoadModuleLibraryNotFound`：测试库文件未找到的情况。
- `TestLoadModuleMissingExportFunction`：测试缺少导出函数的情况。
- `TestLoadModuleCallbackReturnsFalse`：测试回调返回 false 的情况。
- `TestMultipleSearchPathPatterns`：测试多种搜索路径模式。
- `TestComplexModuleName`：测试复杂模块名的处理。
- `TestSearchPathsGetterNonEmpty`：清空后添加路径，`search_paths()` 返回非空。
- `TestAddLoadPathsEmptyAndWhitespace`：空与仅空白路径被正确跳过。
- `TestAddLoadPathInvalid`：不包含 `?` 的模板路径被拒绝并记录告警。
- `TestLoadPathNotReadable`：`is_readable` 返回 `false` 时不尝试加载。
- `TestLoadPathDlopenFailed`：`load` 返回 `nullptr` 时失败。
- `TestLoadPathMissingCloseFunc`：缺少 `mc_close_*` 导出符号时失败并卸载。
- `TestLoadPathCallbackException`：回调抛异常时记录错误并卸载句柄。
- `TestAddSearchPathDuplicate`：重复路径不重复添加。
- `TestGetSetLoadLibFunc`：校验 `set/get_load_lib_func` 的一致性。

### test_module_loader_branches.cpp
模块加载器分支覆盖测试（3 个用例）：
- `TestLoadPathIsReusedBranch`：测试 `load_path` 中 `is_reused=true` 的分支，验证当回调设置 `is_reused=true` 时会调用 `unload` 减少引用计数。
- `TestConstructorEnvVar`：测试 `module_loader` 构造函数中从环境变量 `MC_MODULE_PATH` 读取搜索路径的分支。
- `TestIsReadableExceptionBranch`：测试 `is_readable` 抛出异常时的处理分支，验证异常正确传播。

### test_module_manager.cpp
模块管理器完整功能测试（17 个用例）：
- **基础功能**：
  - `TestModuleLoad`：测试模块加载功能，验证模块名称和版本信息。
  - `TestModuleManagerBasic`：测试模块管理器功能，包括加载状态查询和已加载模块列表。
  - `TestReflection`：测试反射功能，验证类型注册、对象创建和方法调用。
  - `TestUtilityFunctions`：测试便利函数，验证模块加载的缓存机制。
  - `TestUnRegisterBuiltinModule`：测试注销内置模块功能。
- **扩展功能**：
  - `TestModuleUnload`：卸载已加载模块。
  - `TestUnloadNonExistentModule`：卸载不存在的模块。
  - `TestMultipleLoadSameModule`：多次 `require` 返回同一实例。
  - `TestLoadNonExistentModule`：不存在模块返回空。
  - `TestModuleManagerSingleton`：`get_module_manager` 单例模式。
  - `TestAddSearchPath`：透传到 `module_loader`，不应崩溃。
  - `TestLoadedModulesList`：加载前后列表变化。
  - `TestModuleVersion`：默认版本字符串。
  - `TestModuleFactoryNull`：`get_factory()` 非空。
  - `TestUnloadByFactoryId`：通过 factory_id 卸载模块。
  - `TestMultipleUnloadSameModule`：多次卸载同一模块。
  - `TestModuleNameEdgeCases`：测试模块名称边界情况。

## 运行测试

运行所有测试：
```bash
meson test -C builddir
# 或
ninja -C builddir test
```

运行模块相关测试（使用 gtest 过滤器）：
```bash
./builddir/tests/libmcpp_test --gtest_filter="*Module*Test.*"

# 仅运行 loader 相关
./builddir/tests/libmcpp_test --gtest_filter="ModuleLoaderTest.*"

# 仅运行 loader 分支相关
./builddir/tests/libmcpp_test --gtest_filter="ModuleLoaderExtraBranchTest.*"

# 仅运行 manager 相关
./builddir/tests/libmcpp_test --gtest_filter="ModuleManagerTest.*"
```

## 覆盖率与关注点

当前重点覆盖以下分支与函数：
- `module_loader::add_load_paths/add_load_path` 的空白与重复路径分支。
- `module_loader::load_path` 中 `is_readable`/`load`/`sym`/回调/`is_reused` 的分支。
- `module_loader::is_readable` 对普通文件/目录的判定和异常处理。
- `module_loader` 构造函数中的环境变量处理分支。
- `module_manager::require/unload/loaded_modules/is_loaded` 的路径与并发友好清理流程。

目标：`src/module` 行覆盖率≥75%，并尽量减少无效与重复测试。

## 测试统计

- **测试文件总数**: 3 个
- **测试用例总数**: 38 个
  - `test_module_loader.cpp`: 18 个用例
  - `test_module_loader_branches.cpp`: 3 个用例
  - `test_module_manager.cpp`: 17 个用例
- **测试覆盖的功能模块**:
  - Module Loader（模块加载器）
  - Module Manager（模块管理器）
  - Built-in Module（内置模块）
  - Reflection（反射系统）

## 调试建议

```bash
# 打印详细日志
MESON_TEST_ARGS="--gtest_filter=ModuleLoaderTest.* --gtest_break_on_failure" \
meson test -C builddir --verbose --test-args="$MESON_TEST_ARGS"

# gdb 调试单个用例
gdb --args ./builddir/tests/libmcpp_test --gtest_filter="ModuleLoaderTest.TestLoadPathDlopenFailed"
```


