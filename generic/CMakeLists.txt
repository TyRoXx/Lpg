file(GLOB sources "*.c" "*.h")
add_library(generic ${sources})
target_compile_options(generic PUBLIC ${LPG_COMPILE_FLAGS})
if(LPG_CLANG_FORMAT_AUTO)
	add_dependencies(generic clang-format)
endif()

if(NOT MSVC)
	target_link_libraries(generic m)
endif()

find_package(Threads)
target_link_libraries(generic ${CMAKE_THREAD_LIBS_INIT})
