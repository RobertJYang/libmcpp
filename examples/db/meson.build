executable(
  'database_example',
  ['main.cpp'],
  include_directories: incdir,
  dependencies: [boost_dep, boost_program_options_dep, libmcpp_dep],
  install: true,
  install_dir: get_option('bindir'),
)

# 在构建时复制配置文件到构建目录
configure_file(
  input: 'config.json',
  output: 'config.json',
  copy: true,
)