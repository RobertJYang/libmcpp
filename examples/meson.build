# 示例目录构建配置

# 创建示例应用程序可执行文件
executable('example_app',
  'main.cpp',
  include_directories : incdir,
  dependencies : [boost_dep],
  link_with : [libmcpp_lib],
  install : true,
  install_dir : get_option('bindir')
)

# 创建模块目录
modules_dir = meson.current_build_dir() / 'modules'
run_command('mkdir', '-p', modules_dir, check: false)

# 创建配置目录
config_dir = meson.current_build_dir()
run_command('mkdir', '-p', config_dir, check: false)

# 构建示例模块 - 暂时禁用
# example_lib = shared_library('example_module',
#   'dynamic_module/example_module.cpp',
#   include_directories : incdir,
#   dependencies : [boost_dep, boost_program_options_dep],
#   link_with : [libmcpp_lib],
#   install : true,
#   install_dir : get_option('libdir') / 'libmcpp/modules'
# )

# 复制模块到模块目录（用于本地测试）- 暂时禁用
# custom_target('copy_example_module',
#   output : 'copy_example_module',
#   command : ['cp', example_lib.full_path(), modules_dir / 'libexample.so'],
#   depends : example_lib,
#   build_by_default : true
# )

# 复制配置文件到配置目录（用于本地测试）
custom_target('copy_example_config',
  output : 'copy_example_config',
  command : ['cp', meson.current_source_dir() / 'config.ini', config_dir / 'config.ini'],
  build_by_default : true
)

# 安装配置文件
install_data('config.ini',
  install_dir : get_option('sysconfdir') / 'libmcpp',
  preserve_path : false
)

# 插件示例
executable('plugin_example',
  'plugin_example.cpp',
  include_directories : incdir,
  dependencies : [boost_dep, boost_program_options_dep],
  link_with : [libmcpp_lib],
  install : true,
  install_dir : get_option('bindir')
)

# 创建插件目录
plugins_dir = meson.current_build_dir() / 'plugins'
run_command('mkdir', '-p', plugins_dir, check: false)

# 构建示例插件
example_plugin = shared_library('example_plugin',
  'example_plugin.cpp',
  include_directories : incdir,
  dependencies : [boost_dep, boost_program_options_dep],
  link_with : [libmcpp_lib],
  install : true,
  install_dir : get_option('libdir') / 'libmcpp/plugins'
)

# 复制插件到插件目录（用于本地测试）
custom_target('copy_example_plugin',
  output : 'copy_example_plugin',
  command : ['cp', example_plugin.full_path(), plugins_dir / 'libexample_plugin.so'],
  depends : example_plugin,
  build_by_default : true
)

# 复制配置文件
custom_target('copy_plugin_config',
  output : 'copy_plugin_config',
  command : ['cp', meson.current_source_dir() / 'plugin_config.ini', config_dir / 'plugin_config.ini'],
  build_by_default : true
)
