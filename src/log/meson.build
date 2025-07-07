# meson构建为file_appender.cpp添加特殊的包含路径
enable_conan_compile = get_option('enable_conan_compile')
if enable_conan_compile
  file_appender_inc = include_directories('.')
else
  file_appender_inc = include_directories('../../../tests/log')
endif

log_src_files = [
  'log/log_message.cpp',
  'log/logger.cpp',
  'log/log_manager.cpp',
  'log/appender_factory.cpp',
  'log/appenders/console_appender.cpp',
  'log/appenders/file_appender.cpp',
]

# file_appender.cpp需要特殊的包含路径
file_appender_src = 'log/appenders/file_appender.cpp' 