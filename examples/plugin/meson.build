# 插件示例构建配置

dep_arr = [boost_dep, boost_program_options_dep, dbus_dep, glib_dep, libmcpp_dep]

# 创建插件示例可执行文件
executable(
    'example_plugin',
    'main.cpp',
    include_directories: incdir,
    dependencies: dep_arr,
    install: true,
    install_dir: get_option('bindir'),
)

# 创建插件目录
plugins_dir = meson.current_build_dir() / 'plugins'
run_command('mkdir', '-p', plugins_dir, check: false)

# 构建示例插件
example_plugin = shared_library(
    'example_plugin',
    'example_plugin.cpp',
    include_directories: incdir,
    dependencies: dep_arr,
    install: true,
    install_dir: get_option('libdir') / 'libmcpp/plugins',
    name_prefix: 'lib', # 强制使用lib前缀
    name_suffix: 'so' # 强制使用.so后缀,,
)

# 复制插件到插件目录（用于本地测试）
custom_target(
    'copy_example_plugin',
    output: 'copy_example_plugin',
    command: ['cp', example_plugin.full_path(), plugins_dir / 'libexample_plugin.so'],
    depends: example_plugin,
    build_by_default: true,
)

# 复制配置文件
custom_target(
    'copy_plugin_config',
    output: 'copy_plugin_config',
    command: [
        'cp',
        meson.current_source_dir() / 'config.json',
        meson.current_build_dir() / 'config.json',
    ],
    build_by_default: true,
)