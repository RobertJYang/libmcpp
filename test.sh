#! /bin/bash

CONAN_BUILD_DIR=$(ls -d $HOME/.conan/data/libmcpp/0.1.0/*.dev/dev/build/*/ | head -n 1)
CWD=$(pwd)
BUILD_TYPE=$1
if [ -z "$BUILD_TYPE" ]; then
    BUILD_TYPE="debug"
fi

if [ ! -d $CONAN_BUILD_DIR ] || [ ! -f "$CONAN_BUILD_DIR/libsomp.pc" ]; then
    tool="bingo"
    if ! which bingo > /dev/null 2>&1; then
        if which bmcgo > /dev/null 2>&1; then
            tool="bmcgo"
        else
            echo "bingo不存在"
            exit 1
        fi
    fi
    echo "CONAN_BUILD_DIR不存在，执行$tool test"
    $tool test
    exit 0
fi

export PKG_CONFIG_PATH=$CONAN_BUILD_DIR
export LD_LIBRARY_PATH="$CWD/temp/lib:$CWD/temp/usr/lib64:$LD_LIBRARY_PATH"
meson setup -Denable_shared_memory=true builddir --buildtype=$BUILD_TYPE --reconfigure

cd builddir
set +e
meson test -v
ret=$?
set -e

cd $CWD
exit $ret