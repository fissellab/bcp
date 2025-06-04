if [ ! -d "vcpkg_installed" ]; then
    echo "installing vcpkg deps"
    vcpkg install
fi

cmake --preset=vcpkg -DTEST=ON
cmake --build build
./build/test "$@"