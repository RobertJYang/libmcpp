mcdbus_src_files = files(
  'dispatch/pending_call.cpp',
  'dispatch/timeout.cpp',
  'dispatch/watch.cpp',
  'bus_mode_impl.cpp',
  'connection.cpp',
  'connection_impl.cpp',
  'error.cpp',
  'match.cpp',
  'message.cpp',
  'signal.cpp',
  'validator.cpp',
  'sd_bus.cpp',
  'dynamic_object.cpp',
  # gvariant_convert 仅依赖 dbus + glib，是 dbus<->variant 的通用工具，
  # 与 shm 数据结构无关，常驻编译以服务顶层 libmcpp::variant_c_api 等模块。
  'shm/gvariant_convert.cpp',
)

# 旧 dbus/shm 兼容机制（harbor + shm_tree）：仅在 use_old_shm=true 时编译进 libmcdbus。
# 头文件保留在 include/mc/dbus/shm/ 下，但仅当 MCDBUS_USE_OLD_SHM=1 时业务/内部代码才会 include 到。
if use_old_shm
  mcdbus_src_files += files(
    'shm/harbor.cpp',
    'shm/local_msg.cpp',
    'shm/serialize.cpp',
    'shm/shm_tree.cpp',
  )
endif

mcdbus_inc_dirs = [incdir, srcdir]

mcdbus_lib = library(
  'mcdbus',
  mcdbus_src_files,
  include_directories: mcdbus_inc_dirs,
  dependencies: dep_arr,
  install: true,
  build_by_default: true,
)

# MCDBUS_USE_OLD_SHM 同时控制：
#   1) 公共头（match.h / dynamic_object.h）::shm 类型的来源（真实 dbus/shm_tree 头 vs mock_shm.h）；
#   2) bus_mode_impl 等 ABI 中的可选成员（m_shm_tree 等）是否存在；
#   3) 业务侧 #if 选择走旧 shm 路径还是 fallback dbus 原生实现。
# 由于会影响 ABI，必须通过 mcdbus_dep 透传到所有消费方（mcapp / 业务库），
# 保证 mcdbus 自身和下游编译时拿到完全一致的宏定义。
mcdbus_use_old_shm_arg  = use_old_shm ? '-DMCDBUS_USE_OLD_SHM=1' : '-DMCDBUS_USE_OLD_SHM=0'
mcdbus_dep_compile_args = [mcdbus_use_old_shm_arg]

mcdbus_dep = declare_dependency(
  include_directories: incdir,
  link_with: mcdbus_lib,
  dependencies: dep_arr,
  compile_args: mcdbus_dep_compile_args,
)

mcdbus_internal_dep = declare_dependency(
  include_directories: mcdbus_inc_dirs,
  link_with: mcdbus_lib,
  dependencies: dep_arr,
  compile_args: mcdbus_dep_compile_args,
)
