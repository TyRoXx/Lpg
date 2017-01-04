cmake_minimum_required(VERSION 3.0)
project(Lpg)

file(GLOB_RECURSE formatted "*.c" "*.h")

find_program(LPG_CLANG_FORMAT NAMES clang-format clang-format-3.7 PATHS "C:/Program Files/LLVM/bin")
add_custom_target(clang-format COMMAND ${LPG_CLANG_FORMAT} -i ${formatted} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

if(MSVC)
	add_definitions(/Wall /wd4820 /wd4255 /wd4201 /wd4204 /wd4710 /wd4711 /wd4221 /WX /D_WIN32_WINNT_WINTHRESHOLD=0 /DNTDDI_WIN7SP1=0)
endif()

add_subdirectory(ide)
add_subdirectory(tests)
