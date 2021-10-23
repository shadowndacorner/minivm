file(
    GLOB_RECURSE VM_SOURCES
    "src/*.cpp"
)

file(
    GLOB_RECURSE VM_HEADERS
    "include/*.hpp"
    "include/*.h"
)

add_library(minivm STATIC ${VM_SOURCES})
target_include_directories(minivm PUBLIC "./include")