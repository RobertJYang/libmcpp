# 测试工具库 - 构建为独立的静态库
test_utilities_inc = include_directories('.')

test_utilities_sources = [
  'resource_manager.cpp',
  'test_base.cpp',
]

test_utilities_lib = static_library(
  'mc_test_utilities',
  test_utilities_sources,
  include_directories: [incdir, test_utilities_inc],
  dependencies: [boost_dep, gtest_dep],
  install: false,
)

test_utilities_dep = declare_dependency(
  link_with: test_utilities_lib,
  include_directories: [test_utilities_inc],
  dependencies: [gtest_dep],
)
