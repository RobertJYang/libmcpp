mcengine_test_files = files(
  'test_error_engine.cpp',
  'test_interface.cpp',
  'test_interface_inherit.cpp',
  'test_object.cpp',
  'test_shm_engine_business.cpp',
  'test_shm_engine_lifecycle.cpp',
  'test_shm_object_journal.cpp',
  'test_shm_object_layout.cpp',
  'test_shm_object_ops.cpp',
  'test_shm_service_layout.cpp',
  'test_shm_service_ops.cpp',
  'test_shm_service_takeover.cpp',
  'test_utils.cpp',
  'test_engine.cpp',
  'test_property_concurrent_fix.cpp',
  'test_context.cpp',
  'test_path.cpp',
  'test_service_utils.cpp',
  'test_path_iterator.cpp',
  'test_async_invoke.cpp',
  'test_result.cpp',
)

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
)

mcengine_test_env = environment()
mcengine_test_env.set('MC_BUILD_ROOT', meson.project_build_root())

test(
  'mcengine unit tests',
  mcengine_test_exe,
  env: mcengine_test_env,
  timeout: 60,
)