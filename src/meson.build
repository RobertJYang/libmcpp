# 源文件列表
src_files = [
  'object.cpp',
  'exception.cpp',
  'core/application.cpp',
  'variant.cpp',
  'dict.cpp',
  'mutable_dict.cpp',
  'typed_variant.cpp',
  'string.cpp',
  'log/log_message.cpp',
  'log/logger.cpp',
  'filesystem.cpp',
]

# 创建共享库
libmcpp_lib = shared_library('libmcpp',
  src_files,
  include_directories : [incdir, boost_incdir],
  dependencies : [boost_dep, boost_program_options_dep],
  link_with : [],
  install : true
)

# 导出库依赖
libmcpp_dep = declare_dependency(
  include_directories : incdir,
  link_with : libmcpp_lib
)