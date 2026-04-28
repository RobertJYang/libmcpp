project(
  'mcexpr',
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
cpp_compiler = meson.get_compiler('cpp')

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
mcexpr_test_incdir = include_directories('include', 'tests')

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
mcengine_dep = dependency(
  'mcengine',
  fallback: ['mcengine', 'mcengine_dep'],
  default_options: [
    'default_library=static',
    'tests=' + build_tests.to_string(),
  ],
  required: true,
)
mcengine_subproject = subproject('mcengine', default_options: [
  'default_library=static',
  'tests=' + build_tests.to_string(),
])
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
  mcengine_test_utilities_dep = mcengine_subproject.get_variable('mcengine_test_utilities_dep')
else
  gtest_dep = declare_dependency()
  mcbase_test_utilities_dep = declare_dependency()
  mcengine_test_utilities_dep = declare_dependency()
endif

dep_arr = [mcbase_dep]

subdir('include')
subdir('src')
meson.override_dependency('mcexpr', mcexpr_dep)

if build_tests
  mcexpr_test_sources = files(
    'tests/builtin/test_conversion.cpp',
    'tests/builtin/test_math.cpp',
    'tests/builtin/test_string.cpp',
    'tests/function/test_call.cpp',
    'tests/function/test_collection.cpp',
    'tests/function/test_parser.cpp',
    'tests/test_context.cpp',
    'tests/test_expr.cpp',
    'tests/test_lexer.cpp',
    'tests/test_node.cpp',
    'tests/test_object_expr.cpp',
    'tests/test_parser.cpp',
    'tests/test_property.cpp',
    'tests/test_property_processors.cpp',
  )

  mcexpr_test_lib = static_library(
    'mcexpr_test_lib',
    mcexpr_test_sources,
    include_directories: [incdir, srcdir, mcexpr_test_incdir],
    dependencies: [
      gtest_dep,
      boost_dep,
      mcexpr_dep,
      mcbase_test_utilities_dep,
      mcengine_test_utilities_dep,
    ],
    install: false,
    build_by_default: false,
  )

  subdir('tests')
endif
