interprocess_example_sources = [
  'main.cpp',
  'common.cpp',
  'multi_process_test_base.cpp',
  'shared_memory/test_shared_memory.cpp',
  'shared_memory/test_shared_memory_long_running.cpp',
  'shared_memory/test_multi_process_shared_memory.cpp',
  'ipc_mutex/test_multi_process_mutex.cpp',
  'ipc_mutex/test_multi_process_shared_mutex.cpp',
  'ipc_mutex/test_mutex.cpp',
  'ipc_mutex/test_shared_mutex.cpp',
]

interprocess_example_incdir = include_directories('.')

# 构建 include_directories 列表（使用根目录定义的 stubdir_global）
example_inc_dirs = [incdir, interprocess_example_incdir]
if not enable_conan_compile
  example_inc_dirs += [stubdir_global]
endif

executable(
  'interprocess_example',
  interprocess_example_sources,
  include_directories: example_inc_dirs,
  dependencies: [libmcpp_dep, boost_dep, boost_program_options_dep],
  install: false,
)