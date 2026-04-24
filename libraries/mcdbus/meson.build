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
add_project_arguments(
  '-DENABLE_CONAN_COMPILE=' + enable_conan_compile.to_int().to_string(),
  language: 'cpp',
)

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
boost_dep = dependency('boost', required: false, modules: ['system'])

dep_arr = [mcbase_dep, mcengine_dep, dbus_dep, glib_dep]
if boost_dep.found()
  dep_arr += boost_dep
endif

subdir('include')
subdir('src')
meson.override_dependency('mcdbus', mcdbus_dep)
