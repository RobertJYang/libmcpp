# 测试源文件列表
test_files = [
  # 主入口文件
  'main.cpp',

  # 根目录测试文件
  'test_futures.cpp',
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
  'db/test_path_iterator.cpp',
  'db/test_query_advanced.cpp',
  'db/test_table.cpp',
  'db/test_transaction.cpp',

  # dict 目录
  'dict/test_dict_construction.cpp',
  'dict/test_dict_conversion.cpp',
  'dict/test_dict_operations.cpp',
  'dict/test_dict_performance.cpp',
  'dict/test_dict_string_view.cpp',

  # engine 目录
  'engine/test_interface.cpp',

  # exception 目录
  'exception/test_exception.cpp',

  # filesystem 目录
  'filesystem/test_filesystem.cpp',

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
]

# 测试工具库 - 构建为独立的静态库
test_utilities_inc = include_directories('.')

test_utilities_sources = [
  'test_utilities/resource_manager.cpp',
]

test_utilities_lib = static_library(
  'mc_test_utilities',
  test_utilities_sources,
  include_directories: [incdir, test_utilities_inc],
  #dependencies: [boost_dep, boost_program_options_dep],
  install: false,
)

test_utilities_dep = declare_dependency(link_with: test_utilities_lib, include_directories: [test_utilities_inc])

# 创建测试可执行文件 - 现在依赖test_utilities
if cross_compile == false 
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
  ],
  install: true,
)
# 注册测试
test('libmcpp unit tests', test_exe)
endif
