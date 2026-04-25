modules_dir = meson.current_build_dir() / 'modules'

subdir('devices')
subdir('network')
subdir('protocol')
subdir('test/database')

example_inc_dirs = [incdir, include_directories('.')]

executable(
    'module_test_example',
    ['main.cpp'],
    include_directories: example_inc_dirs,
    dependencies: [boost_dep, dbus_dep, glib_dep, libmcpp_dep],
    install: false,
)