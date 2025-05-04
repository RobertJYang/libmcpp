# 测试源文件列表
test_files = [
  # 主入口文件
  'main.cpp',

  # 根目录测试文件
  'test_json.cpp',
  'test_pretty_name.cpp',
  'test_signal_slot.cpp',
  'test_time.cpp',

  # core 目录
  'core/test_service_manager.cpp',
  'core/test_singleton.cpp',

  # db 目录
  'db/test_database.cpp',
  'db/test_index.cpp',
  'db/test_query_advanced.cpp',
  'db/test_table.cpp',
  'db/test_transaction.cpp',

  # dict 目录
  'dict/test_dict_construction.cpp',
  'dict/test_dict_conversion.cpp',
  'dict/test_dict_operations.cpp',
  'dict/test_dict_performance.cpp',
  'dict/test_dict_string_view.cpp',

  # exception 目录
  'exception/test_exception.cpp',

  # filesystem 目录
  'filesystem/test_filesystem.cpp',

  # futures 目录
  'futures/test_futures.cpp',

  # im 目录
  'im/test_custom_allocator.cpp',
  'im/test_node.cpp',
  'im/test_node_list.cpp',
  'im/test_radix_tree.cpp',
  'im/test_radix_tree_iterator.cpp',
  'im/test_simple_node.cpp',
  'im/test_transaction.cpp',

  # interprocess 目录
  'interprocess/test_shared_mutex.cpp',

  # log 目录
  'log/test_console_appender.cpp',
  'log/test_log.cpp',

  # reflect 目录
  'reflect/test_nested_reflect.cpp',
  'reflect/test_reflect.cpp',
  'reflect/test_reflect_custom_name.cpp',
  'reflect/test_reflect_static_info.cpp',
  'reflect/test_custom_member_info.cpp',
  'reflect/test_method_call.cpp',

  # string 目录
  'string/test_format_benchmark.cpp',
  'string/test_string.cpp',

  # variant 目录
  'variant/test_typed_variant.cpp',
  'variant/test_variant_basic.cpp',
  'variant/test_variant_bool_operator.cpp',
  'variant/test_variant_comparison.cpp',
  'variant/test_variant_containers.cpp',
  'variant/test_variant_equality.cpp',
  'variant/test_variant_operations.cpp',
  'variant/test_variant_standard_containers.cpp',
  'variant/test_variant_visitor.cpp',
  'variant/test_variant_io.cpp',

  # engine 目录
  'engine/test_interface.cpp',
  'engine/test_object.cpp',
  'engine/test_utils.cpp',
  'engine/test_std_interface.cpp',
  'engine/test_engine.cpp',
  'engine/test_error_engine.cpp',

  # io 目录
  'io/test_io_buffer.cpp',
  'io/test_io_stream.cpp',

  # dbus 目录
  'dbus/test_path.cpp',
  'dbus/test_reflect.cpp',
  'dbus/test_dbus_message.cpp',
  'dbus/test_connection.cpp',
  'dbus/test_path_iterator.cpp',
]

dbus_dep = dependency('dbus-1', method: 'pkg-config', required: true)

# 创建测试可执行文件 - 现在依赖test_utilities
test_exe = executable(
  'libmcpp_test',
  test_files,
  include_directories: incdir,
  dependencies: [
    libmcpp_dep,
    gtest_dep,
    boost_dep,
    boost_program_options_dep,
    test_utilities_dep,
    dbus_dep,
  ],
  cpp_args: [],
  install: true,
)

# 注册测试
test('libmcpp unit tests', test_exe)