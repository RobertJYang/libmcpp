# 包含子目录的 meson.build 文件

subdir('core')
subdir('crypto')
subdir('dbus')
subdir('debounce')
subdir('engine')
subdir('expr')
subdir('im')
subdir('interprocess')
subdir('sync')
subdir('lua')
subdir('variant')

# 获取 conan 编译选项
enable_conan_compile = get_option('enable_conan_compile')

# 测试源文件列表
test_files = [
  # 主入口文件
  'main.cpp',

  'test_validate.cpp',

  test_core_files,
  test_crypto_files,
  test_dbus_files,
  test_debounce_files,
  test_engine_files,
  test_expr_files,
  test_im_files,
  test_interprocess_files,
  test_sync_files,
  test_variant_files,
]

if enable_conan_compile
  test_files += 'test_json_wrapper.cpp'
endif

test_inc_dirs = [incdir, srcdir]
if not enable_conan_compile
  test_inc_dirs += [stubdir_global]
endif

dbus_dep = dependency('dbus-1', method: 'pkg-config', required: true)

# 创建测试可执行文件
test_cpp_args = []
test_link_args = []
if enable_coverage
  test_cpp_args += coverage_args
  test_link_args += coverage_link_args
endif

# 测试程序使用静态库，可以访问所有符号（包括隐藏的）
test_exe = executable(
  'libmcpp_test',
  test_files,
  include_directories: test_inc_dirs,
  dependencies: [
    mcbase_dep,
    mcbase_test_dep,
    dep_arr,
    gtest_dep,
    dbus_dep,
    glib_dep,
    boost_dep,
    boost_program_options_dep,
    test_utilities_dep,
  ],
  link_whole: [
    libmcpp_static_lib,
  ],
  cpp_args: test_cpp_args,
  link_args: test_link_args,
  install: true,
)

# 注册测试
test(
  'libmcpp unit tests',
  test_exe,
  depends: mock_dynamic_module,
)