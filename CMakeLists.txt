cmake_minimum_required(VERSION 2.8)
project(Lpg C)

enable_testing()

if(MSVC)
	option(LPG_WITH_VLD "use Visual Leak Detector" OFF)
endif()
if(LPG_WITH_VLD)
	add_definitions(-DLPG_WITH_VLD=1)
else()
	add_definitions(-DLPG_WITH_VLD=0)
endif()

if(UNIX)
	option(LPG_WITH_GCOV "compile with gcov support" OFF)
    if(LPG_WITH_GCOV)
        set(LPG_COMPILE_FLAGS -coverage)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -coverage")
        add_custom_target(testcoverage
            COMMAND $<TARGET_FILE:tests> &&
                lcov --capture --directory ${CMAKE_CURRENT_BINARY_DIR} --output-file coverage.info &&
                genhtml coverage.info --output-directory coverage-html &&
                firefox coverage-html/index.html
            WORKING_DIRECTORY ${CMAKE_CURRENT_BUILD_DIR})
        add_dependencies(testcoverage tests)
    endif()

    option(LPG_WITH_UB_SANITIZER "compile with clang undefined behaviour sanitizer" OFF)
    if(LPG_WITH_UB_SANITIZER)
        add_definitions(-fsanitize=undefined -fno-omit-frame-pointer)
    endif()

    option(LPG_WITH_MEMORY_SANITIZER "compile with clang memory sanitizer" OFF)
    if(LPG_WITH_MEMORY_SANITIZER)
        add_definitions(-fsanitize=memory -fno-omit-frame-pointer -fPIE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=memory -pie")
    endif()

    option(LPG_WITH_ADDRESS_SANITIZER "compile with clang address sanitizer" OFF)
    if(LPG_WITH_ADDRESS_SANITIZER)
        add_definitions(-fsanitize=address -fno-omit-frame-pointer -shared-libasan)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -shared-libasan")
    endif()

    option(LPG_WITH_THREAD_SANITIZER "compile with clang thread sanitizer" OFF)
    if(LPG_WITH_THREAD_SANITIZER)
        add_definitions(-fsanitize=thread)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
    endif()
endif()

file(GLOB_RECURSE formatted
	c_backend/**.c c_backend/**.h
	cli/**.c cli/**.h
	cli_impl/**.c cli_impl/**.h
	generic/**.c generic/**.h
    ecmascript_backend/**.c ecmascript_backend/**.h
	optimization/**.c optimization/**.h
	semantics/**.c semantics/**.h
	syntax/**.c syntax/**.h
	tests/**.c tests/**.h
	tokenization/**.c tokenization/**.h
)

option(LPG_WITH_CLANG_FORMAT "use clang-format to format the source code" ON)
if(LPG_WITH_CLANG_FORMAT)
	find_program(LPG_CLANG_FORMAT NAMES clang-format-3.7 clang-format PATHS "C:/Program Files/LLVM/bin")
	if(LPG_CLANG_FORMAT)
		add_custom_target(clang-format COMMAND ${LPG_CLANG_FORMAT} -i ${formatted} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
		option(LPG_WITH_CLANG_FORMAT_AUTO "format the code automatically before the build" OFF)
	endif()
endif()

set(LPG_DUKTAPE_VERSION 2.2.1)
set(LPG_DOWNLOAD_PATH ${CMAKE_BINARY_DIR}/duktape-${LPG_DUKTAPE_VERSION}.tar.xz)
set(LPG_DUKTAPE ${CMAKE_BINARY_DIR}/duktape-${LPG_DUKTAPE_VERSION})

if(NOT EXISTS ${LPG_DUKTAPE})
    file(DOWNLOAD "https://github.com/svaarala/duktape/releases/download/v${LPG_DUKTAPE_VERSION}/duktape-${LPG_DUKTAPE_VERSION}.tar.xz" ${LPG_DOWNLOAD_PATH})
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf ${LPG_DOWNLOAD_PATH}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    file(REMOVE ${LPG_DOWNLOAD_PATH})
endif()

file(GLOB duktape_sources ${LPG_DUKTAPE}/src/*.h ${LPG_DUKTAPE}/src/*.c)
add_library(duktape STATIC ${duktape_sources})
target_include_directories(duktape PUBLIC ${LPG_DUKTAPE}/src)
if(NOT MSVC)
	target_link_libraries(duktape m)
endif()

if(MSVC)
	target_compile_options(duktape PUBLIC /DDUK_USE_DATE_NOW_WINDOWS)
	if(MSVC_VERSION LESS 1911)
		add_definitions(/D_WIN32_WINNT_WINTHRESHOLD=0)
	endif()
	#avoid bogus warnings in Windows headers
	add_definitions(/DNTDDI_WIN7SP1=0)
	add_definitions(/DWINAPI_PARTITION_SYSTEM=0)
	add_definitions(/D_WIN32_WINNT_WIN10_TH2=0)
	add_definitions(/D_WIN32_WINNT_WIN10_RS1=0)
	add_definitions(/D_WIN32_WINNT_WIN10_RS2=0)
	add_definitions(/D_WIN32_WINNT_WIN10_RS3=0)
	add_definitions(/D_WIN32_WINNT_WIN10_RS4=0)
	add_definitions(/Qspectre)
	set(LPG_COMPILE_FLAGS ${LPG_COMPILE_FLAGS} /Wall /wd4820 /wd4255 /wd4201 /wd4204 /wd4710 /wd4711 /wd4221 /wd5045 /WX)
else()
    set(LPG_COMPILE_FLAGS ${LPG_COMPILE_FLAGS}
        -std=gnu11 -Wall -Wextra -Wconversion -Werror -Wunused -Wundef -Wvla -Wshadow -Wpedantic
        -Wdouble-promotion -Wformat -Wswitch-enum -Wuninitialized -Wstrict-overflow=5)
endif()

add_subdirectory(generic)
add_subdirectory(tokenization)
add_subdirectory(syntax)
add_subdirectory(semantics)
add_subdirectory(optimization)
add_subdirectory(cli_impl)
add_subdirectory(c_backend)
add_subdirectory(ecmascript_backend)
add_subdirectory(tests)
add_subdirectory(cli)
