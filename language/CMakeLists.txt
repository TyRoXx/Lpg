include_directories(../generic)
file(GLOB sources "*.c" "*.h")
add_library(language ${sources})
target_link_libraries(language generic)
