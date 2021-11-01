@echo off

if not exist build\clang-debug (
    call clang-debug-init.bat
)

pushd build\clang-debug
    cmake --build . --target repl -j24 || (
        copy compile_commands.json ..\..\compile_commands.json
        exit /B 1
    )
    copy compile_commands.json ..\..\compile_commands.json
popd