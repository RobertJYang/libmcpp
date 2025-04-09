if cross_compile == false
object_framework_example = executable(
    'object_framework_example',
    'main.cpp',
    include_directories: inc,
    dependencies: [
        boost_dep,
        thread_dep
    ],
    link_with: [mcpp_lib],
    install: false
) 
else
object_framework_example = executable(
    'object_framework_example',
    'main.cpp',
    include_directories: inc,
    dependencies: [
        thread_dep
    ],
    link_with: [mcpp_lib],
    install: false
) 
endif