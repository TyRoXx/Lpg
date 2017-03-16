include_directories(../lib)
include_directories(../generic)
add_executable(tests main.c)
target_link_libraries(tests generic)
