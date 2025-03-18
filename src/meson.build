# 源文件列表
src_files = [
  'object.cpp',
  'exception.cpp',
  'filesystem.cpp',
  'json.cpp',
  'reflect.cpp',
  'time.cpp',
  'variant.cpp',
  'dict.cpp',
  'mutable_dict.cpp',
  'typed_variant.cpp',
  'string.cpp',
  'singleton.cpp',
  'core/application.cpp',
  'core/default_supervisor.cpp',
  'core/dependency_sorter.cpp',
  'core/config_manager.cpp',
  'core/config_validator.cpp',
  'core/plugin_manager.cpp',
  'core/service_manager.cpp',
  'core/supervisor_manager.cpp',
  'log/log_message.cpp',
  'log/logger.cpp',
  'log/log_manager.cpp',
  'log/appender_factory.cpp',
  'log/appenders/console_appender.cpp',
  # 'log/appenders/file_appender.cpp',
]

# 创建共享库
libmcpp_lib = shared_library(
  'libmcpp',
  src_files,
  include_directories: [incdir],
  dependencies: [boost_dep, boost_program_options_dep],
  link_with: [],
  install: true,
)

# 导出库依赖
libmcpp_dep = declare_dependency(include_directories: incdir, link_with: libmcpp_lib)