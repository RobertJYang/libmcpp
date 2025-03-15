core_sources = [
    'application.cpp',
    'config_manager.cpp',
    'default_supervisor.cpp',
    'module_manager.cpp',
    'service.cpp',
    'service_manager.cpp',
    'supervisor.cpp',
    'supervisor_manager.cpp',
]

core_lib = static_library('mc_core',
    core_sources,
    include_directories : incdir,
    dependencies : [boost_dep],
    install : true,
    install_dir : get_option('libdir')
) 