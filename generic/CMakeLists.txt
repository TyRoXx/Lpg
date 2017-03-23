file(GLOB sources "*.c" "*.h")
add_library(generic ${sources})
add_dependencies(generic clang-format)
