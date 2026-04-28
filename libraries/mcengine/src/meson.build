engine_src_files = files(
  'errors/std_errors.cpp',
  'abstract_object_factory.cpp',
  'base.cpp',
  'call_stack.cpp',
  'context.cpp',
  'dispatcher.cpp',
  'engine.cpp',
  'engine_codec.cpp',
  'service_proto.cpp',
  'event.cpp',
  'interface.cpp',
  'match/condition_backend.cpp',
  'match/condition_filter.cpp',
  'match/endpoint_sweep.cpp',
  'match/local_table.cpp',
  'match/path_pattern.cpp',
  'match/registry.cpp',
  'match/rule.cpp',
  'match/shared_table.cpp',
  'match/types.cpp',
  'message_codec.cpp',
  'metadata.cpp',
  'object.cpp',
  'shm_binding.cpp',
  'shm_property_sync.cpp',
  'path.cpp',
  'path_template.cpp',
  'path_iterator.cpp',
  'property.cpp',
  'proxy.cpp',
  'service.cpp',
  'utils.cpp',
  'property/helper.cpp',
  'property/processor.cpp',
  'property/relation.cpp',
  'property/ref_object.cpp',
  'property/instantiation.cpp',
  'std_interface.cpp'
)

if use_shm
  engine_src_files += files(
    'shm_object.cpp',
    'shm_object_ops.cpp',
    'shm_runtime_provider.cpp',
    'shm_service.cpp',
    'shm_service_ops.cpp',
    'endpoint_service.cpp',
  )
endif

base_inc_dirs = [incdir, srcdir]

mcengine_lib = library(
  'mcengine',
  engine_src_files,
  include_directories: base_inc_dirs,
  dependencies: dep_arr,
  install: true,
  build_by_default: true,
)
mcengine_use_shm_arg = use_shm ? '-DMCENGINE_USE_SHM=1' : '-DMCENGINE_USE_SHM=0'

mcengine_dep = declare_dependency(
  include_directories: incdir,
  link_with: mcengine_lib,
  dependencies: dep_arr,
  compile_args: [mcengine_use_shm_arg],
)
