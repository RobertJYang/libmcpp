# mcengine 的测试辅助库：仅在 SHM 开启时编译，因为 TestWithEngine 的 SHM
# 实现（SetUp/TearDown/fork_child 等）依赖 shm_runtime。关 SHM 时整个
# static_library 不进编译，不输出符号。

mcengine_test_utilities_inc = include_directories('.')

mcengine_test_utilities_sources = [
  'engine_test_base.cpp',
]

mcengine_test_utilities_lib = static_library(
  'mcengine_test_utilities',
  mcengine_test_utilities_sources,
  include_directories: [incdir],
  dependencies: [
    boost_dep,
    gtest_dep,
    mcengine_dep,
    mcbase_test_utilities_dep,
  ],
  install: true,
  install_dir: get_option('libdir'),
  build_by_default: true,
)

mcengine_test_utilities_dep = declare_dependency(
  link_with: mcengine_test_utilities_lib,
  include_directories: [incdir, mcengine_test_utilities_inc],
  dependencies: [gtest_dep, mcengine_dep, mcbase_test_utilities_dep],
)
