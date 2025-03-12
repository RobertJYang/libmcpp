# 示例插件构建配置

# 源文件列表
plugin_src_files = [
  'example_plugin.cpp',
]

# 创建插件动态库
example_plugin = shared_library('example',
  plugin_src_files,
  include_directories : [incdir, boost_incdir],
  dependencies : [libmcpp_dep, boost_program_options_dep],
  install : true,
  install_dir : get_option('libdir') / 'libmcpp/plugins'
)

# 创建示例应用程序可执行文件
example_app = executable('example_app',
  'main.cpp',
  include_directories : [incdir, boost_incdir],
  dependencies : [libmcpp_dep, boost_program_options_dep],
  install : true
)

# 创建插件目录
plugin_dir = meson.current_build_dir() / 'plugins'
run_command('mkdir', '-p', plugin_dir, check: false)

# 复制插件到插件目录（用于本地测试）
custom_target('copy_plugin',
  output : 'copy_plugin',
  command : ['cp', example_plugin.full_path(), plugin_dir / 'libexample.so'],
  depends : example_plugin,
  build_by_default : true
)