mcexpr_test_files = files(
  'builtin/test_conversion.cpp',
  'builtin/test_math.cpp',
  'builtin/test_string.cpp',
  'function/test_call.cpp',
  'function/test_collection.cpp',
  'function/test_parser.cpp',
  'test_context.cpp',
  'test_expr.cpp',
  'test_lexer.cpp',
  'test_node.cpp',
  'test_object_expr.cpp',
  'test_parser.cpp',
  'test_property.cpp',
  'test_property_processors.cpp',
)

mcexpr_test_main = configure_file(
  input: files('main.cpp.in'),
  output: 'main.cpp',
  copy: true,
)

mcexpr_test_cpp_args = []
mcexpr_test_link_args = []
if enable_coverage
  mcexpr_test_cpp_args += coverage_args
  mcexpr_test_link_args += coverage_link_args
endif

mcexpr_test_exe = executable(
  'mcexpr_test',
  mcexpr_test_main,
  link_whole: [
    mcexpr_test_lib,
    mcexpr_lib,
  ],
  include_directories: [incdir, srcdir, mcexpr_test_incdir],
  dependencies: [
    gtest_dep,
    boost_dep,
    mcbase_test_utilities_dep,
    mcengine_test_utilities_dep,
    mcexpr_dep,
  ],
  cpp_args: mcexpr_test_cpp_args,
  link_args: mcexpr_test_link_args,
  install: false,
  build_by_default: false,
)

mcexpr_test_env = environment()
mcexpr_test_env.set('MC_BUILD_ROOT', meson.project_build_root())

test(
  'mcexpr unit tests',
  mcexpr_test_exe,
  env: mcexpr_test_env,
  is_parallel: false,
  timeout: 300,
)