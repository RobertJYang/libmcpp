# 测试工具库 - 构建为独立的静态库
test_utilities_inc = include_directories('.')

test_utilities_sources = [
  'dbus_daemon_manager.cpp',
]

# 注意：mcbase_dep 已经包含了 include 目录，不需要单独指定
test_utilities_lib = static_library(
  'mc_test_utilities',
  test_utilities_sources,
  include_directories: [incdir, test_utilities_inc],
  dependencies: [boost_dep, boost_program_options_dep, dbus_dep, mcbase_dep],
  install: true, # 设置为可安装
)

test_utilities_dep = declare_dependency(
  link_with: test_utilities_lib,
  include_directories: [test_utilities_inc],
  dependencies: [
    mcbase_dep,
    mcbase_test_utilities_dep,
  ],
)