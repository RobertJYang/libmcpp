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

executable(
  'interprocess_example',
  interprocess_example_sources,
  include_directories: [incdir, interprocess_example_incdir],
  dependencies: [libmcpp_dep, boost_dep, boost_program_options_dep],
  install: false,
)