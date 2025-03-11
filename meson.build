project('libmcpp', 'cpp',
  version : '0.1.0',
  default_options : ['cpp_std=c++17', 'warning_level=3', 
    'cpp_args=-fPIC -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare',
])

# 依赖项
boost_dep = dependency('boost', required : true)
boost_program_options_dep = dependency('boost', modules : ['program_options'], required : true)
gtest_dep = dependency('gtest', required : false)
gmock_dep = dependency('gmock', required : false)

# 如果系统中没有安装gtest，则使用subproject
if not gtest_dep.found() or not gmock_dep.found()
  gtest_proj = subproject('gtest')
  gtest_dep = gtest_proj.get_variable('gtest_dep')
  gmock_dep = gtest_proj.get_variable('gmock_dep')
endif

# 包含目录
incdir = include_directories('include')
boost_incdir = include_directories('/opt/homebrew/opt/boost/include')

# 子目录
subdir('include')
subdir('src')
subdir('tests')
subdir('examples')
subdir('programs/devmon')