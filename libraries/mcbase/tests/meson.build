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
subdir('im')
subdir('intrusive')
subdir('io')
subdir('quark')
subdir('runtime')
subdir('shm')
subdir('metric')
subdir('protocol')

# 测试源文件列表（不包括 main.cpp，因为主项目会有自己的 main）
mcbase_test_sources = [
  # 根目录单文件测试
  'test_pretty_name.cpp',
  'test_signal_slot.cpp',
  'test_future_public_header.cpp',
  'test_json.cpp',
  'test_json_public_header.cpp',
  'test_time.cpp',
  'test_error.cpp',
  'test_filesystem.cpp',

  # 各子目录的测试文件
  test_array_files,
  test_dict_files,
  test_fmt_files,
  test_futures_files,
  test_im_files,
  test_intrusive_files,
  test_log_files,
  test_memory_files,
  test_module_files,
  test_reflect_files,
  test_string_files,
  test_variant_files,
  test_db_files,
  test_io_files,
  test_protocol_files,
  test_quark_files,
  test_runtime_files,
  test_shm_files,
  test_metric_files,
]

# 构建 mcbase 测试静态库（.a 文件）
# 这个库将被主项目的测试程序链接
mcbase_test_cpp_args = mcbase_sanitizer_cpp_args
mcbase_test_link_args = mcbase_sanitizer_link_args
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
  build_by_default: false,
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

# ====================
# 构建 mcbase 独立的测试可执行文件
# ====================

# 创建一个简单的 main.cpp 用于 mcbase 测试
mcbase_test_main = configure_file(
  input: files('main.cpp.in'),
  output: 'main.cpp',
  copy: true,
)

# mcbase 测试可执行文件
mcbase_test_exe = executable(
  'mcbase_test',
  mcbase_test_main,
  link_whole: [
    mcbase_test_lib,
  ],
  include_directories: [incdir, srcdir],
  dependencies: [
    gtest_dep,
    boost_dep,
  ],
  link_with: mcbase_static_lib,
  cpp_args: mcbase_test_cpp_args,
  link_args: mcbase_test_link_args,
  install: false,
  build_by_default: false,
)

# 注册 mcbase 测试：先构建 mock 动态库；MC_BUILD_ROOT 与 dynamic-sdk-phase1/主仓一致，供 ModuleManagerTest 解析模块路径
mcbase_test_env = environment()
mcbase_test_env.set('MC_BUILD_ROOT', meson.project_build_root())

test(
  'mcbase unit tests',
  mcbase_test_exe,
  depends: mock_dynamic_module,
  env: mcbase_test_env,
  is_parallel: false,
  timeout: 300,
)

# ====================
# 独立 fork 测试可执行文件（fork 会污染进程级 runtime 状态，必须隔离）
# ====================

mcbase_test_fork_main = configure_file(
  input: files('main.cpp.in'),
  output: 'main_fork.cpp',
  copy: true,
)

mcbase_test_fork_exe = executable(
  'mcbase_test_fork',
  [mcbase_test_fork_main, test_shm_fork_files],
  include_directories: [incdir, srcdir],
  dependencies: [
    gtest_dep,
    boost_dep,
    mcbase_dep,
    test_utilities_dep,
  ],
  cpp_args: mcbase_test_cpp_args,
  link_args: mcbase_test_link_args,
  install: false,
  build_by_default: false,
)

test(
  'mcbase fork tests',
  mcbase_test_fork_exe,
  env: mcbase_test_env,
  is_parallel: false,
  timeout: 300,
)
