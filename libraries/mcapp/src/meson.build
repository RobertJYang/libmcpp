mcapp_src_files = files(
  'app_proto.cpp',
  'base_app.cpp',
  'application.cpp',
  'dbus_proto.cpp',
  'micro_component.cpp',
  'service.cpp',
  'service_registry.cpp',
)

if use_old_shm
  mcapp_src_files += files('legacy_shm_binding.cpp')
endif

base_inc_dirs = [incdir, srcdir] + libmcpp_incdir

mcapp_lib = library(
  'mcapp',
  mcapp_src_files,
  include_directories: base_inc_dirs,
  dependencies: dep_arr,
  install: true,
  build_by_default: true,
)

# 公共头文件（service.h）按 MCDBUS_USE_OLD_SHM 做 ABI 选择（m_legacy_shm 字段是否存在），
# 该宏由 mcdbus_dep 的 compile_args 透传到 mcapp 自身和下游消费者，无需 mcapp 再重复注入。
mcapp_dep = declare_dependency(
  include_directories: [incdir] + libmcpp_incdir,
  link_with: mcapp_lib,
  dependencies: dep_arr,
)
