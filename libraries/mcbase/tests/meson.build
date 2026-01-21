# 测试工具库
subdir('test_utilities')

# 包含子目录的 meson.build 文件
subdir('array')
subdir('dict')
subdir('fmt')
subdir('log')
subdir('memory')
subdir('module')
subdir('reflect')
subdir('string')
subdir('variant')
subdir('db')
subdir('futures')
subdir('io')

# 测试源文件列表（不包括 main.cpp，因为主项目会有自己的 main）
mcbase_test_sources = [
  # 根目录单文件测试
  'test_pretty_name.cpp',
  'test_signal_slot.cpp',
  'test_json.cpp',
  'test_time.cpp',
  'test_error.cpp',

  # 各子目录的测试文件
  test_array_files,
  test_dict_files,
  test_fmt_files,
  test_futures_files,
  test_log_files,
  test_memory_files,
  test_module_files,
  test_reflect_files,
  test_string_files,
  test_variant_files,
  test_db_files,
  test_io_files,
]

# 构建 mcbase 测试静态库（.a 文件）
# 这个库将被主项目的测试程序链接
mcbase_test_cpp_args = []
mcbase_test_link_args = []
enable_coverage = get_option('enable_coverage')
if enable_coverage
  coverage_args = [
    '--coverage',
    '-fprofile-arcs',
    '-ftest-coverage',
    '-fprofile-update=atomic',
  ]
  coverage_link_args = ['--coverage']
  mcbase_test_cpp_args += coverage_args
  mcbase_test_link_args += coverage_link_args
endif

mcbase_test_lib = static_library(
  'mcbase_test',
  mcbase_test_sources,
  include_directories: [incdir, srcdir],
  dependencies: [
    mcbase_dep,
    gtest_dep,
    boost_dep,
    test_utilities_dep,
  ],
  cpp_args: mcbase_test_cpp_args,
  link_args: mcbase_test_link_args,
  install: false,  # 测试库不需要安装
)

# 导出测试库依赖，供主项目使用
mcbase_test_dep = declare_dependency(
  link_with: mcbase_test_lib,
  dependencies: [
    mcbase_dep,
    gtest_dep,
    boost_dep,
    test_utilities_dep,
  ],
)
