# 基础示例构建配置

# 创建示例应用程序可执行文件
executable(
  'example_app',
  'main.cpp',
  include_directories: incdir,
  dependencies: [boost_dep],
  link_with: [libmcpp_lib],
  install: true,
  install_dir: get_option('bindir'),
)