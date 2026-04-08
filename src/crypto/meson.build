crypto_src_files = [
    'crypto/crc8.cpp',
    'crypto/crc32.cpp',
]

if get_option('enable_conan_compile')
    crypto_src_files += ['crypto/ecdsa_verify_sha256.cpp']
else
    crypto_src_files += ['../stub/ecdsa_verify_sha256.cpp']
endif