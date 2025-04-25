project(
  'libmcpp',
  'cpp',
  version: '0.1.0',
  default_options: [
    'warning_level=3',
    'cpp_std=c++17',
    'libdir=lib',
    'cpp_args=-fPIC -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare -Wno-pedantic'
  ],
)

# 添加编译器选项
add_project_arguments('-Wall', '-Wextra', '-Wno-#warnings',  language: 'cpp')

# 配置选项
shared_mutex_max_readers = get_option('shared_mutex_max_readers')
if shared_mutex_max_readers != 0
  add_project_arguments(
    '-DMC_SHARED_MUTEX_MAX_READERS=' + shared_mutex_max_readers.to_string(),
    language: 'cpp',
  )
endif

# 依赖项
if not meson.is_cross_build()
  message('当前为本地构建')
  boost_dep = dependency('boost', method: 'pkg-config', required: true)
  boost_program_options_dep = dependency('boost', modules: ['program_options'], method: 'pkg-config', required: true)
else
  message('当前为交叉构建')
  # 交叉构建时创建一个空依赖，但确保变量可用
  boost_dep = dependency('', required: false)
  boost_program_options_dep = dependency('', required: false)
endif

# 尝试使用 pkg-config 查找依赖
dbus_dep = dependency('dbus-1', method: 'pkg-config', required: true)

# 包含目录
incdir = include_directories('include')

# 子目录
subdir('include')
subdir('src')
subdir('examples')
subdir('plugins')
subdir('programs/devmon')

if not meson.is_cross_build()
  message('构建测试用例')
  gtest_dep = dependency('gtest', required: false)
  subdir('tests')
endif