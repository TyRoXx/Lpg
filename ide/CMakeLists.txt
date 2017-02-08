include_directories(../lib)
file(GLOB sources "*.c" "*.h")
add_executable(ide ${sources})
target_link_libraries(ide lib)
