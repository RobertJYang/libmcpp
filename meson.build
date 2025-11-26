project(
  'libmcpp',
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

# 添加编译器选项
add_project_arguments('-Wall', '-Wextra', '-fvisibility=hidden', language: 'cpp')

cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.get_id() == 'clang'
  add_project_arguments('-Wno-#warnings', language: 'cpp')
endif

# -Wno-deprecated-copy 选项只在 GCC 9+ 或 Clang 中支持
if cpp_compiler.get_id() == 'gcc'
  # 检查 GCC 版本是否 >= 9
  if cpp_compiler.version().version_compare('>=9.0')
    add_project_arguments('-Wno-deprecated-copy', language: 'cpp')
  endif
elif cpp_compiler.get_id() == 'clang'
  # Clang 支持该选项
  add_project_arguments('-Wno-deprecated-copy', language: 'cpp')
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
boost_dep = dependency('boost', method: 'pkg-config', required: true)
boost_program_options_dep = dependency(
  'boost',
  modules: ['program_options'],
  method: 'pkg-config',
  required: true,
)

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
subdir('modules')
subdir('tests/test_utilities')
if build_tests
  if meson_build
    subdir('examples')
  endif
  gtest_dep = dependency('gtest', method: 'pkg-config', required: true)
  subdir('tests')
endif