if [ ! -d "vcpkg_installed" ]; then
    echo "installing vcpkg deps"
    vcpkg install  > /dev/null 2>&1
fi

cmake_output=$(cmake --preset=vcpkg -DDEBUG=ON -DCMAKE_BUILD_TYPE=Debug 2>&1)
if [ $? -ne 0 ]; then
    printf "%s\n" "$cmake_output"
    exit 1
fi

cmake_build_output=$(cmake --build build)
if [ $? -ne 0 ]; then
    printf "%s\n" "$cmake_build_output"
    exit 1
fi