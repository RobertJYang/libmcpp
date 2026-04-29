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
tests_utilities = get_option('tests_utilities')
build_test_utilities = build_tests or tests_utilities
enable_coverage = get_option('enable_coverage')
use_shm = get_option('use_shm')
with_benches = get_option('with_benches')
cpp_compiler = meson.get_compiler('cpp')

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

boost_root = get_option('boost_root')
if boost_root == ''
  boost_root = run_command('sh', '-c', 'echo $BOOST_ROOT', check: false).stdout().strip()
endif

if boost_root != ''
  boost_include_dir = boost_root
  if not cpp_compiler.has_header('boost/version.hpp', args: ['-I' + boost_include_dir], required: false)
    boost_include_dir = boost_root / 'include'
  endif
  if not cpp_compiler.has_header('boost/version.hpp', args: ['-I' + boost_include_dir], required: false)
    error('Boost headers not found in ' + boost_root)
  endif
  boost_dep = declare_dependency(compile_args: ['-I' + boost_include_dir])
  message('使用 Boost: ' + boost_include_dir)
else
  boost_dep = dependency('boost', required: false)
  if not boost_dep.found()
    if not cpp_compiler.has_header('boost/version.hpp', required: false)
      error('Boost headers not found')
    endif
    boost_dep = declare_dependency()
  endif
  message('使用系统 Boost')
endif

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

if build_test_utilities
  gtest_dep = dependency('gtest', method: 'pkg-config', required: true)
  mcbase_test_utilities_dep = mcbase_subproject.get_variable('test_utilities_dep')
else
  gtest_dep = declare_dependency()
  mcbase_test_utilities_dep = declare_dependency()
endif

mcengine_test_utilities_dep = declare_dependency()

dep_arr = [mcbase_dep]

subdir('include')
subdir('src')
meson.override_dependency('mcengine', mcengine_dep)

if build_test_utilities
  subdir('tests/test_utilities')
endif

if build_tests
  # 与 SHM 无关、任何 use_shm 配置都会编译的用例。
  mcengine_test_sources = files(
    'tests/test_error_engine.cpp',
    'tests/test_service_proto_node.cpp',
    'tests/test_interface.cpp',
    'tests/test_interface_inherit.cpp',
    'tests/test_object.cpp',
    'tests/test_utils.cpp',
    'tests/test_engine.cpp',
    'tests/test_match.cpp',
    'tests/test_property_concurrent_fix.cpp',
    'tests/test_context.cpp',
    'tests/test_path.cpp',
    'tests/test_service_utils.cpp',
    'tests/test_service_emit.cpp',
    'tests/test_proxy.cpp',
    'tests/test_path_iterator.cpp',
    'tests/test_path_pattern.cpp',
    'tests/test_async_invoke.cpp',
    'tests/test_result.cpp',
    'tests/test_std_interface.cpp',
  )

  # 仅在 use_shm=true 时才编译的用例：内部代码直接依赖 shm_object / shm_service
  # POD 布局、shm_runtime、shared_table 等 SHM 机制。关 SHM 时整体不进编译单元。
  if use_shm
    mcengine_test_sources += files(
      'tests/test_shm_object_journal.cpp',
      'tests/test_shm_object_layout.cpp',
      'tests/test_shm_object_ops.cpp',
      'tests/test_shm_service_layout.cpp',
      'tests/test_shm_service_ops.cpp',
      'tests/test_shared_table.cpp',
      'tests/test_endpoint_service.cpp',
      'tests/test_shm_engine_business.cpp',
      'tests/test_shm_engine_lifecycle.cpp',
      'tests/test_shm_service_takeover.cpp',
    )
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
    build_by_default: false,
  )

  subdir('tests')
endif

if with_benches
  subdir('benches')
endif
