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
enable_coverage = get_option('enable_coverage')

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
mcapp_test_incdir = include_directories('include', 'tests')
mcapp_test_utilities_dep = declare_dependency()

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

mcengine_subproject = subproject('mcengine', default_options: [
  'default_library=static',
  'tests=' + build_tests.to_string(),
])
mcengine_dep = mcengine_subproject.get_variable('mcengine_dep')

mcdbus_dep = dependency(
  'mcdbus',
  fallback: ['mcdbus', 'mcdbus_dep'],
  required: true,
)

if build_tests
  gtest_dep = dependency('gtest', method: 'pkg-config', required: true)
  mcbase_test_utilities_dep = mcbase_subproject.get_variable('test_utilities_dep')
  mcengine_test_utilities_dep = mcengine_subproject.get_variable('mcengine_test_utilities_dep')
  raw_dbus_dep = dependency('dbus-1', method: 'pkg-config', required: true)
else
  gtest_dep = declare_dependency()
  mcbase_test_utilities_dep = declare_dependency()
  mcengine_test_utilities_dep = declare_dependency()
  raw_dbus_dep = declare_dependency()
endif

dep_arr = [mcbase_dep, mcengine_dep, mcdbus_dep]

subdir('include')
subdir('src')
meson.override_dependency('mcapp', mcapp_dep)

if build_tests
  subdir('tests/test_utilities')
  subdir('tests')
endif
