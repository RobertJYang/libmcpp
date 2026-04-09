# 定义模块测试文件列表
test_module_files = files(
    'test_module_loader.cpp',
    'test_module_loader_branches.cpp',
    'test_module_manager.cpp',
)


# 构建动态模块库（用于测试动态加载）
# 模块名 "mc.test.dynamic" 按 "." 分割后，模块加载器会依次尝试：
#   - tests/module/mc.dylib          ← 第1次迭代就会找到
#   - tests/module/mc/test.dylib
#   - tests/module/mc/test/dynamic.dylib
#
# 直接构建为 mc.dylib，模块加载器第一次迭代就能找到
# 使用 name_prefix: '' 去掉 lib 前缀
mock_dynamic_module = shared_library(
  'mc',
  'mock_dynamic_module.cpp',
  include_directories: [incdir, srcdir],
  dependencies: [mcbase_dep],
  install: false,
  name_prefix: '',
)
