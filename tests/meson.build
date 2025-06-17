# 测试源文件列表
test_files = [
  # 主入口文件
  'main.cpp',

  # 根目录测试文件
  'test_json.cpp',
  'test_pretty_name.cpp',
  'test_signal_slot.cpp',
  'test_time.cpp',

  # expr 目录
  'expr/test_lexer.cpp',
  'expr/test_expr.cpp',
  'expr/test_context.cpp',
  'expr/test_object_expr.cpp',
  'expr/builtin/test_math.cpp',
  'expr/builtin/test_string.cpp',
  'expr/builtin/test_conversion.cpp',
  'expr/function/test_call.cpp',
  'expr/function/test_parser.cpp',

  # core 目录
  'core/test_service_manager.cpp',
  'core/test_singleton.cpp',
  'core/test_timer.cpp',
  'core/test_thread_safe_object.cpp',

  # db 目录
  'db/test_database.cpp',
  'db/test_index.cpp',
  'db/test_query_advanced.cpp',
  'db/test_table.cpp',
  'db/test_transaction.cpp',

  # dbus 目录
  'dbus/test_local_msg.cpp',
  'dbus/test_serialize.cpp',
  'dbus/test_gvariant_convert.cpp',
  'dbus/test_reflect.cpp',
  'dbus/test_dbus_message.cpp',
  'dbus/test_connection.cpp',

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

  # # futures 目录
  'futures/test_futures.cpp',
  'futures/test_state_pool.cpp',

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
  'reflect/test_reflect_base_class.cpp',

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
  'variant/test_variant_operations.cpp',

  # engine 目录
  'engine/test_interface.cpp',
  'engine/test_interface_inherit.cpp',
  'engine/test_object.cpp',
  'engine/test_utils.cpp',
  'engine/test_std_interface.cpp',
  'engine/test_engine.cpp',
  'engine/test_error_engine.cpp',
  'engine/test_property.cpp',
  'engine/test_path.cpp',
  'engine/test_path_iterator.cpp',
  'engine/test_async_invoke.cpp',
  'engine/test_result.cpp',

  # io 目录
  'io/test_io_buffer.cpp',
  'io/test_io_stream.cpp',

  # sync 目录
  'sync/test_shared_mutex.cpp',
  'sync/test_mutex_box.cpp',
  'sync/test_upgrade_mutex.cpp',
  'sync/test_small_mutex.cpp',

  # memory 目录
  'memory/test_shared_base.cpp',
  'memory/test_shared_ptr_basic.cpp',
  'memory/test_shared_object_ctor.cpp',
  'memory/test_shared_ptr_thread_safety.cpp',
  'memory/test_weak_ptr.cpp',

]

enable_shared_memory = get_option('enable_shared_memory')
if enable_shared_memory
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