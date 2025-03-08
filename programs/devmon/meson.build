# devmon应用程序的meson构建文件

devmon_sources = [
    'main.cpp',
]

devmon_exe = executable('devmon',
    include_directories : incdir,
    sources: devmon_sources,
    dependencies: [libmcpp_dep, boost_dep, boost_program_options_dep],
    install: true,
)
