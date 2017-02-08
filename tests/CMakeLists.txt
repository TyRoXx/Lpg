include_directories(../lib)
add_executable(tests main.c)
target_link_libraries(tests lib)
