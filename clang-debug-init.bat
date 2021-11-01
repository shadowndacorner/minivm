@echo off
mkdir build\clang-debug
pushd build\clang-debug
    REM conan install --build=missing -s compiler=clang -s compiler.version=10 -s build_type=Debug -g cmake ../../ || exit 1
    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=true -G "Unix Makefiles" -DCMAKE_CXX_COMPILER=clang -DCMAKE_C_COMPILER=clang ../../
popd