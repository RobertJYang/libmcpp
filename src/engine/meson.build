engine_src_files = files(
  'errors/std_errors.cpp',
  'base.cpp',
  'call_stack.cpp',
  'context.cpp',
  'engine.cpp',
  'interface.cpp',
  'metadata.cpp',
  'object.cpp',
  'path.cpp',
  'path_iterator.cpp',
  'property.cpp',
  'service.cpp',
  'std_interface.cpp',
  'micro_component.cpp',
  'utils.cpp',
  'property/helper.cpp',
  'property/processor.cpp',
  'property/processor_registry.cpp',
  'property/ref_object.cpp',
  'property/processors/ref_object_processor.cpp',
  'property/processors/ref_property_processor.cpp',
  'property/processors/sync_property_processor.cpp',
  'property/processors/function_call_processor.cpp'
)

engine_src_files += dbus_src_files

# 包含目录（相对于 src/engine/meson.build）
incdir = include_directories('../../include')
srcdir = include_directories('..')
# 注意：mcbase_dep 已经包含了 include 目录，不需要单独指定

libmcpp_engine_lib = library(
  'mcpp_engine',
  engine_src_files,
  include_directories: [incdir, srcdir],
  dependencies: [dep_arr, libmcpp_core_dep],
  install: true,
  install_dir: install_dir,
  build_by_default: true,
)
libmcpp_engine_dep = declare_dependency(
  include_directories: [incdir, srcdir],
  link_with: libmcpp_engine_lib,
  dependencies: [dep_arr, libmcpp_core_dep],
)
