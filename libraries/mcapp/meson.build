project(
  'mcapp',
  'cpp',
  version: '0.1.0',
  default_options: [
    'warning_level=3',
    'cpp_std=c++17',
    'libdir=lib',
    'cpp_args=-fPIC -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare -Wno-pedantic',
    'default_library=static',
  ],
)

build_tests = get_option('tests')
tests_utilities = get_option('tests_utilities')
build_test_utilities = build_tests or tests_utilities
enable_coverage = get_option('enable_coverage')
use_old_shm = get_option('use_old_shm')

if enable_coverage
  coverage_args = [
    '--coverage',
    '-fprofile-arcs',
    '-ftest-coverage',
    '-fprofile-update=atomic',
  ]
  coverage_link_args = ['--coverage']
  add_project_arguments(coverage_args, language: 'cpp')
  add_project_link_arguments(coverage_link_args, language: 'cpp')
endif

# mcapp 的旧 shm 粘合层与 mcdbus 的旧 shm 实现是配对的：仅用于 src/meson.build
# 决定是否把 legacy_shm_binding.cpp 加入编译目标。
# C++ 侧统一用 mcdbus 透传过来的 MCDBUS_USE_OLD_SHM 宏来 #if 切换，避免与 mcdbus 出现
# 不同步的"两个真相"（如下方 use_old_shm 与 mcdbus:use_old_shm 通过 default_options 强同步）。

incdir = include_directories('include')
srcdir = include_directories('src')
mcapp_test_incdir = include_directories('include', 'tests')
mcapp_test_utilities_dep = declare_dependency()

mcbase_dep = dependency(
  'mcbase',
  fallback: ['mcbase', 'mcbase_dep'],
  default_options: [
    'default_library=shared',
    'tests=' + build_tests.to_string(),
    'tests_utilities=' + build_test_utilities.to_string(),
  ],
  required: true,
)
mcbase_subproject = subproject('mcbase', default_options: [
  'default_library=shared',
  'tests=' + build_tests.to_string(),
  'tests_utilities=' + build_test_utilities.to_string(),
])

mcengine_subproject = subproject('mcengine', default_options: [
  'default_library=static',
  'tests=' + build_tests.to_string(),
  'tests_utilities=' + build_test_utilities.to_string(),
])
mcengine_dep = mcengine_subproject.get_variable('mcengine_dep')
mcengine_use_shm = mcengine_subproject.get_variable('use_shm')

# mcapp:use_old_shm 与 mcengine:use_shm 互斥；mcdbus:use_old_shm 必须与 mcapp 一致。
# 这里通过 default_options 把 mcapp 的 use_old_shm 透传给 mcdbus 子项目，避免出现版本错配。
# 顶层 libmcpp meson.build 也会做同样的互斥校验（顶层 use_shm vs use_old_shm），
# 这里保留是为了 mcapp 作为子工程独立构建时仍能 catch 到错配。
if mcengine_use_shm and use_old_shm
  error(
    'mcapp:use_old_shm=true 与 mcengine:use_shm=true 互斥；',
    '请关闭 mcengine:use_shm 以启用旧 dbus/shm 兼容，或关闭 use_old_shm 使用新 SHM。',
  )
endif

mcdbus_subproject = subproject('mcdbus', default_options: [
  'default_library=static',
  'tests=' + build_tests.to_string(),
  'use_old_shm=' + use_old_shm.to_string(),
])
mcdbus_dep = mcdbus_subproject.get_variable('mcdbus_dep')

if build_tests
  gtest_dep = dependency('gtest', method: 'pkg-config', required: true)
else
  gtest_dep = declare_dependency()
endif

if build_test_utilities
  mcbase_test_utilities_dep = mcbase_subproject.get_variable('test_utilities_dep')
  mcengine_test_utilities_dep = mcengine_subproject.get_variable('mcengine_test_utilities_dep')
  raw_dbus_dep = dependency('dbus-1', method: 'pkg-config', required: true)
else
  mcbase_test_utilities_dep = declare_dependency()
  mcengine_test_utilities_dep = declare_dependency()
  raw_dbus_dep = declare_dependency()
endif

dep_arr = [mcbase_dep, mcengine_dep, mcdbus_dep]

subdir('include')
subdir('src')
meson.override_dependency('mcapp', mcapp_dep)

if build_test_utilities
  subdir('tests/test_utilities')
endif

if build_tests
  subdir('tests')
endif
