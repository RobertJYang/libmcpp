modules_dir = meson.current_build_dir() / 'modules'

subdir('devices')
subdir('network')
subdir('protocol')
subdir('test/database')

executable(
    'module_test_example',
    ['main.cpp'],
    include_directories: [incdir, include_directories('.')],
    dependencies: [boost_dep, boost_program_options_dep, dbus_dep, glib_dep, libmcpp_dep],
    install: false,
)