project(
  'mcbase',
  'cpp',
  version: '1.0.0',
  default_options: [
    'warning_level=3',
    'cpp_std=c++17',
    'libdir=lib',
    'cpp_args=-fPIC -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare -Wno-pedantic',
    'default_library=shared',
  ],
)

cpp_compiler = meson.get_compiler('cpp')

# 包含目录
incdir = include_directories('include')
srcdir = include_directories('src')

# ====================
# Boost 依赖检测和配置
# 1. 用户指定的 boost_root 选项
# 2. 环境变量 BOOST_ROOT
# 3. 系统 Boost
# ====================
boost_root = get_option('boost_root')
if boost_root == ''
  boost_root_env = run_command('sh', '-c', 'echo $BOOST_ROOT', check: false).stdout().strip()
  if boost_root_env != ''
    boost_root = boost_root_env
  endif
endif

if boost_root != ''
  message('使用 Boost: ' + boost_root)
  boost_dep = declare_dependency(
    include_directories: include_directories(boost_root),
  )
else
  if not cpp_compiler.has_header('boost/version.hpp', required: true)
    error('Boost headers not found')
  endif
  boost_dep = declare_dependency()
  message('使用系统 Boost')
endif

securec_option = get_option('securec')
securec_dep = dependency('securec', method: 'pkg-config', required: securec_option)
if securec_dep.found()
  message('mcbase 使用外部 securec')
else
  message('mcbase 使用内建 securec')
endif

build_tests = get_option('tests')
tests_utilities = get_option('tests_utilities')
enable_coverage = get_option('enable_coverage')
sanitizer_option = get_option('sanitizer')

mcbase_sanitizer_cpp_args = []
mcbase_sanitizer_link_args = []

if enable_coverage and sanitizer_option != 'none'
  error('mcbase 的 enable_coverage 与 sanitizer 不能同时启用')
endif

if sanitizer_option != 'none'
  sanitizer_flag = '-fsanitize=' + sanitizer_option
  sanitizer_cpp_args = [
    sanitizer_flag,
    '-fno-omit-frame-pointer',
  ]
  sanitizer_link_args = [
    sanitizer_flag,
  ]

  if cpp_compiler.get_supported_arguments(sanitizer_cpp_args).length() != sanitizer_cpp_args.length()
    error('当前 C++ 编译器不支持 mcbase sanitizer 编译参数：' + sanitizer_option)
  endif

  if cpp_compiler.get_supported_link_arguments(sanitizer_link_args).length() != sanitizer_link_args.length()
    error('当前 C++ 编译器不支持 mcbase sanitizer 链接参数：' + sanitizer_option)
  endif

  mcbase_sanitizer_cpp_args = sanitizer_cpp_args
  mcbase_sanitizer_link_args = sanitizer_link_args
  message('mcbase sanitizer 已启用：' + sanitizer_option)
endif

if build_tests or tests_utilities
  gtest_dep = dependency('gtest', method: 'pkg-config', required: true)
endif

# 子目录
subdir('include')
subdir('src')

if build_tests or tests_utilities
  subdir('tests/test_utilities')
endif

# 测试相关
if build_tests
  subdir('tests')
endif
