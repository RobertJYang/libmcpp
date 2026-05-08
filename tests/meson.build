subdir('algorithm')
subdir('compress')
subdir('crypto')
subdir('dbus')
subdir('debounce')
subdir('error_message')
subdir('log')
subdir('mdb')
subdir('lua')
subdir('variant')

# 获取 conan 编译选项
enable_conan_compile = get_option('enable_conan_compile')

# 测试源文件列表
test_files = [
  # 主入口文件
  'main.cpp',

  # 根目录测试文件
  'test_validate.cpp',
  'test_validator.cpp',
  'test_timer.cpp',

  # 各子目录的测试文件
  test_compress_files,
  test_crypto_files,
  test_dbus_files,
  test_debounce_files,
  test_log_files,
  test_variant_files,
  test_algorithm_files,
  test_mdb_files,
  test_error_message_files,
]

if enable_conan_compile
  test_files += 'test_json_wrapper.cpp'
endif

mcdbus_src_incdir = include_directories('../libraries/mcdbus/src')
# 构建 include_directories 列表（使用根目录定义的 stubdir_global）
test_inc_dirs = [incdir, srcdir, mcdbus_src_incdir]
if not enable_conan_compile
  test_inc_dirs += [stubdir_global]
endif

# 创建测试可执行文件
test_cpp_args = []
test_link_args = []
test_deps = [
  gtest_dep,
  test_utilities_dep,
  lua_dep,
  mcdbus_internal_dep,
  libmcpp_dep_with_link,
  openssl_dep,
]
if enable_coverage
  test_cpp_args += coverage_args
  test_link_args += coverage_link_args
endif
if enable_conan_compile
  json_dep = dependency('json', method: 'pkg-config', required: true)
  test_deps += [json_dep]
endif

# 测试程序使用静态库，可以访问所有符号（包括隐藏的）
test_exe = executable(
  'libmcpp_test',
  test_files,
  include_directories: test_inc_dirs,
  dependencies: test_deps,
  cpp_args: test_cpp_args,
  link_args: test_link_args,
  install: false,
  build_by_default: false,
)

# 注册测试
test(
  'libmcpp unit tests',
  test_exe,
  is_parallel: false,
  timeout: 300,
)