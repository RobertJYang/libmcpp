# 测试工具库 - 构建为独立的静态库
test_utilities_inc = include_directories('.')

test_utilities_sources = [
  'dbus_daemon_manager.cpp',
]

# 获取 conan 编译选项
enable_conan_compile = get_option('enable_conan_compile')
test_inc_dirs = [incdir, test_utilities_inc]

# 构建依赖列表
test_utilities_deps = [boost_dep, boost_program_options_dep, dbus_dep]

test_utilities_lib = static_library(
  'mc_test_utilities',
  test_utilities_sources,
  include_directories: test_inc_dirs,
  dependencies: test_utilities_deps + [mcbase_dep],
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
