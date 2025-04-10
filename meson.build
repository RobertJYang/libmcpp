project('libmcpp', 'cpp',
    version : '0.1.0',
    default_options : [
        'warning_level=3', 
        'cpp_std=c++17',
        'libdir=lib',
        'cpp_args=-fPIC -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare -Wno-pedantic',
    ]
)

# 添加编译器选项
add_project_arguments('-Wall', '-Wextra', language : 'cpp')
add_global_link_arguments('-lstdc++fs', language: 'cpp')

# 配置选项
shared_mutex_max_readers = get_option('shared_mutex_max_readers')
if shared_mutex_max_readers != 0
  add_project_arguments('-DMC_SHARED_MUTEX_MAX_READERS=' + shared_mutex_max_readers.to_string(), language : 'cpp')
endif

# 依赖项
if meson.is_cross_build()
  message('当前为交叉编译环境')
  cross_compile = true
else
  message('当前为本地编译环境')
  cross_compile = false
endif
if cross_compile == false
  boost_dep = dependency('boost', method : 'pkg-config', required : true)
  boost_program_options_dep = dependency('boost', modules : ['program_options'], required : true)
endif

gtest_dep = dependency('gtest', required : false)

# 包含目录
incdir = include_directories('include')

# 子目录
subdir('include')
subdir('src')
subdir('tests')
subdir('examples')
subdir('plugins')
subdir('programs/devmon')
