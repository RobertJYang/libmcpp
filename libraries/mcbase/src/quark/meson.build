# quark 模块源文件与编译参数
quark_storage = get_option('quark_storage')

quark_src_files = files(
    'quark.cpp',
    'quark_provider.cpp',
)

if quark_storage == 'shared_memory'
    quark_src_files += files('shm_quark_provider.cpp')
else
    quark_src_files += files('process_quark_provider.cpp')
endif

quark_cpp_args = [
    '-DMC_QUARK_INITIAL_BUCKETS=' + get_option('quark_initial_buckets').to_string() + 'U',
    '-DMC_QUARK_MAX_COUNT=' + get_option('quark_max_count').to_string() + 'U',
    '-DMC_QUARK_MAX_LITERAL_LENGTH=' + get_option('quark_max_literal_length').to_string() + 'U',
]

if quark_storage == 'shared_memory'
    quark_cpp_args += '-DMC_QUARK_STORAGE_SHARED_MEMORY=1'
endif
