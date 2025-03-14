if [ ! -d "../bvex-link" ]; then
    echo "updating submodules"
    git submodule update --init --recursive
fi

if [ ! -d "vcpkg_installed" ]; then
    echo "installing vcpkg deps"
    vcpkg install
fi

if [ ! -d "build" ]; then
    echo "generating build files"
    cmake --preset=default
fi

cmake --build build
./build/main bcp_Oph.config