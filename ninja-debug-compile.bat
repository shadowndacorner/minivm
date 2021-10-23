@echo off

if not exist build\ninja-debug (
    call ninja-debug-init.bat
)

pushd build\ninja-debug
    cmake --build . --target repl  || (
        copy compile_commands.json ..\..\compile_commands.json
        exit /B 1
    )
    copy compile_commands.json ..\..\compile_commands.json
popd