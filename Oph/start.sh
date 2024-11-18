if [ ! -d "../bvex-link" ]; then
    echo "updating submodules"
    git submodule update --init --recursive
fi

if [ ! -d "vcpkg_installed" ]; then
    echo "installing vcpkg deps"
    vcpkg install
fi

cmake --preset=vcpkg "$@"  # pass any additional arguments to cmake
cmake --build build
./build/main bcp_Oph.config