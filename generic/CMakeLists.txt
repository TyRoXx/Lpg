file(GLOB sources "*.c" "*.h")
add_library(generic ${sources})
if(LPG_CLANG_FORMAT_AUTO)
	add_dependencies(generic clang-format)
endif()

if(NOT MSVC)
	target_link_libraries(generic m)
endif()