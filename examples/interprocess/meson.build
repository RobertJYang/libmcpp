interprocess_example_sources = [
  'main.cpp',
  'common.cpp',
  'test_shared_memory.cpp',
  'test_shared_memory_long_running.cpp',
  'test_shared_mutex.cpp', 
  'tests_impl.cpp',
]

executable('interprocess_example',
  interprocess_example_sources,
  include_directories : incdir,
  dependencies : [libmcpp_dep, boost_dep, boost_program_options_dep],
  install : false
)

# subdir('shared_tree_db') 