database_sources = [
  'database_plugin.cpp',
  'services/memory_manager/service.cpp',
  'services/communication/service.cpp',
  'services/object_tree/service.cpp',
  'services/reference_tracker/service.cpp',
]

database_lib = shared_library('database_plugin',
  database_sources,
  include_directories : [incdir, include_directories('.')],
  dependencies : [libmcpp_dep],
  install : true,
  install_dir : join_paths(get_option('libdir'), 'libmcpp', 'plugins'),
  name_prefix : 'lib',
  name_suffix : 'so'
)

if cross_compile == false
database_plugin_dep = declare_dependency(
  link_with : database_lib,
  include_directories : [incdir, include_directories('.')],
  dependencies : [boost_dep, boost_program_options_dep, libmcpp_dep],
) 
else
database_plugin_dep = declare_dependency(
  link_with : database_lib,
  include_directories : [incdir, include_directories('.')],
  dependencies : [boost_dep],
) 
endif