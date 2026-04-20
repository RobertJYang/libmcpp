test_quark_files = [
    'quark/test_quark_basic.cpp',
    'quark/test_quark_macro.cpp',
    'quark/test_quark_compat.cpp',
    'quark/test_quark_concurrency.cpp',
]

if get_option('quark_storage') == 'shared_memory'
    test_quark_files += ['quark/test_quark_shm.cpp']
else
    test_quark_files += ['quark/test_quark_rehash.cpp']
endif
