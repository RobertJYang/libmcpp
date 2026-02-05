# 创建插件目录
devices_dir = modules_dir / 'mc'
run_command('mkdir', '-p', devices_dir, check: false)

# 构建 include_directories 列表（使用根目录定义的 stubdir_global）
example_inc_dirs = [incdir]
if not enable_conan_compile
  example_inc_dirs += [stubdir_global]
endif

# 构建示例插件
example_moudle = shared_library(
    'devices',
    'module.cpp',
    include_directories: example_inc_dirs,
    dependencies: dep_arr,
    install: true,
    install_dir: get_option('libdir') / 'libmcpp/modules/mc',
    name_prefix: 'lib',
    name_suffix: 'so',
)

# 复制插件到插件目录（用于本地测试）
custom_target(
    'copy_devices_module',
    output: 'copy_devices_module',
    command: ['cp', example_moudle.full_path(), devices_dir / 'devices.so'],
    depends: example_moudle,
    build_by_default: true,
)