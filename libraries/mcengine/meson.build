project(
  'mcengine',
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
use_shm = get_option('use_shm')
with_benches = get_option('with_benches')

if use_shm
  add_project_arguments('-DMCENGINE_USE_SHM=1', language: 'cpp')
else
  add_project_arguments('-DMCENGINE_USE_SHM=0', language: 'cpp')
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
mcengine_test_incdir = include_directories('include', 'tests')

boost_dep = dependency('boost', method: 'pkg-config', required: true)

mcbase_dep = dependency(
  'mcbase',
  fallback: ['mcbase', 'mcbase_dep'],
  default_options: [
    'default_library=shared',
    'tests=' + build_tests.to_string(),
  ],
  required: true,
)
mcbase_subproject = subproject('mcbase', default_options: [
  'default_library=shared',
  'tests=' + build_tests.to_string(),
])

if build_tests
  gtest_dep = dependency('gtest', method: 'pkg-config', required: true)
  mcbase_test_utilities_dep = mcbase_subproject.get_variable('test_utilities_dep')
else
  gtest_dep = declare_dependency()
  mcbase_test_utilities_dep = declare_dependency()
endif

# 非 SHM 配置下 mcengine_test_utilities 库不构建（见 tests/test_utilities/
# meson.build），这里给一个占位空 dep，保证 mcengine_test_lib 的 deps 语法
# 在两种配置下都合法。
mcengine_test_utilities_dep = declare_dependency()

dep_arr = [mcbase_dep]

subdir('include')
subdir('src')
meson.override_dependency('mcengine', mcengine_dep)

if build_tests
  # 与 SHM 无关、任何 use_shm 配置都会编译的用例。
  mcengine_test_sources = files(
    'tests/test_error_engine.cpp',
    'tests/test_service_proto_node.cpp',
    'tests/test_interface.cpp',
    'tests/test_interface_inherit.cpp',
    'tests/test_object.cpp',
    'tests/test_shm_object_journal.cpp',
    'tests/test_shm_object_layout.cpp',
    'tests/test_shm_object_ops.cpp',
    'tests/test_shm_service_layout.cpp',
    'tests/test_shm_service_ops.cpp',
    'tests/test_utils.cpp',
    'tests/test_engine.cpp',
    'tests/test_match.cpp',
    'tests/test_property_concurrent_fix.cpp',
    'tests/test_context.cpp',
    'tests/test_path.cpp',
    'tests/test_service_utils.cpp',
    'tests/test_service_emit.cpp',
    'tests/test_path_iterator.cpp',
    'tests/test_path_pattern.cpp',
    'tests/test_async_invoke.cpp',
    'tests/test_result.cpp',
    'tests/test_std_interface.cpp',
  )

  # 仅在 use_shm=true 时才编译的用例：内部大段代码依赖 shm_runtime / shared_table /
  # SHM-only 公共机制。关 SHM 时这些文件整体不进编译单元，避免原来靠
  # #if MCENGINE_USE_SHM + GTEST_SKIP 的"空壳"样板。
  if use_shm
    mcengine_test_sources += files(
      'tests/test_shared_table.cpp',
      'tests/test_shm_engine_business.cpp',
      'tests/test_shm_engine_lifecycle.cpp',
      'tests/test_shm_service_takeover.cpp',
    )

    # mcengine 侧 SHM 专属的测试辅助库（TestWithEngineShmRegion 等）。只在开
    # SHM 时编译，避免关 SHM 时引入 shm_runtime 等头的编译噪声。
    subdir('tests/test_utilities')
  endif

  mcengine_test_lib = static_library(
    'mcengine_test_lib',
    mcengine_test_sources,
    include_directories: [incdir, srcdir, mcengine_test_incdir],
    dependencies: [
      gtest_dep,
      boost_dep,
      mcengine_dep,
      mcbase_test_utilities_dep,
      mcengine_test_utilities_dep,
    ],
    install: false,
  )

  subdir('tests')
endif

if with_benches
  subdir('benches')
endif
