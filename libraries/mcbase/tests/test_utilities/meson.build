# 测试工具库 - 构建为独立的静态库
test_utilities_inc = include_directories('.')

test_utilities_sources = [
  'resource_manager.cpp',
  'test_base.cpp',
]

install_dir = 'usr/lib64'
test_utilities_lib = static_library(
  'mcbase_test_utilities',
  test_utilities_sources,
  include_directories: [incdir, test_utilities_inc],
  dependencies: [boost_dep, gtest_dep, mcbase_dep],
  install: true,
  install_dir: install_dir,
  build_by_default: true,
)

test_utilities_dep = declare_dependency(
  link_with: test_utilities_lib,
  include_directories: [test_utilities_inc],
  dependencies: [gtest_dep, mcbase_dep],
)
