# 测试源文件列表
test_files = [
  'main.cpp',
  'test_json.cpp',
  # 'test_signal_slot.cpp',
  # 'test_time.cpp',
  # 'test_futures.cpp',
  # 'test_priority_queue.cpp',
  'interprocess/test_shared_mutex.cpp',
  # 'core/test_singleton.cpp',
  # 'core/test_service_manager.cpp',
  # 'string/test_string.cpp',
  # 'exception/test_exception.cpp',
  # 'log/test_log.cpp',
  # 'log/test_console_appender.cpp',
  # 'variant/test_variant_basic.cpp',
  # 'variant/test_variant_containers.cpp',
  # 'variant/test_variant_standard_containers.cpp',
  # 'variant/test_typed_variant.cpp',
  # 'variant/test_variant_equality.cpp',
  # 'variant/test_variant_comparison.cpp',
  # 'variant/test_variant_bool_operator.cpp',
  # 'variant/test_variant_visitor.cpp',
  # 'dict/test_dict_string_view.cpp',
  # 'dict/test_dict_construction.cpp',
  # 'dict/test_dict_operations.cpp',
  # 'dict/test_dict_conversion.cpp',
  # 'dict/test_dict_performance.cpp',
  # 'filesystem/test_filesystem.cpp',
  # 'database/test_path_iterator.cpp',
  # 'database/test_shared_tree_db.cpp',
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
  dependencies: [boost_dep, boost_program_options_dep],
  install: false
)

test_utilities_dep = declare_dependency(
  link_with: test_utilities_lib,
  include_directories: [test_utilities_inc]
)

# 创建测试可执行文件 - 现在依赖test_utilities
test_exe = executable('libmcpp_test',
  test_files,
  include_directories : incdir,
  dependencies : [libmcpp_dep, gtest_dep, boost_dep, boost_program_options_dep, test_utilities_dep],
  cpp_args : ['-Wno-self-assign-overloaded', '-Wno-unused-lambda-capture', '-Wno-self-move'],
  install : false
)

# 注册测试
test('libmcpp unit tests', test_exe)
