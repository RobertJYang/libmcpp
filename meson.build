project('libmcpp', 'cpp',
    version : '0.1.0',
  default_options : ['cpp_std=c++17', 'warning_level=3', 
    'cpp_args=-fPIC -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare -Wno-gnu-zero-variadic-macro-arguments',
])

# 依赖项
boost_dep = dependency('boost', required : true)
boost_program_options_dep = dependency('boost', modules : ['program_options'], required : true)
gtest_dep = dependency('gtest', required : false)

# 包含目录
incdir = include_directories('include')

# 子目录
subdir('include')
subdir('src')
subdir('tests')
subdir('examples')
subdir('programs/devmon')