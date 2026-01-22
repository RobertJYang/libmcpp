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
    include_directories: include_directories(boost_root / 'include'),
  )
else
  boost_dep = dependency('boost', method: 'pkg-config', required: true)
  message('使用系统 Boost')
endif

build_tests = get_option('tests')
meson_build = get_option('meson_build')

if build_tests
  gtest_dep = dependency('gtest', method: 'pkg-config', required: true)
endif

# 子目录
subdir('include')
subdir('src')

# 测试相关
if build_tests
  subdir('tests')
endif
