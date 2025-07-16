expr_src_files = files(
  'builtin/conversion.cpp',
  'builtin/math.cpp',
  'builtin/string.cpp',

  'function/call.cpp',
  'function/collection.cpp',
  'function/parser.cpp',

  'node/binary_op_node.cpp',
  'node/conditional_node.cpp',
  'node/function_call_node.cpp',
  'node/literal_node.cpp',
  'node/object_method_call_node.cpp',
  'node/property_access_node.cpp',
  'node/template_string_node.cpp',
  'node/unary_op_node.cpp',
  'node/variable_node.cpp',

  'builtin.cpp',
  'context.cpp',
  'engine.cpp',
  'function.cpp',
  'lexer.cpp',
  'parser.cpp',
)

libmcpp_expr_lib = library(
  'mcpp_expr',
  expr_src_files,
  include_directories: [incdir, srcdir],
  dependencies: [dep_arr, libmcpp_engine_dep],
  install: true,
  install_dir: install_dir,
  build_by_default: true,
)
libmcpp_expr_dep = declare_dependency(
  include_directories: incdir,
  link_with: libmcpp_expr_lib,
  dependencies: [dep_arr, libmcpp_engine_dep],
)