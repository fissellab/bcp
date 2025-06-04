if [ ! -d "vcpkg_installed" ]; then
    echo "installing vcpkg deps"
    vcpkg install  > /dev/null 2>&1
fi

cmake --preset=vcpkg -DDEBUG=ON  > /dev/null 2>&1
if [ $? -ne 0 ]; then
    cmake --preset=vcpkg -DDEBUG=ON
    exit 1
fi

cmake --build build
if [ $? -ne 0 ]; then
    exit 1
fi

echo "RUNNING MAIN"
if [ "$#" -ne 0 ]; then
    ./build/main "$@"
else
    ./build/main 3000 3001 localhost 3002
fi
