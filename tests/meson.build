# 包含子目录的 meson.build 文件
subdir('core')
subdir('db')
subdir('dbus')
subdir('dict')
subdir('engine')
subdir('exception')
subdir('expr')
subdir('filesystem')
subdir('futures')
subdir('im')
subdir('interprocess')
subdir('io')
subdir('log')
subdir('memory')
subdir('reflect')
subdir('runtime')
subdir('string')
subdir('sync')
subdir('variant')

# 测试源文件列表
test_files = [
  # 主入口文件
  'main.cpp',

  # 根目录测试文件
  'test_json.cpp',
  'test_pretty_name.cpp',
  'test_signal_slot.cpp',
  'test_time.cpp',

  # 各子目录的测试文件
  test_core_files,
  test_db_files,
  test_dbus_files,
  test_dict_files,
  test_engine_files,
  test_exception_files,
  test_expr_files,
  test_filesystem_files,
  test_futures_files,
  test_im_files,
  test_interprocess_files,
  test_io_files,
  test_log_files,
  test_memory_files,
  test_reflect_files,
  test_runtime_files,
  test_string_files,
  test_sync_files,
  test_variant_files,
]


enable_conan_complie = get_option('enable_conan_complie')
if enable_conan_complie
  test_files += ['dbus/test_shm_call.cpp']
endif

dbus_dep = dependency('dbus-1', method: 'pkg-config', required: true)

# 创建测试可执行文件 - 现在依赖test_utilities
test_exe = executable(
  'libmcpp_test',
  test_files,
  include_directories: incdir,
  dependencies: [
    libmcpp_dep,
    gtest_dep,
    dbus_dep,
    glib_dep,
    boost_dep,
    boost_program_options_dep,
    test_utilities_dep,
  ],
  cpp_args: [],
  install: true,
)

# 注册测试
test('libmcpp unit tests', test_exe)