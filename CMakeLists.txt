cmake_minimum_required(VERSION 2.8)
project(Lpg C)

file(GLOB_RECURSE formatted "*.c" "*.h")

find_program(LPG_CLANG_FORMAT NAMES clang-format clang-format-3.7 PATHS "C:/Program Files/LLVM/bin")
add_custom_target(clang-format COMMAND ${LPG_CLANG_FORMAT} -i ${formatted} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

if(MSVC)
	add_definitions(/WX /Wall /wd4820 /wd4255 /wd4201 /wd4204 /wd4710 /wd4711 /wd4221 /D_WIN32_WINNT_WINTHRESHOLD=0 /DNTDDI_WIN7SP1=0 /DWINAPI_PARTITION_SYSTEM=0)
else()
	add_definitions(-std=gnu99 -Wall -Wextra -Wconversion -Werror -Wunused -Wundef -Wvla)
endif()

add_subdirectory(generic)
add_subdirectory(language)
add_subdirectory(ide)
add_subdirectory(tests)
