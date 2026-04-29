project(
  'mcdbus',
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
enable_coverage = get_option('enable_coverage')
enable_conan_compile = get_option('enable_conan_compile')
use_old_shm = get_option('use_old_shm')
add_project_arguments(
  '-DENABLE_CONAN_COMPILE=' + enable_conan_compile.to_int().to_string(),
  language: 'cpp',
)

# 旧 dbus/shm 兼容机制（harbor + shm_tree）的真实实现头由 skynet conan 包提供，
# 仅在 conan 构建（macOS / 纯 meson 都拉不到 skynet）下可用。
# 校验策略：use_old_shm=true 时强制要求 skynet 必须找到；否则配置阶段直接 error，
# 避免出现"编入 harbor.cpp 但配着 mock_shm.h 头"的半残组合。
# use_old_shm=false 时 mcdbus 公共头里的 ::shm 类型仍由 header-only 的 mock_shm.h
# 提供 ABI 占位（dynamic_property::shm_prop 等字段始终需要 ::shm::weak_ptr 可见）。
skynet_dep = dependency('skynet', required: false)
if use_old_shm and not skynet_dep.found()
  error(
    'mcdbus:use_old_shm=true 需要 skynet（仅由 conan 构建提供 ::shm 真实实现头），',
    '当前 pkg-config 未找到 skynet。请改用 -Duse_old_shm=false（顶层同时 -Duse_shm=true），',
    '或在 conan 环境下构建。',
  )
endif

if use_old_shm
  add_project_arguments('-DMCDBUS_USE_OLD_SHM=1', language: 'cpp')
else
  add_project_arguments('-DMCDBUS_USE_OLD_SHM=0', language: 'cpp')
endif

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

incdir = include_directories('include')
srcdir = include_directories('src')

mcbase_dep = dependency(
  'mcbase',
  fallback: ['mcbase', 'mcbase_dep'],
  default_options: [
    'default_library=shared',
    'tests=' + build_tests.to_string(),
  ],
  required: true,
)

mcengine_dep = dependency(
  'mcengine',
  fallback: ['mcengine', 'mcengine_dep'],
  required: true,
)

dbus_dep = dependency('dbus-1', required: true)
glib_dep = dependency('glib-2.0', required: true)
boost_dep = dependency('boost', required: false)

libsomp_dep = dependency('libsomp', required: false)

dep_arr = [mcbase_dep, mcengine_dep, dbus_dep, glib_dep]
if boost_dep.found()
  dep_arr += boost_dep
endif
if use_old_shm
  dep_arr += skynet_dep
  if libsomp_dep.found()
    dep_arr += libsomp_dep
  endif
endif

subdir('include')
subdir('src')
meson.override_dependency('mcdbus', mcdbus_dep)
