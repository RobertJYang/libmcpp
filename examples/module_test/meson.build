modules_dir = meson.current_build_dir() / 'modules'

subdir('devices')
subdir('network')
subdir('protocol')
subdir('test/database')

# 构建 include_directories 列表（使用根目录定义的 stubdir_global）
example_inc_dirs = [incdir, include_directories('.')]
if not enable_conan_compile
  example_inc_dirs += [stubdir_global]
endif

executable(
    'module_test_example',
    ['main.cpp'],
    include_directories: example_inc_dirs,
    dependencies: [boost_dep, boost_program_options_dep, dbus_dep, glib_dep, libmcpp_dep],
    install: false,
)