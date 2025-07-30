project(
  'libmcpp',
  'cpp',
  version: '0.1.0',
  default_options: [
    'warning_level=3',
    'cpp_std=c++17',
    'libdir=lib',
    'cpp_args=-fPIC -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare -Wno-pedantic -Wno-deprecated-copy',
    'default_library=static',
  ],
)

# 添加编译器选项
add_project_arguments('-Wall', '-Wextra', '-fvisibility=hidden', language: 'cpp')

if meson.get_compiler('cpp').get_id() == 'clang'
  add_project_arguments('-Wno-#warnings', language: 'cpp')
endif

# 配置选项
shared_mutex_max_readers = get_option('shared_mutex_max_readers')
if shared_mutex_max_readers != 0
  add_project_arguments(
    '-DMC_SHARED_MUTEX_MAX_READERS=' + shared_mutex_max_readers.to_string(),
    language: 'cpp',
  )
endif

meson_build = get_option('meson_build')
build_tests = get_option('tests')
enable_coverage = get_option('enable_coverage')

# 代码覆盖率配置
if enable_coverage
  coverage_args = []
  coverage_link_args = []
  coverage_args = ['--coverage', '-fprofile-arcs', '-ftest-coverage', '-fprofile-update=atomic']
  coverage_link_args = ['--coverage']
  add_project_arguments(coverage_args, language: 'cpp')
  add_project_link_arguments(coverage_link_args, language: 'cpp')
  message('代码覆盖率统计已启用')
endif

# 依赖项
if not meson.is_cross_build()
  boost_dep = dependency('boost', method: 'pkg-config', required: true)
  boost_program_options_dep = dependency(
    'boost',
    modules: ['program_options'],
    method: 'pkg-config',
    required: true,
  )
else
  boost_dep = dependency('', required: false)
  boost_program_options_dep = dependency('', required: false)
endif

# 设置编译宏
if meson.is_cross_build()
  current_buildtype = get_option('buildtype')
  if current_buildtype == 'debug'
    add_project_arguments('-DBUILD_TYPE=BUILD_TYPE_DEBUG', language: 'cpp')
  else
    add_project_arguments('-DBUILD_TYPE=BUILD_TYPE_RELEASE', language: 'cpp')
  endif
  add_project_arguments('-DLOGGING_PATH="/usr/lib64/liblogging.so"', language: 'cpp')
else
  add_project_arguments('-DBUILD_TYPE=BUILD_TYPE_DT', language: 'cpp')
  add_project_arguments('-DLOGGING_PATH=""', language: 'cpp')
  test_log_dir = meson.current_build_dir() + '/log'
  add_project_arguments('-DTEST_LOG_DIR="' + test_log_dir + '"', language: 'cpp')
endif

# 尝试使用 pkg-config 查找依赖
dbus_dep = dependency('dbus-1', method: 'pkg-config', required: true)
glib_dep = dependency('glib-2.0', method: 'pkg-config', required: true)

# 包含目录
incdir = include_directories('include')
srcdir = include_directories('src')

# 子目录
subdir('include')
subdir('src')
subdir('tests/test_utilities')
if build_tests
  if meson_build
    subdir('examples')
    subdir('plugins')
    subdir('programs/devmon')
  endif
  gtest_dep = dependency('gtest', required: false)
  subdir('tests')
endif