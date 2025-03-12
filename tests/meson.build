# 测试源文件列表
test_files = [
  'main.cpp',
  'test_object.cpp',
  'test_signal_slot.cpp',
  'test_event.cpp',
  'test_application.cpp',
  'test_priority_queue.cpp',
  'exception/test_exception.cpp',
  'exception/test_exception_macros.cpp',
  'exception/test_exception_integration.cpp',
  'log/test_log.cpp',
  'variant/test_variant_basic.cpp',
  'variant/test_variant_containers.cpp',
  'variant/test_variant_standard_containers.cpp',
  'variant/test_typed_variant.cpp',
  'variant/test_variant_equality.cpp',
  'variant/test_variant_comparison.cpp',
  'variant/test_variant_bool_operator.cpp',
  'variant/test_variant_visitor.cpp',
  'dict/test_dict_string_view.cpp',
  'dict/test_dict_construction.cpp',
  'dict/test_dict_operations.cpp',
  'dict/test_dict_conversion.cpp',
  'dict/test_dict_performance.cpp',
  'filesystem/test_filesystem.cpp',
]

# 创建测试可执行文件
test_exe = executable('libmcpp_test',
  test_files,
  include_directories : incdir,
  dependencies : [libmcpp_dep, gtest_dep, boost_dep, gmock_dep],
  cpp_args : ['-Wno-self-assign-overloaded', '-Wno-unused-lambda-capture', '-Wno-self-move'],
  install : false
)

# 注册测试
test('libmcpp unit tests', test_exe)
