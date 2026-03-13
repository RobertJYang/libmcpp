# 测试工具库 - 构建为独立的静态库
test_utilities_inc = include_directories('.')

test_utilities_sources = [
  'resource_manager.cpp',
  'dbus_daemon_manager.cpp',
  'test_base.cpp',
]

# 获取 conan 编译选项
enable_conan_compile = get_option('enable_conan_compile')

# 获取 securec 依赖（参考 src/luaclib/meson.build）
securec_dep = dependency('securec', required: false)

# 构建 include_directories 列表（使用根目录定义的 stubdir_global）
test_inc_dirs = [incdir, test_utilities_inc]
if not enable_conan_compile
  test_inc_dirs += [stubdir_global]
endif

# 构建依赖列表
test_utilities_deps = [boost_dep, boost_program_options_dep, dbus_dep]
if securec_dep.found()
  test_utilities_deps += [securec_dep]
endif

test_utilities_lib = static_library(
  'mc_test_utilities',
  test_utilities_sources,
  include_directories: test_inc_dirs,
  dependencies: test_utilities_deps,
  install: true, # 设置为可安装
)

test_utilities_dep = declare_dependency(link_with: test_utilities_lib, include_directories: [test_utilities_inc])