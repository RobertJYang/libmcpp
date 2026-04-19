expr_src_files = files(
  'builtin/conversion.cpp',
  'builtin/math.cpp',
  'builtin/string.cpp',

  'function/call.cpp',
  'function/collection.cpp',
  'function/parser.cpp',
  'property/processor_registry.cpp',
  'property/processors/function_call_processor.cpp',
  'property/processors/ref_object_processor.cpp',
  'property/processors/ref_property_processor.cpp',
  'property/processors/sync_property_processor.cpp',

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

base_inc_dirs = [incdir, srcdir]

mcexpr_lib = library(
  'mcexpr',
  expr_src_files,
  include_directories: base_inc_dirs,
  dependencies: [dep_arr, mcengine_dep],
  install: true,
  build_by_default: true,
)
mcexpr_dep = declare_dependency(
  include_directories: incdir,
  link_with: mcexpr_lib,
  dependencies: [dep_arr, mcengine_dep],
)