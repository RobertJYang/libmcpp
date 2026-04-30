mcengine_test_support_files = files()

mcengine_test_main = configure_file(
  input: files('main.cpp.in'),
  output: 'main.cpp',
  copy: true,
)

mcengine_test_cpp_args = []
mcengine_test_link_args = []
if enable_coverage
  mcengine_test_cpp_args += coverage_args
  mcengine_test_link_args += coverage_link_args
endif

mcengine_test_exe = executable(
  'mcengine_test',
  mcengine_test_main,
  mcengine_test_support_files,
  link_whole: [
    mcengine_test_lib,
    mcengine_lib,
  ],
  include_directories: [incdir, srcdir, mcengine_test_incdir],
  dependencies: [
    gtest_dep,
    boost_dep,
    mcbase_test_utilities_dep,
    mcengine_dep,
  ],
  cpp_args: mcengine_test_cpp_args,
  link_args: mcengine_test_link_args,
  install: false,
  build_by_default: false,
)

mcengine_test_env = environment()
mcengine_test_env.set('MC_BUILD_ROOT', meson.project_build_root())

test(
  'mcengine unit tests',
  mcengine_test_exe,
  env: mcengine_test_env,
  is_parallel: false,
  timeout: 300,
)